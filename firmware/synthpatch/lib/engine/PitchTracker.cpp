#include "PitchTracker.h"

#include <algorithm>
#include <cmath>

namespace synth {

float PitchTracker::hzToCents(float hz) { return 1200.0f * std::log2(hz / 440.0f) + 6900.0f; }

int PitchTracker::nearestNote(float cents) { return static_cast<int>(std::lround(cents / 100.0f)); }

std::string PitchTracker::noteName(int midiNote) {
  if (midiNote < 0) return "-";
  static const char *names[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
  return std::string(names[midiNote % 12]) + std::to_string(midiNote / 12 - 1);
}

const char *PitchTracker::stateName(State s) {
  switch (s) {
    case State::NoInput: return "no input";
    case State::Settling: return "settling";
    case State::Stable: return "stable";
  }
  return "?";
}

void PitchTracker::addBlock(float hz, float dt) {
  t_ += dt;
  hz_ = hz;

  if (hz <= 0.0f) {  // no signal: forget the window, keep the last note and drift
    state_ = State::NoInput;
    window_.clear();
    haveLast_ = false;
    return;
  }

  const float c = hzToCents(hz);

  // 1. A big jump between two blocks = a note was played. Start over with the window.
  if (haveLast_ && std::fabs(c - lastCents_) >= cfg_.noteJumpCents) {
    notePending_ = true;
    window_.clear();
    holdoffUntil_ = t_ + cfg_.holdoffS;
  }
  lastCents_ = c;
  haveLast_ = true;

  // 2. Sliding window of the last second: add the new sample, drop the old ones.
  window_.push_back({t_, c});
  while (!window_.empty() && window_.front().t < t_ - cfg_.stableWindowS) window_.pop_front();

  // Stable = the window covers (almost) a full second and its spread is small.
  auto [lo, hi] = std::minmax_element(window_.begin(), window_.end(),
                                      [](const Sample &a, const Sample &b) { return a.cents < b.cents; });
  const float covered = window_.back().t - window_.front().t;
  const bool stable = covered >= 0.8f * cfg_.stableWindowS && (hi->cents - lo->cents) < cfg_.stableSpreadCents;
  state_ = stable ? State::Stable : State::Settling;
  if (!stable) return;

  // 3. Stable after a jump: this is the new note.
  if (notePending_) {
    int n = nearestNote(c);
    notePending_ = false;
    if (n != note_) {
      int old = note_;
      note_ = n;
      noteChanged.emit(old, n);
    }
  }
  centsOff_ = c - note_ * 100.0f;
  if (t_ < holdoffUntil_) return;

  // 4. Drift: only slow movement counts (rate over the window, in cents per second).
  const float rate = covered > 0.0f ? (window_.back().cents - window_.front().cents) / covered : 0.0f;
  if (std::fabs(rate) > cfg_.driftMaxCentsPerS) return;

  // Exponential moving average: each block pulls the average a little towards the new value.
  // alpha = dt / averageTime gives roughly the same result as averaging the last 30 s.
  const float alpha = std::min(1.0f, dt / cfg_.driftAverageS);
  if (!driftValid_) {
    drift_ = centsOff_;
    driftValid_ = true;
  } else {
    const float before = drift_;
    drift_ += alpha * (centsOff_ - drift_);
    driftRate_ += alpha * ((drift_ - before) / dt - driftRate_);
  }
}

}  // namespace synth
