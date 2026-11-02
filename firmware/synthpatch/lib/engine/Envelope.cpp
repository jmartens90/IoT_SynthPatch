#include "Envelope.h"

#include <algorithm>

namespace synth {

Envelope::Envelope(std::string id) : Modulator(std::move(id)) {
  addParam(attack_);
  addParam(decay_);
  addParam(sustain_);
  addParam(release_);
  addParam(peak_);
}

void Envelope::gate(bool on) {
  // Retrigger starts from the current level, so there is no click when a new note comes
  // while the previous one is still releasing.
  if (on) stage_ = Stage::Attack;
  else if (stage_ != Stage::Idle) stage_ = Stage::Release;
}

float Envelope::tick(float dt) {
  // Linear segments: each stage moves the level at a fixed speed (1 / time per second).
  switch (stage_) {
    case Stage::Idle:
      level_ = 0.0f;
      break;
    case Stage::Attack:
      level_ += dt / attack_.get();
      if (level_ >= 1.0f) {
        level_ = 1.0f;
        stage_ = Stage::Decay;
      }
      break;
    case Stage::Decay:
      level_ -= dt * (1.0f - sustain_.get()) / decay_.get();
      if (level_ <= sustain_.get()) {
        level_ = sustain_.get();
        stage_ = Stage::Sustain;
      }
      break;
    case Stage::Sustain:
      level_ = sustain_.get();  // follows the knob if sustain is changed while holding
      break;
    case Stage::Release:
      // Release time is defined from full level, like on most analog envelopes.
      level_ -= dt / release_.get();
      if (level_ <= 0.0f) {
        level_ = 0.0f;
        stage_ = Stage::Idle;
      }
      break;
  }
  output_ = level_ * peak_.get();
  return output_;
}

const char *Envelope::stageName(Stage s) {
  switch (s) {
    case Stage::Idle: return "idle";
    case Stage::Attack: return "attack";
    case Stage::Decay: return "decay";
    case Stage::Sustain: return "sustain";
    case Stage::Release: return "release";
  }
  return "?";
}

}  // namespace synth
