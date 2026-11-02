#include "cvbus.h"

#include <Preferences.h>
#include <SPI.h>

#include "pins.h"

namespace cvbus {

static SPIClass bus(HSPI);
static SemaphoreHandle_t mtx;
static uint32_t clockHz = 8000000;
static uint16_t cvCodes[NUM_CV];
static uint16_t voctCodes[2];
static uint8_t ledMask = 0;
static bool voctBi = false;
static uint32_t nTransfers = 0;

// Two-point calibration: code = a * volts + b. Index 0..7 = CV outs, 8..9 = V/oct A/B.
// Defaults are the nominal transfer functions from the schematic.
struct Cal {
  float a, b;
};
static Cal cal[NUM_CV + 2];

static void nominalCal() {
  for (int i = 0; i < NUM_CV; i++) {
    if (CV_OUTS[i].bipolar) {  // Vout = 2*Vdac - 5,  Vdac = code/4096*5
      cal[i] = {4096.0f / 10.0f, 2048.0f};
    } else {  // Vout = 2*Vdac
      cal[i] = {4096.0f / 10.0f, 0.0f};
    }
  }
  // DAC8562: Vdac = code/65536 * 5 V, Vout = 2*Vdac - offset (0 or 5 V)
  for (int i = 0; i < 2; i++) cal[NUM_CV + i] = {65536.0f / 10.0f, 0.0f};
}

static void lock() { xSemaphoreTake(mtx, portMAX_DELAY); }
static void unlock() { xSemaphoreGive(mtx); }

void begin(uint32_t hz) {
  clockHz = hz;
  mtx = xSemaphoreCreateMutex();
  for (int pin : {PIN_CS_DAC_LFO1, PIN_CS_DAC_LFO2, PIN_CS_DAC_VOCT}) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
  }
  pinMode(PIN_TLC_LE, OUTPUT);
  digitalWrite(PIN_TLC_LE, LOW);
  pinMode(PIN_TLC_OE, OUTPUT);
  digitalWrite(PIN_TLC_OE, HIGH);  // LEDs off until the first write
  bus.begin(PIN_BUS_SCLK, -1, PIN_BUS_MOSI, -1);

  nominalCal();
  Preferences p;
  if (p.begin("cvcal", true)) {
    if (p.getBytesLength("cal") == sizeof(cal)) p.getBytes("cal", cal, sizeof(cal));
    voctBi = p.getBool("voctbi", false);
    p.end();
  }
  ledsWrite(0);
  ledsEnable(true);
}

void setClock(uint32_t hz) { clockHz = hz; }

void dac124Write(uint8_t dac, uint8_t channel, uint16_t code) {
  if (code > 4095) code = 4095;
  // DB15-14 address, DB13-12 = 01 "write to register and update output", DB11-0 data
  uint16_t word = ((channel & 3) << 14) | (1 << 12) | code;
  int cs = dac == 0 ? PIN_CS_DAC_LFO1 : PIN_CS_DAC_LFO2;
  lock();
  bus.beginTransaction(SPISettings(clockHz, MSBFIRST, SPI_MODE1));
  digitalWrite(cs, LOW);
  bus.transfer16(word);
  digitalWrite(cs, HIGH);
  bus.endTransaction();
  nTransfers++;
  unlock();
}

static void dac8562Raw(uint8_t cmd, uint16_t data) {
  lock();
  bus.beginTransaction(SPISettings(clockHz, MSBFIRST, SPI_MODE1));
  digitalWrite(PIN_CS_DAC_VOCT, LOW);
  bus.transfer(cmd);
  bus.transfer16(data);
  digitalWrite(PIN_CS_DAC_VOCT, HIGH);
  bus.endTransaction();
  nTransfers++;
  unlock();
}

void dac8562Init() {
  // Command byte = 00 C2C1C0 A2A1A0 (see DAC856x datasheet, "Command and address" table)
  dac8562Raw(0x28, 0x0001);  // software reset, all registers
  delay(1);
  dac8562Raw(0x20, 0x0003);  // power up DAC-A and DAC-B
  dac8562Raw(0x38, 0x0001);  // enable internal 2.5 V reference (VREF net, also feeds VREF_BUF)
  dac8562Raw(0x02, 0x0000);  // gain register: both channels gain 2 -> 0..5 V
  dac8562Raw(0x30, 0x0003);  // LDAC pin inactive for both (pin is tied low anyway)
  dac8562Write(0, 0);
  dac8562Write(1, 0);
}

