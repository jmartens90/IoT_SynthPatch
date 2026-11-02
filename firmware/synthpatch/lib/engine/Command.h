// Command.h - a request to change something, as an object.
//
// C++ lesson: COMMAND PATTERN
// ---------------------------
// Settings can change from three places: the Qt app (via MQTT), the HTTP page and the TFT +
// encoder. They all run in different FreeRTOS tasks, while the engine runs in its own 1 kHz
// task. If they all wrote into the Lfo directly, two tasks could touch the same value at once.
// Instead each change is packed into a Command object and put in one queue. The engine task
// takes them out and executes them, one by one, at a safe moment. One path, no races.
// Because a command remembers the old value, undo is easy to add later.
#pragma once

#include <string>

namespace synth {

struct Command {  // `struct` = a class whose members are public by default
  enum class Type { SetParam, SetShape, Gate };

  Type type;
  std::string target;  // modulator id, e.g. "lfo1"
  std::string name;    // parameter name, e.g. "rate" (unused for Gate)
  float value = 0.0f;  // new value, or 1/0 for Gate
  std::string text;    // shape name for SetShape
  std::string source;  // "app", "tft", "http": shown in the log, and used to avoid echo loops

  // Small STATIC "factory" helpers so calling code reads well:
  //   engine.post(Command::setParam("lfo1", "rate", 2.0f, "tft"));
  static Command setParam(std::string target, std::string name, float value, std::string source) {
    return {Type::SetParam, std::move(target), std::move(name), value, "", std::move(source)};
  }
  static Command setShape(std::string target, std::string shape, std::string source) {
    return {Type::SetShape, std::move(target), "shape", 0.0f, std::move(shape), std::move(source)};
  }
  static Command gate(std::string target, bool on, std::string source) {
    return {Type::Gate, std::move(target), "", on ? 1.0f : 0.0f, "", std::move(source)};
  }
};

// What the engine announces after it executed a change (sent to the TFT, MQTT, ...).
struct ParamEvent {
  std::string target;
  std::string name;
  float value;
  std::string text;    // shape name when name == "shape"
  std::string source;  // who asked for it
};

}  // namespace synth
