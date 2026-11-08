#include "App.h"

#include <Preferences.h>

#include <cmath>

#include "ModulatorFactory.h"

using namespace synth;

App::App() {
  // Observer: every note the tracker recognises becomes a NoteEvent in the list and is
  // announced to whoever listens (MQTT). The lambda captures `this` to reach the members.
  pitch_.noteChanged.connect([this](int from, int to) {
    NoteEvent e{static_cast<uint32_t>(millis() / 1000), from, to};
    events_.push(e);
    noteCount_ = noteCount_ + 1;
    noteEvent.emit(e);
  });
}

void App::begin() {
  I2cBus::instance().begin(cfg::PIN_SDA, cfg::PIN_SCL, cfg::I2C_HZ);

  // ---- outputs: use what answers on the bus ----
  lfoDac_.begin();
  if (envDac_.begin()) {
    envOut_ = &envDac_;
  } else {
    envPwm_.begin();
    envOut_ = &envPwm_;
  }
  if (voct_.begin()) {
    Preferences p;  // V/oct calibration survives a reboot (NVS flash)
    p.begin("voct", false);  // read-write: creates the namespace on first boot (no NOT_FOUND error)
    if (p.isKey("cpv")) voct_.setCalibration(p.getFloat("cpv"), p.getFloat("off"));
    p.end();
    voct_.setVolts(0.0f);
  }

  // ---- flash log ----
  if (!log_.begin()) Serial.println("LittleFS log could not be opened");

  // ---- inputs ----
  temp_ = TemperatureSensor::detect();
  freq_.begin();
  tone_.begin();
  adc_.begin();
  pinMode(cfg::PIN_GATE_BTN, INPUT_PULLUP);
  pinMode(cfg::PIN_GATE_OUT, OUTPUT);
  digitalWrite(cfg::PIN_GATE_OUT, LOW);
  pinMode(cfg::PIN_TRIG_IN, INPUT_PULLDOWN);  // nothing plugged in = low = no trigger

  // ---- sequencer: its notes go to V/oct, its gates to the envelope + gate output ----
  // These slots run in the engine task (the sequencer is ticked there).
  seq_.noteOut.connect([this](int midi) { outputNote(midi, "seq"); });
  seq_.gateOut.connect([this](bool on) { gate(on, "seq"); });

  // ---- engine: one LFO, one envelope, scaled to the 0..3.3 V of the breadboard DACs ----
  Modulator *lfo = engine_.add(ModulatorFactory::create("lfo", "lfo1"), lfoDac_);
  Modulator *env = engine_.add(ModulatorFactory::create("envelope", "env1"), *envOut_);
  // The DACs can only make 0..3.3 V, so the ranges are narrowed to what the hardware can do.
  // The LFO swings around the middle, 1.65 V (op-amps will scale that to +-5 V later):
  //   depth  = amplitude, 0..1.65 V  (1.65 = the full 0..3.3 V swing)
  //   offset = centre,    0..3.3 V   (1.65 = the middle; the page shows it as +-1.65 V)
  // offset + depth can still go past 0 or 3.3 V: the DAC clips there and the page shows CLIP.
  lfo->setRange("depth", 0.0f, cfg::MCP4725_VDD / 2);
  lfo->setRange("offset", 0.0f, cfg::MCP4725_VDD);
  env->setRange("peak", 0.0f, cfg::MCP4725_VDD);
  lfo->setParam("rate", 0.5f);
  lfo->setParam("depth", cfg::MCP4725_VDD / 2);
  lfo->setParam("offset", cfg::MCP4725_VDD / 2);
  env->setParam("peak", 3.3f);
  env->setParam("attack", 0.05f);
  env->setParam("release", 0.8f);

  settings_.build(engine_, {"lfo1", "env1"});
  // Connect before the task starts: a Signal's slot list must not change while it emits.
  engine_.paramChanged.connect([this](const ParamEvent &e) { settings_.apply(e); });
  engine_.error.connect([](const std::string &msg) { Serial.printf("engine: %s\n", msg.c_str()); });

  // The engine gets its own FreeRTOS task on core 1 at a higher priority than loop(),
  // so the LFO keeps running smoothly while WiFi and the web server are busy.
  xTaskCreatePinnedToCore(engineTask, "engine", 6144, this, 3, nullptr, 1);
}

