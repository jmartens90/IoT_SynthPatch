// Inputs.h - what the prototype measures besides temperature.
//
// FrequencyInput replaces the PCM1808 for now: a square wave (0..3.3 V!) on a GPIO raises an
// interrupt on every rising edge. Per block (~50 ms) we know how many edges came and the time
// of the first and the last one, so  f = (edges - 1) / (t_last - t_first).
// Timing whole periods ("reciprocal counting") gives ~0.05 cent resolution at 440 Hz with a
// 1 us clock, far better than counting edges per second. Fine up to ~20 kHz.
//
// C++ lesson: `static` members and an interrupt service routine (ISR). An ISR is a plain
// function the hardware calls, it can't be a normal member function (no `this`), so the data
// it shares with the class is `static` and `volatile` (may change at any moment), and access
// from normal code is wrapped in a critical section so the ISR can't change it halfway.
#pragma once

#include <Arduino.h>

class FrequencyInput {
 public:
  explicit FrequencyInput(int pin) : pin_(pin) {}
  void begin();
  // Frequency over the time since the last call, 0 if fewer than 3 edges (no signal).
  float measure();
  bool noisy() const { return noisy_; }  // too many edges: the interrupt is paused

 private:
  static void IRAM_ATTR onEdge();
  int pin_;
  bool noisy_ = false;
  uint32_t pausedAt_ = 0;
  static volatile uint32_t edges_;
  static volatile int64_t first_, last_;
  static portMUX_TYPE mux_;
};

// Square-wave test tone (LEDC). Jumper its pin to the frequency input to try the tracker:
// change the note from the web page and watch the note events.
class TestTone {
 public:
  TestTone(int pin, int channel) : pin_(pin), ch_(channel) {}
  void begin() {
    ledcSetup(ch_, 1000, 10);
    ledcAttachPin(pin_, ch_);
    ledcWrite(ch_, 0);
  }
  void setHz(float hz) {
    hz_ = hz;
    ledcWriteTone(ch_, hz);  // 0 = off
  }
  float hz() const { return hz_; }

 private:
  int pin_, ch_;
  float hz_ = 0.0f;
};

// Analog input in volts (ADC1, factory-calibrated by the ESP32 core). Range 0..~3.1 V.
class VoltageInput {
 public:
  explicit VoltageInput(int pin) : pin_(pin) {}
  void begin() { analogSetPinAttenuation(pin_, ADC_11db); }
  float read() {
    uint32_t mv = 0;
    for (int i = 0; i < 16; i++) mv += analogReadMilliVolts(pin_);  // average out the noise
    return mv / 16.0f / 1000.0f;
  }

 private:
  int pin_;
};
