// CvOutput.h - "something that can put out a voltage".
//
// C++ lesson: INTERFACE + ADAPTER PATTERN
// ---------------------------------------
// The engine only wants to say "J12, give me 2.3 V". How that happens differs: on the module a
// DAC124S085 channel needs a 12-bit code with calibration and an offset for bipolar jacks; on the
// PC demo we just record the number. Each of those is a class that implements this interface.
// The DAC version is an *adapter*: it adapts the DAC's "channel + code" API to setVolts().
// Because the engine depends only on this interface, it compiles and runs on a PC unchanged.
#pragma once

#include <algorithm>

namespace synth {

class CvOutput {
 public:
  virtual ~CvOutput() = default;
  virtual void setVolts(float v) = 0;
  virtual float minVolts() const = 0;
  virtual float maxVolts() const = 0;

  // Helper shared by every implementation: clamp to the jack's range first.
  float clamp(float v) const { return std::clamp(v, minVolts(), maxVolts()); }
};

// Simplest possible implementation: remembers the last value. Used on the PC and in tests.
class MemoryOutput final : public CvOutput {
 public:
  MemoryOutput(float minV, float maxV) : min_(minV), max_(maxV) {}
  void setVolts(float v) override { last_ = clamp(v); }
  float minVolts() const override { return min_; }
  float maxVolts() const override { return max_; }
  float last() const { return last_; }

 private:
  float min_, max_;
  float last_ = 0.0f;
};

}  // namespace synth
