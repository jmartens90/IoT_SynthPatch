// Signal.h - a tiny version of Qt's signals and slots.
//
// C++ lesson: OBSERVER PATTERN, TEMPLATES, std::function, LAMBDAS
// ---------------------------------------------------------------
// In Qt you write `connect(sender, &Sender::changed, receiver, &Receiver::update)`. When the
// sender emits `changed`, every connected slot runs. The sender doesn't know who listens.
// This class does the same without Qt, so it also works on the ESP32:
//
//     Signal<int> valueChanged;
//     valueChanged.connect([](int v) { printf("now %d\n", v); });  // a lambda as the slot
//     valueChanged.emit(42);                                       // prints "now 42"
//
// `template <typename... Args>` makes it work for any argument types: Signal<float>,
// Signal<std::string, float>, ... The compiler generates a version for each one you use.
// std::function can hold anything callable: a lambda, a normal function, a bound member function.
#pragma once

#include <functional>
#include <vector>

namespace synth {

template <typename... Args>
class Signal {
 public:
  using Slot = std::function<void(Args...)>;

  void connect(Slot slot) { slots_.push_back(std::move(slot)); }

  void emit(Args... args) const {
    for (const auto &slot : slots_) slot(args...);
  }

  size_t connections() const { return slots_.size(); }

 private:
  std::vector<Slot> slots_;
};

}  // namespace synth
