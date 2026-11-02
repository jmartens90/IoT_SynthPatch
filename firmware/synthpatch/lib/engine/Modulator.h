// Modulator.h - common base class of everything that produces a control voltage over time.
//
// C++ lesson: INHERITANCE + ABSTRACT BASE CLASS + POLYMORPHISM
// ------------------------------------------------------------
// An LFO and an envelope are different, but they share a lot: they have a name, adjustable
// parameters, and every millisecond they produce a new output value. That shared part lives
// here, in the *parent* class. Lfo and Envelope are *child* classes (`class Lfo : public
// Modulator`) that inherit it and only add what is special about them.
//
// tick() is pure virtual (`= 0`): every child MUST implement it. The engine keeps a list of
// `Modulator` pointers and simply calls tick() on each. It never needs to know which kind it
// is: the right tick() is chosen at runtime. That is polymorphism.
//
// Access levels: `public` = everyone, `protected` = this class and its children,
// `private` = this class only.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "Param.h"

namespace synth {

class Modulator {
 public:
  explicit Modulator(std::string id) : id_(std::move(id)) {}
  virtual ~Modulator() = default;

  // Copying a modulator would also copy pointers into its own params_, which then point to the
  // wrong object. So copying is switched off (`= delete`): the compiler refuses it.
  Modulator(const Modulator &) = delete;
  Modulator &operator=(const Modulator &) = delete;

  // Advance time by dt seconds and return the new output in volts.
  virtual float tick(float dt) = 0;

  // "lfo" or "envelope", used by the factory and the app.
  virtual std::string type() const = 0;

  // Optional: react to a gate input. The default does nothing; Envelope overrides it.
  virtual void gate(bool /*on*/) {}

  // Optional: a waveform shape. Only Lfo overrides these. Asking "can you do this?" through a
  // virtual function is cleaner than checking the real type with a cast (and the ESP32 build
  // has RTTI switched off, so dynamic_cast isn't even available there).
  virtual bool setShape(const std::string & /*name*/) { return false; }
  virtual std::string shape() const { return ""; }

  // Set a parameter by name, e.g. setParam("rate", 2.5). Returns false if the name is unknown.
  // `virtual` so a child can catch names that aren't plain numbers (Lfo uses it for "shape").
  virtual bool setParam(const std::string &name, float value);
  bool getParam(const std::string &name, float &value) const;

  const std::string &id() const { return id_; }
  float output() const { return output_; }  // last value returned by tick()

  // All parameters, in a fixed order (for the TFT menu and the app).
  const std::vector<Param *> &params() const { return order_; }

 protected:
  // Children call this in their constructor to register their parameters.
  // std::map gives fast lookup by name; std::vector keeps the order for display.
  void addParam(Param &p) {
    byName_[p.name()] = &p;
    order_.push_back(&p);
  }
  float output_ = 0.0f;

 private:
  std::string id_;
  std::map<std::string, Param *> byName_;
  std::vector<Param *> order_;
};

}  // namespace synth