void App::engineTask(void *self) {
  App *app = static_cast<App *>(self);  // a FreeRTOS task gets a void*; we passed `this`
  TickType_t last = xTaskGetTickCount();
  int scopeTicks = 0;
  for (;;) {
    // Trigger input: checked every 1 ms here, so a trigger is never more than 1 ms late.
    // (An interrupt would be faster, but trigger() locks a mutex, which an ISR may not do.)
    bool high = digitalRead(cfg::PIN_TRIG_IN) == HIGH;
    if (high && !app->trigWasHigh_) app->seq_.trigger();  // rising edge only
    app->trigWasHigh_ = high;

    app->seq_.tick(0.001f);  // may post gate commands: run it before the engine executes them
    app->engine_.tick(0.001f);
    if (++scopeTicks == CvScope::DT_MS) {  // scope sample: exactly every 20 ms
      scopeTicks = 0;
      float lfo, env, voct;
      app->cvNow(lfo, env, voct);
      app->scope_.add(lfo, env, voct);
    }
    vTaskDelayUntil(&last, pdMS_TO_TICKS(1));  // exactly every 1 ms (FreeRTOS tick = 1 kHz)
  }
}

// One place for every gate: the envelope (via the command queue) and the gate output pin.
void App::gate(bool on, const char *source) {
  digitalWrite(cfg::PIN_GATE_OUT, on ? HIGH : LOW);
  engine_.post(Command::gate("env1", on, source));
}

void App::playNote(int midi, bool on, const char *source) {
  if (on) outputNote(midi, source);
  gate(on, source);
}

// Every note that goes to the V/oct DAC passes here, so the log is complete.
// Called from the engine task (sequencer) and the loop task (web, shell): History has its own lock.
void App::outputNote(int midi, const char *source) {
  voct_.setVolts(noteToVolts(midi));
  {
    std::lock_guard<std::mutex> lock(liveMutex_);
    live_.voctNote = midi;
  }
  voctLog_.push({voctCount_ + 1, static_cast<uint32_t>(millis()), midi, voct_.last(), voct_.code(), source});
  voctCount_ = voctCount_ + 1;
}

void App::cvNow(float &lfo, float &env, float &voct) const {
  lfo = lfoDac_.last();
  env = envOut_ == &envDac_ ? envDac_.last() : envPwm_.last();
  voct = voct_.last();
}

bool App::calibrateVoct(uint16_t c1, float v1, uint16_t c2, float v2) {
  if (!voct_.calibrate(c1, v1, c2, v2)) return false;
  Preferences p;
  p.begin("voct", false);
  p.putFloat("cpv", voct_.codesPerVolt());
  p.putFloat("off", voct_.offset());
  p.end();
  return true;
}

void App::resetVoctCalibration() {
  voct_.resetCalibration();
  Preferences p;
  p.begin("voct", false);
  p.clear();
  p.end();
}

// Edge processing on the temperature: reject impossible values and sudden spikes, then
// average the last 10 readings.
void App::readTemperature() {
  float t;
  if (!temp_->read(t) || t < -40.0f || t > 125.0f) return;
  if (tempCount_ >= 3) {
    float avg = 0;
    for (int i = 0; i < tempCount_; i++) avg += tempAvg_[i];
    avg /= tempCount_;
    if (std::fabs(t - avg) > 5.0f) return;  // a 5 degree jump in one second is a glitch
  }
  tempAvg_[tempIdx_] = t;
  tempIdx_ = (tempIdx_ + 1) % 10;
  if (tempCount_ < 10) tempCount_++;
  float avg = 0;
  for (int i = 0; i < tempCount_; i++) avg += tempAvg_[i];

  std::lock_guard<std::mutex> lock(liveMutex_);
  live_.tempC = avg / tempCount_;
  live_.tempSensor = temp_->name();
  live_.humidity = temp_->humidity();  // virtual call: NAN unless the sensor has humidity
}

