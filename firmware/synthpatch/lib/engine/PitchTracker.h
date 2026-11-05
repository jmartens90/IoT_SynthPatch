// PitchTracker.h - tells played notes apart from slow tuning drift.
//
// Feed it one frequency measurement per block (every ~50 ms). It works in CENTS (1/100 of a
// semitone), so the same rules hold at any pitch:  cents = 1200 * log2(hz / 440) + 6900
// (that is MIDI note number * 100, so A4 = 440 Hz = 6900 cents).
//
//   * a jump of >= noteJumpCents between two blocks  -> a NOTE was played: once the pitch is
//     stable again, noteChanged is emitted with the old and new note
//   * pitch stable and moving slowly                 -> DRIFT: offset from the current note,
//     smoothed over ~30 s, plus its rate in cents per minute
//   * no signal (hz <= 0)                            -> state NoInput, nothing logged
//
// The "current note" only changes at a note event, not when the drift crosses a half-tone,
// so a VCO that drifts 60 cents shows +60, not a fake note change to the next key.
//
// C++ lesson: a plain class with a nested `struct Config` holding DEFAULT MEMBER VALUES, a
// std::deque as a sliding window (push at the back, pop at the front), `static` helper
// functions that need no object, and the Signal (observer) from Signal.h for events.
// No Arduino code, so it is tested on the PC with generated frequencies (desktop/tests.cpp)
// and used unchanged on the ESP32 with a GPIO frequency counter now and the PCM1808 later.
#pragma once

#include <deque>
#include <string>

#include "Signal.h"

namespace synth {

class PitchTracker {
 public:
  struct Config {
    float noteJumpCents = 50.0f;     // block-to-block jump that counts as a new note
    float stableSpreadCents = 3.0f;  // max spread over stableWindowS to call the pitch stable
    float stableWindowS = 1.0f;
    float driftMaxCentsPerS = 2.0f;  // faster than this (vibrato, glide) is not drift
    float driftAverageS = 30.0f;     // smoothing time of the drift value
    float holdoffS = 0.3f;           // ignore drift this long after a note change
  };

  enum class State { NoInput, Settling, Stable };

  PitchTracker() : PitchTracker(Config{}) {}  // delegating constructor: default settings
  explicit PitchTracker(const Config &cfg) : cfg_(cfg) {}

  // One measurement. hz <= 0 means "no signal". dt = time since the previous block, seconds.
  void addBlock(float hz, float dt);

  // Emitted when a new note is stable: (old note, new note). Old is -1 for the very first.
  Signal<int, int> noteChanged;

  // Getters.
  State state() const { return state_; }
  float hz() const { return hz_; }
  int note() const { return note_; }                // MIDI note, -1 = none yet
  float centsOff() const { return centsOff_; }      // now, raw
  float drift() const { return drift_; }            // smoothed cents off the current note
  float driftPerMinute() const { return driftRate_ * 60.0f; }
  static const char *stateName(State s);

  // Helpers you can call without an object: PitchTracker::hzToCents(440) == 6900.
  static float hzToCents(float hz);
  static int nearestNote(float cents);
  static std::string noteName(int midiNote);  // 69 -> "A4"

 private:
  struct Sample {
    float t;      // seconds since start
    float cents;
  };

  Config cfg_;
  State state_ = State::NoInput;
  std::deque<Sample> window_;  // the last stableWindowS seconds
  float t_ = 0.0f;
  float lastCents_ = 0.0f;
  bool haveLast_ = false;
  bool notePending_ = true;  // a jump happened (or we just started): set the note when stable
  float holdoffUntil_ = 0.0f;

  float hz_ = 0.0f;
  int note_ = -1;
  float centsOff_ = 0.0f;
  float drift_ = 0.0f;
  float driftRate_ = 0.0f;  // cents per second, smoothed
  bool driftValid_ = false;
};

}  // namespace synth
