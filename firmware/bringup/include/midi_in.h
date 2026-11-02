// MIDI in on UART1 (J20 -> HCPL-0700 -> IO13), 31250 baud, running-status parser.
#pragma once
#include <Arduino.h>

namespace midi {

struct Message {
  uint8_t status;  // full status byte (type | channel)
  uint8_t d1, d2;
  uint8_t len;     // 1..3 bytes
};

void begin();
// Feeds all pending UART bytes to the parser; returns true and fills m when a message completes.
bool poll(Message &m);
uint32_t bytesSeen();
void describe(const Message &m, Print &out);
const char *noteName(uint8_t note);  // e.g. "A4"

}  // namespace midi