void App::loop() {
  uint32_t now = millis();

  // Pitch: one block every 50 ms.
  if (now - lastBlockMs_ >= 50) {
    float dt = (now - lastBlockMs_) / 1000.0f;
    lastBlockMs_ = now;
    float hz = freq_.measure();
    pitch_.addBlock(hz, dt);
    std::lock_guard<std::mutex> lock(liveMutex_);
    live_.hz = hz;
    live_.note = pitch_.note();
    live_.centsOff = pitch_.centsOff();
    live_.drift = pitch_.drift();
    live_.driftPerMin = pitch_.driftPerMinute();
    live_.pitchState = PitchTracker::stateName(pitch_.state());
  }

  // Temperature + ADC every second, then one history sample.
  if (now - lastTempMs_ >= 1000) {
    lastTempMs_ = now;
    readTemperature();
    float v = adc_.read();
    std::lock_guard<std::mutex> lock(liveMutex_);
    live_.volts = v;
    history_.push({now / 1000, live_.tempC, live_.humidity, live_.hz, live_.drift, v});
  }

  // Every 10 s one record into the flash log, but only once NTP has given us the real time
  // (without it the time stamps would be meaningless after a reboot).
  if (now - lastLogMs_ >= 10000 && timeValid()) {
    lastLogMs_ = now;
    Live l = live();
    log_.append({static_cast<uint32_t>(time(nullptr)), l.tempC, l.humidity, l.hz > 0 ? l.hz : NAN,
                 l.drift, l.volts});
  }

  // BOOT button (active low), with a simple edge detect:
  // sequencer running -> press restarts it at step 1; otherwise it is a manual gate.
  bool down = digitalRead(cfg::PIN_GATE_BTN) == LOW;
  if (down != btnWasDown_) {
    btnWasDown_ = down;
    if (seq_.running()) {
      if (down) seq_.trigger();
    } else {
      gate(down, "button");
    }
  }
}

Live App::live() {
  std::lock_guard<std::mutex> lock(liveMutex_);
  Live l = live_;
  // Output values are plain floats written by the engine task; reading one is atomic on the ESP32.
  l.lfoV = lfoDac_.last();
  l.envV = envOut_ == &envDac_ ? envDac_.last() : envPwm_.last();
  l.voctV = voct_.last();
  l.toneHz = tone_.hz();
  l.envOut = envOut_ == &envDac_ ? "MCP4725 0x61" : "PWM GPIO 2";
  return l;
}

std::string App::hardwareReport() const {
  std::string s;
  s += std::string("  LFO  -> MCP4725 0x60: ") + (lfoDac_.ok() ? "ok" : "NOT FOUND") + "\n";
  s += std::string("  ENV  -> ") + (envOut_ == &envDac_ ? "MCP4725 0x61" : "PWM on GPIO 2 (no 2nd MCP4725)") + "\n";
  s += std::string("  V/oct-> AD5693R 0x4C: ") + (voct_.ok() ? "ok" : "NOT FOUND") + "\n";
  s += "  GATE -> GPIO " + std::to_string(cfg::PIN_GATE_OUT) + " (0/3.3 V), trigger in on GPIO " +
       std::to_string(cfg::PIN_TRIG_IN) + "\n";
  s += std::string("  temperature: ") + (temp_ ? temp_->name() : "-") + "\n";
  s += "  flash log: " + std::string(log_.ok() ? "ok, " + std::to_string(log_.count()) + " records" : "NOT available") +
       (timeValid() ? "" : " (waiting for NTP time: needs home WiFi)") + "\n";
  return s;
}
