// IoT_SYNTHPATCH bring-up firmware: a serial test shell that exercises one subsystem at a time.
// Open the USB serial port (115200, newline) and type `help`.
// The stages follow docs/bringup-test-plan.md.

#include <Arduino.h>
#include <esp_system.h>

#include "audio_in.h"
#include "cvbus.h"
#include "display.h"
#include "inputs.h"
#include "midi_in.h"
#include "net.h"
#include "pins.h"
#include "sht45.h"

static const char *FW_VERSION = "bringup-0.1";

// ---------------------------------------------------------------- helpers
static bool heartbeat = true;
static TaskHandle_t lfoTask = nullptr;
static volatile bool lfoRun = false;

static bool keyPressed() {
  if (!Serial.available()) return false;
  while (Serial.available()) Serial.read();
  return true;
}

static int findJack(const String &s) {
  for (int i = 0; i < NUM_CV; i++)
    if (s.equalsIgnoreCase(CV_OUTS[i].jack)) return i;
  return -1;
}

static int voctIndex(const String &s) {
  if (s.equalsIgnoreCase("a") || s.equalsIgnoreCase(VOCT_JACK[0])) return 0;
  if (s.equalsIgnoreCase("b") || s.equalsIgnoreCase(VOCT_JACK[1])) return 1;
  return -1;
}

static void printBits(uint8_t v, int n) {
  for (int i = 0; i < n; i++) Serial.print((v >> i) & 1 ? '1' : '.');
}

static void allCvZero() {
  for (int i = 0; i < NUM_CV; i++) cvbus::cvSetVolts(i, 0.0f);
}

// ---------------------------------------------------------------- LFO test (stage 7)
static void lfoLoop(void *) {
  TickType_t last = xTaskGetTickCount();
  uint32_t n = 0;
  float sh = 0;
  while (lfoRun) {
    float t = n++ / 1000.0f;
    auto ph = [&](float hz) { return fmodf(t * hz, 1.0f); };
    auto sine = [&](float hz) { return sinf(2 * PI * ph(hz)); };
    auto tri = [&](float hz) { float p = ph(hz); return p < 0.5f ? 4 * p - 1 : 3 - 4 * p; };
    if (n % 500 == 0) sh = random(-1000, 1000) / 1000.0f;
    // unipolar outs 0..10 V
    cvbus::cvSetVolts(0, 5 + 5 * sine(0.5f));                 // J10 sine 0.5 Hz
    cvbus::cvSetVolts(1, 5 + 5 * tri(1.0f));                  // J11 triangle 1 Hz
    cvbus::cvSetVolts(4, 10 * ph(2.0f));                      // J14 saw 2 Hz
    cvbus::cvSetVolts(5, ph(0.25f) < 0.5f ? 10 : 0);          // J15 square 0.25 Hz
    // bipolar outs -5..+5 V
    cvbus::cvSetVolts(2, 5 * sine(1.0f));                     // J12 sine 1 Hz
    cvbus::cvSetVolts(3, 5 * tri(0.3f));                      // J13 triangle 0.3 Hz
    cvbus::cvSetVolts(6, 10 * ph(4.0f) - 5);                  // J16 saw 4 Hz
    cvbus::cvSetVolts(7, 5 * sh);                             // J17 sample & hold 2 Hz
    vTaskDelayUntil(&last, 1);
  }
  lfoTask = nullptr;
  vTaskDelete(nullptr);
}

static void lfoStart() {
  if (lfoRun) return;
  lfoRun = true;
  xTaskCreatePinnedToCore(lfoLoop, "lfo", 4096, nullptr, 4, &lfoTask, 1);
}

static void lfoStop() {
  lfoRun = false;
  while (lfoTask) delay(2);
  allCvZero();
}

// ---------------------------------------------------------------- commands
static void cmdHelp() {
  Serial.println(R"(
Stage 1  info | led on|off|blink | spi <hz>
Stage 2  i2c | sht [watch]
Stage 3  in [watch] | enc
Stage 4  leds <hex>|chase|off
Stage 5  cv <J10..J17|all> <volts> | cvcode <jack> <0-4095> | cvsteps
         cal <jack> <code1> <volts1> <code2> <volts2> | cal show | cal reset
Stage 6  voct init | voct <a|b> <volts> | voct code <a|b> <0-65535>
         voct range uni|bi (match SW3) | voct octaves
Stage 7  lfo on|off | follow
Stage 8  midi [cv]
Stage 9  adc [watch] | adc gain <1|0.27> | adc stop | tft test|scope | bl <0-255> | sd
Stage 10 wifi set <ssid> <pass> | wifi on | wifi status | wifi forget
Any watch mode stops when you press Enter.)");
}

