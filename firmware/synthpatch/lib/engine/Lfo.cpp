#include "Lfo.h"

#include <array>

namespace synth {

// The list of shape names in index order. std::array is a fixed-size array that knows its size.
static const std::array<const char *, 4> kShapes = {"sine", "triangle", "saw", "square"};

Lfo::Lfo(std::string id) : Lfo(std::move(id), "sine", 1.0f) {}  // DELEGATING constructor

Lfo::Lfo(std::string id, const std::string &shape, float rateHz, float depthV, float offsetV)
    : Modulator(std::move(id)),  // first construct the parent part
      wave_(makeWaveform(shape)) {
  if (!wave_) wave_ = std::make_unique<Sine>();  // unknown name: fall back to sine
  rate_.set(rateHz);
  depth_.set(depthV);
  offset_.set(offsetV);
  registerParams();
}

void Lfo::registerParams() {
  addParam(rate_);
  addParam(depth_);
  addParam(offset_);
}

float Lfo::tick(float dt) {
  phase_ += rate_.get() * dt;
  phase_ -= static_cast<int>(phase_);  // keep only the fractional part: wrap to 0..1
  output_ = offset_.get() + depth_.get() * wave_->value(phase_);
  return output_;
}

bool Lfo::setShape(const std::string &name) {
  auto w = makeWaveform(name);
  if (!w) return false;
  // std::move hands ownership over: the old shape is deleted, the new one is now ours.
  wave_ = std::move(w);
  return true;
}

bool Lfo::setParam(const std::string &name, float value) {
  if (name == "shape") {
    int i = static_cast<int>(value);
    if (i < 0 || i >= static_cast<int>(kShapes.size())) return false;
    return setShape(kShapes[i]);
  }
  return Modulator::setParam(name, value);  // everything else: let the parent handle it
}

}  // namespace synth
