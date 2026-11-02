// Lfo.h - low frequency oscillator: output = offset + depth * shape(phase).
//
// C++ lesson: a CHILD CLASS
// -------------------------
// `class Lfo : public Modulator` means: an Lfo *is a* Modulator. It gets everything Modulator has
// (id, params, setParam...) and must implement the pure virtual tick() and type().
// The waveform is held through std::unique_ptr<Waveform>: the Lfo *owns* its shape object
// (composition, the filled diamond in UML) and the shape is deleted automatically with the Lfo.
#pragma once

#include <memory>

#include "Modulator.h"
#include "Waveform.h"

namespace synth {

class Lfo final : public Modulator {
 public:
  // Two constructors = CONSTRUCTOR OVERLOADING: the compiler picks one by the arguments.
  explicit Lfo(std::string id);  // sine, 1 Hz
  Lfo(std::string id, const std::string &shape, float rateHz, float depthV = 5.0f, float offsetV = 0.0f);

  float tick(float dt) override;
  std::string type() const override { return "lfo"; }

  // Accepts the numeric params and also "shape" given as an index (0 sine, 1 triangle, 2 saw,
  // 3 square), which is how the TFT encoder and the app's dropdown send it.
  bool setParam(const std::string &name, float value) override;

  // Strategy swap. Returns false for an unknown name and keeps the old shape.
  bool setShape(const std::string &name) override;
  std::string shape() const override { return wave_->name(); }

  // Restart the cycle (used when a gate should sync the LFO).
  void resetPhase() { phase_ = 0.0f; }
  float phase() const { return phase_; }

 private:
  void registerParams();

  std::unique_ptr<Waveform> wave_;
  float phase_ = 0.0f;  // 0..1
  Param rate_{"rate", "Hz", 0.01f, 50.0f, 1.0f};
  Param depth_{"depth", "V", 0.0f, 5.0f, 5.0f};
  Param offset_{"offset", "V", -5.0f, 5.0f, 0.0f};
};

}  // namespace synth