static void cmdInfo() {
  Serial.printf("  firmware %s, built %s %s\n", FW_VERSION, __DATE__, __TIME__);
  Serial.printf("  chip %s rev %d, %d cores @ %d MHz\n", ESP.getChipModel(), ESP.getChipRevision(),
                ESP.getChipCores(), ESP.getCpuFreqMHz());
  Serial.printf("  flash %u MB, PSRAM %u KB (free %u KB), heap free %u KB\n", ESP.getFlashChipSize() >> 20,
                ESP.getPsramSize() >> 10, ESP.getFreePsram() >> 10, ESP.getFreeHeap() >> 10);
  Serial.printf("  reset reason %d, MAC %012llX\n", (int)esp_reset_reason(), ESP.getEfuseMac());
  Serial.printf("  SPI frames sent %u\n", cvbus::transfers());
}

static void watchSht() {
  Serial.println("  T [degC]   RH [%]");
  do {
    auto r = sht45::read();
    if (r.ok) Serial.printf("  %7.2f   %6.1f\n", r.tempC, r.rh);
    else Serial.printf("  error: %s\n", r.error);
    for (int i = 0; i < 10 && !Serial.available(); i++) delay(100);
  } while (!keyPressed());
}

static void showInputs(bool watch) {
  Serial.println("  gates J2..J9   mutes SW4..SW11   menu 12 13   raw");
  uint32_t prev = 0xFFFFFFFF;
  do {
    auto s = inputs::read();
    if (s.raw != prev || !watch) {
      Serial.print("  ");
      printBits(s.gates, 8);
      Serial.print("       ");
      printBits(s.mutes, 8);
      Serial.print("          ");
      printBits(s.menu, 2);
      Serial.printf("       %06X\n", s.raw);
      prev = s.raw;
    }
    delay(5);
  } while (watch && !keyPressed());
}

static void watchEncoder() {
  int32_t prev = INT32_MIN;
  bool prevBtn = false;
  do {
    int32_t c = inputs::encoderCount();
    bool b = inputs::encoderButton();
    if (c != prev || b != prevBtn) {
      Serial.printf("  count %ld  button %s  (edges %u)\n", (long)c, b ? "DOWN" : "up", inputs::encoderEdges());
      prev = c;
      prevBtn = b;
    }
    delay(5);
  } while (!keyPressed());
}

static void cmdLeds(const String &arg) {
  if (arg == "chase") {
    Serial.println("  chasing DS4..DS11, Enter to stop");
    int i = 0;
    while (!keyPressed()) {
      cvbus::ledsWrite(1 << (i++ % 8));
      delay(150);
    }
    cvbus::ledsWrite(0);
  } else if (arg == "off" || arg.isEmpty()) {
    cvbus::ledsWrite(0);
  } else {
    cvbus::ledsWrite(strtoul(arg.c_str(), nullptr, 16));
    Serial.printf("  mute LEDs = 0x%02X\n", cvbus::leds());
  }
}

static void cmdCv(const String &jack, const String &val) {
  float v = val.toFloat();
  if (jack.equalsIgnoreCase("all")) {
    for (int i = 0; i < NUM_CV; i++) cvbus::cvSetVolts(i, v);
    Serial.printf("  all CV outs -> %.3f V (bipolar outs clip at +5 V)\n", v);
    return;
  }
  int i = findJack(jack);
  if (i < 0) {
    Serial.println("  unknown jack, use J10..J17");
    return;
  }
  cvbus::cvSetVolts(i, v);
  Serial.printf("  %s (IC%d ch %c, %s) code %u -> expect %.3f V\n", CV_OUTS[i].jack, CV_OUTS[i].dac ? 11 : 7,
                'A' + CV_OUTS[i].channel, CV_OUTS[i].bipolar ? "-5..+5 V" : "0..10 V", cvbus::cvCode(i),
                cvbus::cvVoltsFromCode(i, cvbus::cvCode(i)));
}

