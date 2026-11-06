// main.cpp - SynthPatch prototype on a bare ESP32-S3 dev kit.
//
// setup() builds everything, loop() keeps the slow things going (sensors, web, MQTT, shell).
// The LFO/envelope engine runs in its own 1 kHz task, started by App::begin().
#include <Arduino.h>

#include "App.h"
#include "MqttLink.h"
#include "WebUi.h"

App app;
WebUi web(app);
MqttLink mqtt(app);

static void help() {
  Serial.println(
      "commands:\n"
      "  status                         hardware found, live values\n"
      "  scan                           list I2C addresses\n"
      "  wifi set <ssid> <password>     save WiFi and reboot\n"
      "  wifi forget                    back to access-point mode\n"
      "  mqtt set <host> [port] [user] [pass]   save broker and reboot\n"
      "  mqtt off\n"
      "  note <midi>                    V/oct to a note (60 = C4 = 2 V)\n"
      "  voct <volts> | voct code <0-65535> | voct gain 1|2\n"
      "  voct cal <code1> <volts1> <code2> <volts2> | voct cal show | voct cal reset\n"
      "  tone <hz>                      test tone on GPIO 17 (0 = off)\n"
      "  gate on|off\n"
      "  seq start|stop|trig            sequencer (trig = restart at step 1)\n"
      "  log | log clear                flash log info / erase it");
}

static void status() {
  Serial.print(app.hardwareReport().c_str());
  Live l = app.live();
  Serial.printf("  temp %.2f C, input %.2f Hz (note %d, %+.1f c, drift %+.1f c), ADC %.3f V\n", l.tempC, l.hz,
                l.note, l.centsOff, l.drift, l.volts);
  Serial.printf("  out: LFO %.2f V, ENV %.2f V, V/oct %.4f V (code %u)\n", l.lfoV, l.envV, l.voctV, app.voct().code());
  Serial.printf("  MQTT %s, heap %u\n", mqtt.connected() ? "connected" : "not connected", ESP.getFreeHeap());
}

static void scan() {
  for (uint8_t a = 1; a < 127; a++)
    if (I2cBus::instance().probe(a)) Serial.printf("  0x%02X\n", a);
}

// Split a line into words. std::vector<String> grows as needed.
static std::vector<String> words(const String &line) {
  std::vector<String> w;
  int i = 0;
  while (i < (int)line.length()) {
    while (i < (int)line.length() && line[i] == ' ') i++;
    int j = line.indexOf(' ', i);
    if (j < 0) j = line.length();
    if (j > i) w.push_back(line.substring(i, j));
    i = j;
  }
  return w;
}

static void command(const String &line) {
  auto w = words(line);
  if (w.empty()) return;
  const String &c = w[0];
  auto arg = [&](size_t i) { return i < w.size() ? w[i] : String(); };

  if (c == "help") help();
  else if (c == "status") status();
  else if (c == "scan") scan();
  else if (c == "wifi" && arg(1) == "set" && w.size() >= 3) {
    WebUi::saveWifi(arg(2).c_str(), arg(3).c_str());
    Serial.println("saved, rebooting");
    delay(200);
    ESP.restart();
  } else if (c == "wifi" && arg(1) == "forget") {
    WebUi::forgetWifi();
    ESP.restart();
  } else if (c == "mqtt" && arg(1) == "set" && w.size() >= 3) {
    MqttLink::save(arg(2).c_str(), w.size() > 3 ? arg(3).toInt() : 1883, arg(4).c_str(), arg(5).c_str());
    Serial.println("saved, rebooting");
    delay(200);
    ESP.restart();
  } else if (c == "mqtt" && arg(1) == "off") {
    MqttLink::forget();
    ESP.restart();
  } else if (c == "note" && w.size() == 2) {
    app.setVoctVolts(App::noteToVolts(arg(1).toInt()));
    Serial.printf("  %.4f V -> code %u\n", App::noteToVolts(arg(1).toInt()), app.voct().code());
  } else if (c == "voct" && arg(1) == "gain" && w.size() == 3) {
    bool ok = app.setVoctGain2(arg(2) == "2");
    Serial.printf("  AD5693R gain %s: %s\n", arg(2) == "2" ? "2 (0..5 V)" : "1 (0..2.5 V)", ok ? "ACK" : "NO ACK");
  } else if (c == "voct" && arg(1) == "code") {
    bool ok = app.setVoctCode(arg(2).toInt());
    Serial.printf("  AD5693R code %ld: %s\n", arg(2).toInt(), ok ? "ACK" : "NO ACK (check wiring/address/power)");
  } else if (c == "voct" && arg(1) == "cal" && arg(2) == "show") {
    Serial.printf("  %.2f codes/V, offset %.1f codes\n", app.voct().codesPerVolt(), app.voct().offset());
  } else if (c == "voct" && arg(1) == "cal" && arg(2) == "reset") {
    app.resetVoctCalibration();
  } else if (c == "voct" && arg(1) == "cal" && w.size() == 6) {
    bool ok = app.calibrateVoct(arg(2).toInt(), arg(3).toFloat(), arg(4).toInt(), arg(5).toFloat());
    Serial.println(ok ? "  calibrated and saved" : "  volts1 and volts2 must differ");
  } else if (c == "voct" && w.size() == 2) {
    app.setVoctVolts(arg(1).toFloat());
    Serial.printf("  %.4f V -> code %u\n", arg(1).toFloat(), app.voct().code());
  } else if (c == "tone") {
    app.setToneHz(arg(1).toFloat());
  } else if (c == "log" && arg(1) == "clear") {
    app.log().clear();
    Serial.println("  log erased");
  } else if (c == "log") {
    time_t a = app.log().oldestTime(), b = app.log().newestTime();
    Serial.printf("  %u records (max %u), %s", app.log().count(), DataLog::CAPACITY, a ? ctime(&a) : "empty\n");
    if (b) Serial.printf("  newest: %s", ctime(&b));
    Serial.printf("  clock: %s\n", App::timeValid() ? "set by NTP" : "not set (connect to home WiFi)");
  } else if (c == "seq") {
    if (arg(1) == "start") app.seq().start();
    else if (arg(1) == "stop") app.seq().stop();
    else if (arg(1) == "trig") app.seq().trigger();
    auto p = app.seq().pattern();
    Serial.printf("  %s, %.0f bpm, %d steps, %s\n", app.seq().running() ? "running" : "stopped", p.bpm, p.length,
                  synth::Sequencer::modeName(p.mode));
  } else if (c == "gate") {
    app.gate(arg(1) == "on", "shell");
  } else {
    Serial.println("?  (type help)");
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\nSynthPatch prototype");
  app.begin();
  Serial.print(app.hardwareReport().c_str());
  web.begin(Serial);
  mqtt.begin(Serial);
  help();
}

void loop() {
  app.loop();
  web.loop();
  mqtt.loop();

  static String line;
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == '\r') continue;
    if (ch == '\n') {
      command(line);
      line = "";
    } else {
      line += ch;
    }
  }
  delay(1);
}
