// CvScope.h - the three DAC outputs (LFO, envelope, V/oct), sampled every 20 ms for the
// scope plots in the web page and the Android app.
//
// The ENGINE TASK samples (it runs exactly every 1 ms, so every 20th tick is exactly 20 ms
// apart). Before, loop() took the samples, and loop() is often late (WiFi, the web server,
// a sensor read): the samples came 20..60 ms apart but were drawn 20 ms apart, so the sine
// wobbled, and a long stall glued two far-apart moments together (a jump in the curve).
//
// Every sample gets a running number (its index). The senders remember the index they sent
// last and send everything after it, so nothing is lost or sent twice even when the link is
// slow for a moment. The ring holds 5 s; a client that falls further behind skips ahead, and
// the index in the message tells it where the gap is.
#pragma once

#include <Arduino.h>

class CvScope {
 public:
  static constexpr int SIZE = 256;    // 256 x 20 ms = 5.1 s
  static constexpr int DT_MS = 20;

  // Engine task: one sample.
  void add(float lfo, float env, float voct) {
    portENTER_CRITICAL(&mux_);
    const int i = count_ % SIZE;
    s_[0][i] = lfo;
    s_[1][i] = env;
    s_[2][i] = voct;
    count_++;
    portEXIT_CRITICAL(&mux_);
  }

  // How many samples since boot (the index the next sample will get).
  uint32_t count() {
    portENTER_CRITICAL(&mux_);
    uint32_t c = count_;
    portEXIT_CRITICAL(&mux_);
    return c;
  }

  // Copy up to maxN samples starting at index `from` into out[3][maxN]. If `from` is older
  // than the ring, it is moved forward to the oldest sample still there. Returns how many.
  int read(uint32_t &from, int maxN, float (*out)[3]) {
    portENTER_CRITICAL(&mux_);
    if (count_ - from > static_cast<uint32_t>(SIZE)) from = count_ - SIZE;
    int n = static_cast<int>(count_ - from);
    if (n > maxN) n = maxN;
    for (int j = 0; j < n; j++) {
      const int i = (from + j) % SIZE;
      out[j][0] = s_[0][i];
      out[j][1] = s_[1][i];
      out[j][2] = s_[2][i];
    }
    portEXIT_CRITICAL(&mux_);
    return n;
  }

 private:
  portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;  // spinlock: the engine runs on core 1, WiFi on 0
  float s_[3][SIZE] = {};
  uint32_t count_ = 0;
};
