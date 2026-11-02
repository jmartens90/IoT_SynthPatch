// ModulatorFactory.h - build a modulator from its type name.
//
// C++ lesson: FACTORY PATTERN
// ---------------------------
// Saved settings, MQTT messages and the TFT menu talk about modulators as text: "lfo",
// "envelope". The factory is the one place that turns that text into the right class.
// Calling code doesn't include Lfo.h or Envelope.h and never writes `new`; adding a new kind
// of modulator later means changing only this function.
#pragma once

#include <memory>
#include <string>

#include "Envelope.h"
#include "Lfo.h"

namespace synth {

class ModulatorFactory {
 public:
  // `static`: called on the class itself, no object needed: ModulatorFactory::create(...)
  static std::unique_ptr<Modulator> create(const std::string &type, std::string id) {
    if (type == "lfo") return std::make_unique<Lfo>(std::move(id));
    if (type == "envelope") return std::make_unique<Envelope>(std::move(id));
    return nullptr;  // unknown type
  }
};

}  // namespace synth
