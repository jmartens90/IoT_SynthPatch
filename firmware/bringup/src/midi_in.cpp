#include "midi_in.h"

#include "pins.h"

namespace midi {

static uint8_t running = 0;
static uint8_t data[2];
static uint8_t have = 0;
static uint32_t nBytes = 0;

static uint8_t dataLen(uint8_t status) {
  switch (status & 0xF0) {
    case 0xC0:
    case 0xD0:
      return 1;
    case 0xF0:
      if (status == 0xF1 || status == 0xF3) return 1;
      if (status == 0xF2) return 2;
      return 0;
    default:
      return 2;
  }
}

void begin() { Serial1.begin(31250, SERIAL_8N1, PIN_MIDI_RX, -1); }

bool poll(Message &m) {
  while (Serial1.available()) {
    uint8_t b = Serial1.read();
    nBytes++;
    if (b >= 0xF8) {  // real-time (clock, start, stop...): pass through, keep running status
      m = {b, 0, 0, 1};
      return true;
    }
    if (b & 0x80) {
      if (b == 0xF0 || b == 0xF7) {  // ignore SysEx contents
        running = 0;
        continue;
      }
      running = b;
      have = 0;
      if (dataLen(b) == 0) {
        m = {b, 0, 0, 1};
        if (b >= 0xF0) running = 0;
        return true;
      }
      continue;
    }
    if (!running) continue;
    data[have++] = b;
    if (have >= dataLen(running)) {
      m = {running, data[0], have > 1 ? data[1] : (uint8_t)0, (uint8_t)(1 + have)};
      have = 0;
      if (running >= 0xF0) running = 0;
      return true;
    }
  }
  return false;
}

uint32_t bytesSeen() { return nBytes; }

const char *noteName(uint8_t note) {
  static const char *n[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
  static char buf[8];
  snprintf(buf, sizeof(buf), "%s%d", n[note % 12], note / 12 - 1);
  return buf;
}

void describe(const Message &m, Print &out) {
  uint8_t type = m.status & 0xF0, ch = (m.status & 0x0F) + 1;
  switch (type) {
    case 0x90:
      if (m.d2) {
        out.printf("ch%-2d note on   %-4s (%3d) vel %3d\n", ch, noteName(m.d1), m.d1, m.d2);
        return;
      }
      // fall through: note on with velocity 0 = note off
    case 0x80:
      out.printf("ch%-2d note off  %-4s (%3d)\n", ch, noteName(m.d1), m.d1);
      return;
    case 0xB0:
      out.printf("ch%-2d CC %3d = %3d\n", ch, m.d1, m.d2);
      return;
    case 0xE0:
      out.printf("ch%-2d pitch bend %+d\n", ch, ((m.d2 << 7) | m.d1) - 8192);
      return;
    case 0xC0:
      out.printf("ch%-2d program %d\n", ch, m.d1);
      return;
    default:
      if (m.status == 0xF8) return;  // clock is too chatty to print
      out.printf("status 0x%02X %02X %02X\n", m.status, m.d1, m.d2);
  }
}

}  // namespace midi
