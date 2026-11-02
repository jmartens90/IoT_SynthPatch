#include "audio_in.h"

#include <Preferences.h>
#include <driver/i2s.h>

#include "pins.h"

namespace audio {

static constexpr i2s_port_t PORT = I2S_NUM_0;
// PCM1808 full scale is 0.6 * VCC = 3.0 Vpp at VINx with VCC = 5 V -> 1.5 V peak.
static constexpr float ADC_FS_PEAK_V = 1.5f;

static TaskHandle_t task = nullptr;
static volatile bool run = false;
static SemaphoreHandle_t mtx;
static Stats st[2];
static int32_t *blockBuf = nullptr;  // interleaved L/R, BLOCK frames
static float *last[2] = {nullptr, nullptr};
static float feGain = 1.0f;

static float estimateFreq(const float *x, int n, float pp) {
  // Zero crossings (rising) with hysteresis, linear interpolation between samples.
  float hyst = pp * 0.1f;
  if (pp < 1e-4f) return 0;
  bool armed = false;
  float first = -1, lastX = -1;
  int count = 0;
  for (int i = 1; i < n; i++) {
    if (x[i] < -hyst) armed = true;
    if (armed && x[i - 1] < 0 && x[i] >= 0) {
      float t = (i - 1) + (-x[i - 1]) / (x[i] - x[i - 1]);
      if (first < 0) first = t;
      else count++;
      lastX = t;
      armed = false;
    }
  }
  if (count < 2) return 0;
  return SAMPLE_RATE * count / (lastX - first);
}

static void analyse(int ch, const int32_t *buf, float *out) {
  double sum = 0;
  for (int i = 0; i < BLOCK; i++) {
    out[i] = (float)buf[2 * i + ch] / 2147483648.0f;  // fraction of full scale
    sum += out[i];
  }
  float mean = sum / BLOCK;
  float mn = 1e9, mx = -1e9;
  double sq = 0;
  for (int i = 0; i < BLOCK; i++) {
    out[i] -= mean;
    mn = min(mn, out[i]);
    mx = max(mx, out[i]);
    sq += (double)out[i] * out[i];
  }
  float rmsFs = sqrt(sq / BLOCK);
  float freq = estimateFreq(out, BLOCK, mx - mn);
  // Convert to volts at the jack (front end is inverting; sign dropped).
  float scale = ADC_FS_PEAK_V / feGain;
  for (int i = 0; i < BLOCK; i++) out[i] *= -scale;

  Stats s;
  s.freqHz = freq;
  s.vpp = (mx - mn) * scale;
  s.vrms = rmsFs * scale;
  s.dbfs = rmsFs > 1e-9f ? 20.0f * log10f(rmsFs * 1.41421356f) : -150.0f;
  s.dcRaw = mean;
  xSemaphoreTake(mtx, portMAX_DELAY);
  s.blocks = st[ch].blocks + 1;
  st[ch] = s;
  memcpy(last[ch], out, sizeof(float) * BLOCK);
  xSemaphoreGive(mtx);
}

static void captureTask(void *) {
  static float work[BLOCK];
  while (run) {
    size_t got = 0, want = BLOCK * 2 * sizeof(int32_t);
    uint8_t *p = (uint8_t *)blockBuf;
    while (got < want && run) {
      size_t n = 0;
      i2s_read(PORT, p + got, want - got, &n, pdMS_TO_TICKS(200));
      got += n;
    }
    if (!run) break;
    analyse(0, blockBuf, work);
    analyse(1, blockBuf, work);
  }
  task = nullptr;
  vTaskDelete(nullptr);
}

bool begin() {
  if (run) return true;
  if (!mtx) mtx = xSemaphoreCreateMutex();
  Preferences p;
  if (p.begin("adc", true)) {
    feGain = p.getFloat("gain", 1.0f);
    p.end();
  }
  if (!blockBuf) {
    blockBuf = (int32_t *)heap_caps_malloc(BLOCK * 2 * sizeof(int32_t), MALLOC_CAP_8BIT);
    last[0] = (float *)ps_malloc(BLOCK * sizeof(float));
    last[1] = (float *)ps_malloc(BLOCK * sizeof(float));
    if (!last[0]) last[0] = (float *)malloc(BLOCK * sizeof(float));
    if (!last[1]) last[1] = (float *)malloc(BLOCK * sizeof(float));
    if (!blockBuf || !last[0] || !last[1]) return false;
    memset(last[0], 0, BLOCK * sizeof(float));
    memset(last[1], 0, BLOCK * sizeof(float));
  }

  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
  cfg.sample_rate = SAMPLE_RATE;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;  // 32-bit slots -> BCK = 64 fs
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;  // PCM1808 FMT = 0 (I2S)
  cfg.intr_alloc_flags = 0;
  cfg.dma_buf_count = 8;
  cfg.dma_buf_len = 256;
  cfg.use_apll = false;
  cfg.fixed_mclk = 0;
  cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;  // SCKI = 12.288 MHz
  if (i2s_driver_install(PORT, &cfg, 0, nullptr) != ESP_OK) return false;

  i2s_pin_config_t pins = {};
  pins.mck_io_num = PIN_I2S_MCLK;
  pins.bck_io_num = PIN_I2S_BCK;
  pins.ws_io_num = PIN_I2S_LRCK;
  pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.data_in_num = PIN_I2S_DIN;
  if (i2s_set_pin(PORT, &pins) != ESP_OK) {
    i2s_driver_uninstall(PORT);
    return false;
  }
  run = true;
  xTaskCreatePinnedToCore(captureTask, "audio", 6144, nullptr, 3, &task, 0);
  return true;
}

void end() {
  if (!run) return;
  run = false;
  while (task) delay(5);
  i2s_driver_uninstall(PORT);
}

bool running() { return run; }

Stats stats(int ch) {
  Stats s{};
  if (!mtx) return s;
  xSemaphoreTake(mtx, portMAX_DELAY);
  s = st[ch & 1];
  xSemaphoreGive(mtx);
  return s;
}

int snapshot(int ch, float *dst, int maxFrames) {
  if (!mtx || !last[0]) return 0;
  int n = min(maxFrames, BLOCK);
  xSemaphoreTake(mtx, portMAX_DELAY);
  memcpy(dst, last[ch & 1], n * sizeof(float));
  xSemaphoreGive(mtx);
  return n;
}

void setFrontEndGain(float g) {
  if (g <= 0) return;
  feGain = g;
  Preferences p;
  if (p.begin("adc", false)) {
    p.putFloat("gain", g);
    p.end();
  }
}

float frontEndGain() { return feGain; }

}  // namespace audio
