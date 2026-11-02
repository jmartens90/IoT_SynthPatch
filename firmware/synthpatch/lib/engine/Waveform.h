// Waveform.h - the shapes an LFO can have.
//
// C++ lesson: STRATEGY PATTERN + INTERFACES
// -----------------------------------------
// An LFO needs "some shape", but which one is chosen by the user at runtime. Instead of a big
// `switch (shape)` inside the LFO, every shape is its own small class with the same interface.
// The LFO only holds a pointer to "a Waveform" and calls value(). Swapping the object swaps the
// behaviour: that is the Strategy pattern.
//
// `Waveform` is an *abstract class* (an interface): it has a *pure virtual* function (`= 0`), so
// you can't create a plain Waveform, only one of the concrete shapes below. `virtual` means the
// call is decided at runtime by the real type of the object (polymorphism).
#pragma once

#include <cmath>
#include <memory>
#include <string>

namespace synth {

class Waveform {
 public:
  // A class you delete through a base-class pointer needs a virtual destructor, otherwise only
  // the base part would be destroyed. `= default` lets the compiler write the (empty) body.
  virtual ~Waveform() = default;

  // phase is 0.0 .. 1.0 (one cycle). Returns -1.0 .. +1.0.
  // `const` at the end: calling value() doesn't change the object.
  virtual float value(float phase) const = 0;

  // Name used by the app, MQTT and the TFT ("sine", "triangle", ...).
  virtual std::string name() const = 0;
};

// `final` = nobody can inherit from this class any further. `override` asks the compiler to
// check that we really replace a virtual function of the base class (catches typos).
class Sine final : public Waveform {
 public:
  float value(float phase) const override { return std::sin(6.2831853f * phase); }
  std::string name() const override { return "sine"; }
};

class Triangle final : public Waveform {
 public:
  float value(float phase) const override {
    return phase < 0.5f ? 4.0f * phase - 1.0f : 3.0f - 4.0f * phase;
  }
  std::string name() const override { return "triangle"; }
};

class Saw final : public Waveform {
 public:
  float value(float phase) const override { return 2.0f * phase - 1.0f; }
  std::string name() const override { return "saw"; }
};

class Square final : public Waveform {
 public:
  float value(float phase) const override { return phase < 0.5f ? 1.0f : -1.0f; }
  std::string name() const override { return "square"; }
};

// FACTORY FUNCTION: turn a name into the right object.
// It returns std::unique_ptr: a smart pointer that *owns* the object and deletes it
// automatically when the pointer goes away. No `delete` needed, no memory leak possible.
// Returns nullptr for an unknown name.
inline std::unique_ptr<Waveform> makeWaveform(const std::string &name) {
  if (name == "sine") return std::make_unique<Sine>();
  if (name == "triangle") return std::make_unique<Triangle>();
  if (name == "saw") return std::make_unique<Saw>();
  if (name == "square") return std::make_unique<Square>();
  return nullptr;
}

}  // namespace synth