void dac8562Write(uint8_t channel, uint16_t code) {
  voctCodes[channel & 1] = code;
  dac8562Raw(0x18 | (channel & 1), code);  // write input register n and update DAC n
}

void ledsWrite(uint8_t mask) {
  ledMask = mask;
  // The TLC5916 shares SCLK/SDI with the DACs, so every DAC frame also shifts its register.
  // Shift + latch must therefore happen under the same lock, with nothing in between.
  lock();
  bus.beginTransaction(SPISettings(clockHz, MSBFIRST, SPI_MODE0));
  bus.transfer(mask);  // first bit shifted lands in OUT7
  bus.endTransaction();
  digitalWrite(PIN_TLC_LE, HIGH);
  delayMicroseconds(1);
  digitalWrite(PIN_TLC_LE, LOW);
  nTransfers++;
  unlock();
}

void ledsEnable(bool on) { digitalWrite(PIN_TLC_OE, on ? LOW : HIGH); }

void cvSetCode(uint8_t out, uint16_t code) {
  if (out >= NUM_CV) return;
  if (code > 4095) code = 4095;
  cvCodes[out] = code;
  dac124Write(CV_OUTS[out].dac, CV_OUTS[out].channel, code);
}

void cvSetVolts(uint8_t out, float volts) {
  if (out >= NUM_CV) return;
  float c = cal[out].a * volts + cal[out].b;
  cvSetCode(out, (uint16_t)constrain(lroundf(c), 0L, 4095L));
}

float cvVoltsFromCode(uint8_t out, uint16_t code) {
  if (out >= NUM_CV) return NAN;
  return (code - cal[out].b) / cal[out].a;
}

uint16_t cvCode(uint8_t out) { return out < NUM_CV ? cvCodes[out] : 0; }

void voctSetBipolar(bool bipolar) {
  voctBi = bipolar;
  Preferences p;
  if (p.begin("cvcal", false)) {
    p.putBool("voctbi", bipolar);
    p.end();
  }
}

bool voctBipolar() { return voctBi; }

void voctSetVolts(uint8_t ch, float volts) {
  ch &= 1;
  float v = voctBi ? volts + 5.0f : volts;  // offset from SW3
  float c = cal[NUM_CV + ch].a * v + cal[NUM_CV + ch].b;
  dac8562Write(ch, (uint16_t)constrain(lroundf(c), 0L, 65535L));
}

uint16_t voctCode(uint8_t ch) { return voctCodes[ch & 1]; }
uint8_t leds() { return ledMask; }
uint32_t transfers() { return nTransfers; }

}  // namespace cvbus

namespace cvbus {
// idx 0..7 CV outs, 8..9 V/oct. Two measured points (code, volts) -> new line.
bool calibrate(uint8_t idx, float code1, float volts1, float code2, float volts2) {
  if (idx >= NUM_CV + 2 || volts1 == volts2) return false;
  float a = (code2 - code1) / (volts2 - volts1);
  float b = code1 - a * volts1;
  if (idx >= NUM_CV && voctBi) b -= a * 5.0f;  // store relative to the 0..10 V scale
  cal[idx] = {a, b};
  Preferences p;
  if (!p.begin("cvcal", false)) return false;
  p.putBytes("cal", cal, sizeof(cal));
  p.end();
  return true;
}

void calibrationReset() {
  nominalCal();
  Preferences p;
  if (p.begin("cvcal", false)) {
    p.remove("cal");
    p.end();
  }
}

void calibrationPrint(Print &out) {
  for (int i = 0; i < NUM_CV + 2; i++) {
    const char *name = i < NUM_CV ? CV_OUTS[i].jack : VOCT_JACK[i - NUM_CV];
    out.printf("  %-4s code = %.3f * V + %.1f\n", name, cal[i].a, cal[i].b);
  }
}
}  // namespace cvbus
