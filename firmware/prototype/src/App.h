// App.h - the whole prototype in one object: engine, outputs, sensors, live values, history.
//
// The web page (WebUi), MQTT (MqttLink) and the serial shell all get a reference to this one
// App and only use its public functions. They never touch the hardware themselves.
//
// C++ lesson: COMPOSITION. An App *has* an Engine, *has* DAC outputs, *has* a PitchTracker...
// (filled diamonds in UML). The members are created in the order they are declared and
// destroyed in reverse order, automatically.
#pragma once

#include <memory>
#include <mutex>
#include <string>

#include "DataLog.h"
#include "Engine.h"
#include "History.h"
#include "Inputs.h"
#include "Outputs.h"
#include "PatchBank.h"
#include "PitchTracker.h"
#include "Sequencer.h"
#include "SettingsMirror.h"
#include "TemperatureSensor.h"
#include "config.h"

// One point of the logged history (every second).
struct Sample {
  uint32_t t;        // seconds since boot
  float tempC;       // averaged, NAN if no sensor reading
  float rh;          // humidity %, NAN if the sensor has none
  float hz;          // input frequency, 0 = no signal
  float drift;       // smoothed cents off the current note
  float volts;       // ADC input
};

struct NoteEvent {
  uint32_t t;
  int from, to;      // MIDI notes, -1 = none
};

// Everything the GUI shows "live", copied out in one go under a lock.
struct Live {
  float tempC = NAN;
  const char *tempSensor = "";
  float humidity = NAN;
  float hz = 0;
  int note = -1;
  float centsOff = 0, drift = 0, driftPerMin = 0;
  const char *pitchState = "";
  float volts = 0;
  float lfoV = 0, envV = 0, voctV = 0;
  int voctNote = -1;
  float toneHz = 0;
  const char *envOut = "";
};

class App {
 public:
  App();
  void begin();      // detect hardware, set up the engine, start the 1 kHz engine task
  void loop();       // call from Arduino loop(): sensors, pitch blocks, history, gate button

  // ---- actions (thread-safe, called from web / MQTT / shell) ----
  void post(synth::Command c) { engine_.post(std::move(c)); }
  void gate(bool on, const char *source);
  void playNote(int midi, bool on, const char *source);   // V/oct + envelope gate
  void setVoctVolts(float v) { voct_.setVolts(v); }
  bool setVoctCode(uint16_t c) { return voct_.writeCode(c); }
  bool setVoctGain2(bool on) { return voct_.setGain2(on); }
  bool calibrateVoct(uint16_t c1, float v1, uint16_t c2, float v2);
  void resetVoctCalibration();
  void setToneHz(float hz) { tone_.setHz(hz); }

  // ---- read-outs ----
  Live live();
  SettingsMirror &settings() { return settings_; }
  History<Sample, 600> &history() { return history_; }        // 10 minutes at 1 Hz
  History<NoteEvent, 20> &noteEvents() { return events_; }
  synth::Sequencer &seq() { return seq_; }                      // 16-step sequencer
  PatchBank &patches() { return patches_; }                     // named patches A1..D8 in flash
  DataLog &log() { return log_; }                               // weeks, 1 per 10 s, in flash
  static bool timeValid() { return time(nullptr) > 1700000000; }  // NTP has set the clock
  unsigned noteEventCount() const { return noteCount_; }  // total since boot
  std::string hardwareReport() const;
  const Ad5693Output &voct() const { return voct_; }

  // Called by MqttLink for every note event, so it can publish it.
  synth::Signal<const NoteEvent &> noteEvent;

  static float noteToVolts(int midi) { return (midi - VOCT_ZERO_NOTE) / 12.0f; }  // 1 V/octave
  static constexpr int VOCT_ZERO_NOTE = 36;  // C2 = 0 V, so 0..5 V = C2..C7

 private:
  static void engineTask(void *self);
  void readTemperature();

  synth::Engine engine_;
  synth::Sequencer seq_;
  SettingsMirror settings_;
  synth::PitchTracker pitch_;
  History<Sample, 600> history_;
  History<NoteEvent, 20> events_;
  DataLog log_;
  PatchBank patches_;

  Mcp4725Output lfoDac_{cfg::ADDR_MCP4725_LFO, cfg::MCP4725_VDD};
  Mcp4725Output envDac_{cfg::ADDR_MCP4725_ENV, cfg::MCP4725_VDD};
  PwmOutput envPwm_{cfg::PIN_ENV_PWM, cfg::LEDC_ENV};
  synth::CvOutput *envOut_ = nullptr;  // points at envDac_ or envPwm_, whichever is fitted
  Ad5693Output voct_{cfg::ADDR_AD5693R, cfg::AD5693R_GAIN2};

  std::unique_ptr<TemperatureSensor> temp_;
  FrequencyInput freq_{cfg::PIN_FREQ_IN};
  TestTone tone_{cfg::PIN_TEST_TONE, cfg::LEDC_TONE};
  VoltageInput adc_{cfg::PIN_ADC_IN};

  std::mutex liveMutex_;
  Live live_;
  float tempAvg_[10] = {};
  int tempCount_ = 0, tempIdx_ = 0;
  uint32_t lastBlockMs_ = 0, lastTempMs_ = 0, lastLogMs_ = 0;
  bool btnWasDown_ = false;
  bool trigWasHigh_ = false;  // trigger input, read in the engine task
  volatile unsigned noteCount_ = 0;
};
