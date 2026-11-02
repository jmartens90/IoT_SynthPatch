// 74HC165 chain (gate inputs, mute buttons, menu buttons) and rotary encoder.
#pragma once
#include <Arduino.h>

namespace inputs {

struct State {
  uint8_t gates;  // bit n = gate jack J(2+n) is HIGH (already un-inverted)
  uint8_t mutes;  // bit n = mute button SW(4+n) pressed
  uint8_t menu;   // bit 0 = SW12, bit 1 = SW13 pressed
  uint32_t raw;   // the 24 bits as shifted in, first bit in bit 23
};

void begin();
State read();          // ~10 us
int32_t encoderCount(); // detent count (signed, since boot)
bool encoderButton();   // true while pressed
uint32_t encoderEdges(); // raw quadrature transitions seen (debug)

}  // namespace inputs
