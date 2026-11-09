// LoopTiming.h - how long each part of loop() takes, to find what holds it up.
//
// Everything that is sent to the web page and the app (scope data, live values, replies) goes
// out from loop(). If one part of loop() blocks for a while (a sensor read, a flash write, a
// network connect), every push waits that long and the plots stutter. This keeps, per second,
// the longest time each part took; `timing` in the shell prints them and the live values carry
// the longest whole loop of the last second ("loop_ms", shown on the Monitor page).
#pragma once

#include <Arduino.h>

class LoopTiming {
 public:
  enum Stage { APP, WEB, BLE, MQTT, SHELL, STAGES };
  static LoopTiming &instance() {
    static LoopTiming t;
    return t;
  }
  static const char *name(int s) {
    static const char *n[STAGES] = {"app (sensors, log)", "web (HTTP, WebSocket)", "bluetooth", "mqtt", "shell"};
    return n[s];
  }

  void begin() { t0_ = last_ = micros(); }
  void mark(Stage s) {  // the part `s` just finished
    const uint32_t now = micros(), d = now - last_;
    last_ = now;
    if (d > cur_[s]) cur_[s] = d;
  }
  void end() {
    const uint32_t d = micros() - t0_;
    if (d > cur_[STAGES]) cur_[STAGES] = d;
    const uint32_t now = millis();
    if (now - secStart_ < 1000) return;
    secStart_ = now;  // a new second: keep the old one, and the worst of the last 10
    slot_ = (slot_ + 1) % 10;
    for (int s = 0; s <= STAGES; s++) {
      hist_[slot_][s] = cur_[s];
      if (cur_[s] > worst_[s]) worst_[s] = cur_[s];
      cur_[s] = 0;
    }
  }

  // longest whole loop() in the last full second, in ms
  float lastSecondMs() const { return hist_[slot_][STAGES] / 1000.0f; }
  // longest of a part (or STAGES = whole loop) in the last 10 s / since boot, in ms
  float last10sMs(int s) const {
    uint32_t m = 0;
    for (const auto &h : hist_)
      if (h[s] > m) m = h[s];
    return m / 1000.0f;
  }
  float worstMs(int s) const { return worst_[s] / 1000.0f; }

 private:
  uint32_t t0_ = 0, last_ = 0, secStart_ = 0;
  uint32_t cur_[STAGES + 1] = {}, worst_[STAGES + 1] = {}, hist_[10][STAGES + 1] = {};
  int slot_ = 0;
};