static void cmdCvSteps() {
  Serial.println("  stepping all outs through 0/25/50/75/100 % of range, 3 s per step. Enter to stop.");
  const uint16_t codes[] = {0, 1024, 2048, 3072, 4095};
  for (uint16_t c : codes) {
    for (int i = 0; i < NUM_CV; i++) cvbus::cvSetCode(i, c);
    Serial.printf("  code %4u: unipolar outs ~%.2f V, bipolar outs ~%.2f V\n", c, cvbus::cvVoltsFromCode(0, c),
                  cvbus::cvVoltsFromCode(2, c));
    for (int t = 0; t < 30; t++) {
      if (keyPressed()) {
        allCvZero();
        return;
      }
      delay(100);
    }
  }
  allCvZero();
}

static void cmdCal(String *a, int n) {
  if (n < 2 || a[1] == "show") {
    cvbus::calibrationPrint(Serial);
    return;
  }
  if (a[1] == "reset") {
    cvbus::calibrationReset();
    Serial.println("  calibration reset to nominal");
    return;
  }
  if (n < 6) {
    Serial.println("  cal <jack> <code1> <volts1> <code2> <volts2>");
    return;
  }
  int idx = findJack(a[1]);
  if (idx < 0) {
    int v = voctIndex(a[1]);
    if (v >= 0) idx = NUM_CV + v;
  }
  if (idx < 0 || !cvbus::calibrate(idx, a[2].toFloat(), a[3].toFloat(), a[4].toFloat(), a[5].toFloat())) {
    Serial.println("  calibration failed (bad jack or equal volts)");
    return;
  }
  Serial.println("  saved");
  cvbus::calibrationPrint(Serial);
}

static void cmdVoct(String *a, int n) {
  if (n < 2) {
    Serial.printf("  range %s, A(%s) code %u, B(%s) code %u\n", cvbus::voctBipolar() ? "-5..+5 V" : "0..10 V",
                  VOCT_JACK[0], cvbus::voctCode(0), VOCT_JACK[1], cvbus::voctCode(1));
    return;
  }
  if (a[1] == "init") {
    cvbus::dac8562Init();
    Serial.println("  DAC8562 reset, internal ref on, gain 2. Check VREF (tp8) = 2.500 V, VREF_BUF (tp7) = 5.00 V");
  } else if (a[1] == "range" && n > 2) {
    cvbus::voctSetBipolar(a[2] == "bi");
    Serial.printf("  V/oct range set to %s; SW3 must match\n", cvbus::voctBipolar() ? "-5..+5 V" : "0..10 V");
  } else if (a[1] == "code" && n > 3 && voctIndex(a[2]) >= 0) {
    cvbus::dac8562Write(voctIndex(a[2]), a[3].toInt());
  } else if (a[1] == "octaves") {
    Serial.println("  1 V steps on both V/oct outs, 3 s each (tune a VCO, each step = 1 octave). Enter to stop.");
    float lo = cvbus::voctBipolar() ? -5 : 0;
    for (int k = 0; k <= 10; k++) {
      cvbus::voctSetVolts(0, lo + k);
      cvbus::voctSetVolts(1, lo + k);
      Serial.printf("  %+.1f V\n", lo + k);
      for (int t = 0; t < 30; t++) {
        if (keyPressed()) return;
        delay(100);
      }
    }
  } else if (n > 2 && voctIndex(a[1]) >= 0) {
    int ch = voctIndex(a[1]);
    cvbus::voctSetVolts(ch, a[2].toFloat());
    Serial.printf("  %s code %u -> expect %.4f V\n", VOCT_JACK[ch], cvbus::voctCode(ch), a[2].toFloat());
  } else {
    Serial.println("  voct init | voct <a|b> <volts> | voct code <a|b> <n> | voct range uni|bi | voct octaves");
  }
}

