// Param.h - one adjustable setting (LFO rate, envelope attack, ...).
//
// C++ lesson: ENCAPSULATION, GETTERS AND SETTERS
// ----------------------------------------------
// The value is `private`: code outside the class can't write `rate.value_ = -999`. The only way
// in is set(), which clamps the value to the allowed range. So a bad MQTT message or a wrong
// encoder turn can never put the module in an impossible state. That is encapsulation: the
// class protects its own rules.
#pragma once

#include <algorithm>
#include <string>

namespace synth {

class Param {
 public:
  // CONSTRUCTOR: runs when the object is created. The part after ':' is the
  // "member initializer list", which initialises the members directly.
  Param(std::string name, std::string unit, float min, float max, float initial)
      : name_(std::move(name)), unit_(std::move(unit)), min_(min), max_(max), value_(initial) {
    set(initial);
  }

  // GETTERS: read-only access, marked const.
  const std::string &name() const { return name_; }
  const std::string &unit() const { return unit_; }
  float min() const { return min_; }
  float max() const { return max_; }
  float get() const { return value_; }

  // Narrow (or widen) the allowed range, e.g. to the 0..3.3 V of a breadboard DAC. The value is
  // clamped into the new range right away.
  void setRange(float min, float max) {
    min_ = min;
    max_ = max;
    value_ = std::clamp(value_, min_, max_);
  }

  // SETTER: the only way to change the value. Returns true if the value changed.
  bool set(float v) {
    float clamped = std::clamp(v, min_, max_);
    if (clamped == value_) return false;
    value_ = clamped;
    return true;
  }

 private:
  // Trailing underscore is a common naming habit for private members.
  std::string name_;
  std::string unit_;
  float min_;
  float max_;
  float value_;
};

}  // namespace synth
