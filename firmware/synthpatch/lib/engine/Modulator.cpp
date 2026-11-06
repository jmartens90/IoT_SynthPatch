// Modulator.cpp - the function bodies that are shared by all modulators.
// Declarations live in the .h file (what exists), definitions in the .cpp file (how it works).
// `Modulator::` says "this function belongs to class Modulator".
#include "Modulator.h"

namespace synth {

bool Modulator::setParam(const std::string &name, float value) {
  auto it = byName_.find(name);  // `auto` = let the compiler figure out the iterator type
  if (it == byName_.end()) return false;
  it->second->set(value);  // it->first is the key (name), it->second the Param*
  return true;
}

bool Modulator::getParam(const std::string &name, float &value) const {
  auto it = byName_.find(name);
  if (it == byName_.end()) return false;
  value = it->second->get();
  return true;
}

bool Modulator::setRange(const std::string &name, float min, float max) {
  auto it = byName_.find(name);
  if (it == byName_.end()) return false;
  it->second->setRange(min, max);
  return true;
}

}  // namespace synth