static void cmdFollow() {
  lfoStop();
  Serial.println("  gates J2..J9 -> CV outs J10..J17 (high = max), mute buttons toggle mute LEDs. Enter to stop.");
  uint8_t ledState = 0, prevMutes = 0, prevGates = 0xFF;
  while (!keyPressed()) {
    auto s = inputs::read();
    uint8_t pressed = s.mutes & ~prevMutes;
    prevMutes = s.mutes;
    ledState ^= pressed;
    cvbus::ledsWrite(ledState ^ s.gates);
    if (s.gates != prevGates) {
      for (int j = 0; j < 8; j++) {
        bool on = s.gates & (1 << j);
        cvbus::cvSetVolts(j, on ? (CV_OUTS[j].bipolar ? 5.0f : 10.0f) : 0.0f);
      }
      prevGates = s.gates;
    }
    digitalWrite(PIN_STATUS_LED, s.gates || s.menu ? HIGH : LOW);
    delay(2);
  }
  cvbus::ledsWrite(0);
  allCvZero();
}

static void cmdMidi(bool cv) {
  midi::begin();
  Serial.printf("  listening on MIDI in (31250 baud)%s. Enter to stop.\n",
                cv ? ", note -> J23 V/oct (C1 = 0 V), gate -> J10, velocity -> J14" : "");
  midi::Message m;
  uint8_t held = 255;
  uint32_t lastBytes = 0, lastPrint = millis();
  while (!keyPressed()) {
    while (midi::poll(m)) {
      midi::describe(m, Serial);
      uint8_t type = m.status & 0xF0;
      if (!cv) continue;
      if (type == 0x90 && m.d2) {
        held = m.d1;
        cvbus::voctSetVolts(0, (m.d1 - 24) / 12.0f);  // MIDI 24 = C1 = 0 V
        cvbus::cvSetVolts(4, m.d2 / 127.0f * 10.0f);
        cvbus::cvSetVolts(0, 10.0f);
      } else if ((type == 0x80 || (type == 0x90 && !m.d2)) && m.d1 == held) {
        cvbus::cvSetVolts(0, 0.0f);
        held = 255;
      }
    }
    if (millis() - lastPrint > 5000) {
      if (midi::bytesSeen() == lastBytes) Serial.println("  (no MIDI bytes in the last 5 s)");
      lastBytes = midi::bytesSeen();
      lastPrint = millis();
    }
    delay(1);
  }
}

static void cmdAdc(String *a, int n) {
  if (n > 1 && a[1] == "stop") {
    audio::end();
    Serial.println("  I2S stopped");
    return;
  }
  if (n > 2 && a[1] == "gain") {
    audio::setFrontEndGain(a[2].toFloat());
    Serial.printf("  front-end gain %.2f saved (must match jumpers J25/J27)\n", audio::frontEndGain());
    return;
  }
  if (!audio::begin()) {
    Serial.println("  I2S driver install failed");
    return;
  }
  delay(200);
  bool watch = n > 1 && a[1] == "watch";
  Serial.println("  ch    freq [Hz]   Vpp [V]   Vrms [V]   dBFS    DC");
  do {
    for (int ch = 0; ch < 2; ch++) {
      auto s = audio::stats(ch);
      Serial.printf("  %s  %9.2f   %7.3f   %8.4f   %6.1f  %+.4f  (blk %u)\n", ch ? "J28" : "J26", s.freqHz, s.vpp,
                    s.vrms, s.dbfs, s.dcRaw, s.blocks);
    }
    if (watch)
      for (int i = 0; i < 5 && !Serial.available(); i++) delay(100);
  } while (watch && !keyPressed());
}

static void cmdTft(const String &arg) {
  display::begin();
  if (arg == "scope") {
    if (!audio::begin()) {
      Serial.println("  I2S start failed");
      return;
    }
    Serial.println("  live scope of J26 on the display. Enter to stop.");
    static float buf[audio::BLOCK];
    char label[48];
    while (!keyPressed()) {
      int n = audio::snapshot(0, buf, 640);
      auto s = audio::stats(0);
      snprintf(label, sizeof(label), "J26 %.1f Hz %.2f Vpp", s.freqHz, s.vpp);
      display::scope(buf, n, max(0.1f, s.vpp * 0.6f), label);
      delay(50);
    }
  } else {
    display::testPattern();
    Serial.println("  colour fills, 8 bars, white border, text. Check orientation and edge pixels.");
  }
}

static void cmdWifi(String *a, int n) {
  if (n > 3 && a[1] == "set") {
    net::setCredentials(a[2].c_str(), a[3].c_str());
    Serial.println("  saved to NVS (not printed, not in the repo). Now: wifi on");
  } else if (n > 1 && a[1] == "on") {
    sht45::begin();
    net::begin(Serial);
  } else if (n > 1 && a[1] == "forget") {
    net::forget();
    Serial.println("  credentials erased");
  } else {
    net::status(Serial);
  }
}

static void dispatch(String line) {
  line.trim();
  if (line.isEmpty()) return;
  String a[8];
  int n = 0;
  while (line.length() && n < 8) {
    int sp = line.indexOf(' ');
    if (sp < 0) sp = line.length();
    a[n++] = line.substring(0, sp);
    line = line.substring(sp);
    line.trim();
  }
  const String &c = a[0];
  if (c == "help" || c == "?") cmdHelp();
  else if (c == "info") cmdInfo();
  else if (c == "led") {
    heartbeat = a[1] == "blink";
    if (!heartbeat) digitalWrite(PIN_STATUS_LED, a[1] == "on" ? HIGH : LOW);
  } else if (c == "spi" && n > 1) {
    cvbus::setClock(a[1].toInt());
    Serial.printf("  CV bus SCLK = %ld Hz\n", a[1].toInt());
  } else if (c == "i2c") sht45::i2cScan(Serial);
  else if (c == "sht") {
    if (a[1] == "watch") watchSht();
    else {
      auto r = sht45::read();
      if (r.ok) Serial.printf("  %.2f degC  %.1f %%RH  serial %08X\n", r.tempC, r.rh, sht45::readSerial());
      else Serial.printf("  error: %s\n", r.error);
    }
  } else if (c == "in") showInputs(a[1] == "watch");
  else if (c == "enc") watchEncoder();
  else if (c == "leds") cmdLeds(a[1]);
  else if (c == "cv" && n > 2) cmdCv(a[1], a[2]);
  else if (c == "cvcode" && n > 2 && findJack(a[1]) >= 0) {
    cvbus::cvSetCode(findJack(a[1]), a[2].toInt());
    Serial.printf("  expect %.3f V\n", cvbus::cvVoltsFromCode(findJack(a[1]), a[2].toInt()));
  } else if (c == "cvsteps") cmdCvSteps();
  else if (c == "cal") cmdCal(a, n);
  else if (c == "voct") cmdVoct(a, n);
  else if (c == "lfo") {
    if (a[1] == "off") lfoStop();
    else lfoStart();
    Serial.printf("  test LFOs %s\n", lfoRun ? "running on J10..J17" : "stopped");
  } else if (c == "follow") cmdFollow();
  else if (c == "midi") cmdMidi(a[1] == "cv");
  else if (c == "adc") cmdAdc(a, n);
  else if (c == "tft") cmdTft(a[1]);
  else if (c == "bl" && n > 1) {
    display::begin();
    display::backlight(a[1].toInt());
  } else if (c == "sd") display::sdTest(Serial);
  else if (c == "wifi") cmdWifi(a, n);
  else Serial.println("  unknown command, type help");
}

// ---------------------------------------------------------------- setup / loop
void setup() {
  pinMode(PIN_STATUS_LED, OUTPUT);
  digitalWrite(PIN_STATUS_LED, HIGH);
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 3000) delay(10);

  cvbus::begin();
  allCvZero();  // bipolar outs would sit at -5 V with DAC code 0
  cvbus::dac8562Init();
  inputs::begin();
  sht45::begin();

  Serial.printf("\nIoT_SYNTHPATCH %s. Type help.\n> ", FW_VERSION);
}

void loop() {
  static String line;
  static uint32_t lastBeat = 0;
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == '\r') continue;
    if (ch == '\n') {
      Serial.println();
      dispatch(line);
      line = "";
      Serial.print("> ");
    } else if (ch == 8 || ch == 127) {
      if (line.length()) line.remove(line.length() - 1);
    } else {
      line += ch;
      Serial.print(ch);  // echo
    }
  }
  if (heartbeat && millis() - lastBeat > 500) {
    static bool on = false;
    lastBeat = millis();
    on = !on;
    digitalWrite(PIN_STATUS_LED, on ? HIGH : LOW);
  }
  net::loop();
  delay(1);
}
