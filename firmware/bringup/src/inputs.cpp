#include "inputs.h"

#include "pins.h"

namespace inputs {

static volatile int32_t encSteps = 0;  // quarter steps
static volatile uint8_t encPrev = 0;
static volatile uint32_t encEdges = 0;

static void IRAM_ATTR encIsr() {
  // Gray-code state table: +1 / -1 for valid transitions, 0 for bounce/invalid
  static const int8_t table[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
  uint8_t s = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);
  encSteps += table[(encPrev << 2) | s];
  encPrev = s;
  encEdges++;
}

void begin() {
  pinMode(PIN_165_PL, OUTPUT);
  digitalWrite(PIN_165_PL, HIGH);
  pinMode(PIN_165_CP, OUTPUT);
  digitalWrite(PIN_165_CP, LOW);
  pinMode(PIN_165_Q7, INPUT);

  // The PCB has external 10k pull-ups + RC filters. The internal pull-ups (~45k) are switched on
  // too, so a bare encoder on a breadboard dev kit also works; on the PCB they're harmless.
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);
  encPrev = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);
  attachInterrupt(PIN_ENC_A, encIsr, CHANGE);
  attachInterrupt(PIN_ENC_B, encIsr, CHANGE);
}

State read() {
  // Load: PL low latches all parallel inputs, Q7 then shows IC6 D7.
  digitalWrite(PIN_165_PL, LOW);
  delayMicroseconds(1);
  digitalWrite(PIN_165_PL, HIGH);
  delayMicroseconds(1);
  uint32_t raw = 0;
  for (int i = 0; i < 24; i++) {
    raw = (raw << 1) | (digitalRead(PIN_165_Q7) ? 1 : 0);
    digitalWrite(PIN_165_CP, HIGH);
    delayMicroseconds(1);
    digitalWrite(PIN_165_CP, LOW);
    delayMicroseconds(1);
  }
  // raw bit 23..16 = IC6 D7..D0, 15..8 = IC13 D7..D0, 7..0 = IC17 D7..D0
  uint8_t ic6 = (raw >> 16) & 0xFF;
  uint8_t ic13 = (raw >> 8) & 0xFF;
  uint8_t ic17 = raw & 0xFF;

  State st{};
  st.raw = raw;
  for (int j = 0; j < 8; j++) {
    bool low = !(ic6 & (1 << GATE_BIT[j]));  // NPN inverts: low = gate high
    if (low) st.gates |= 1 << j;
  }
  st.mutes = ~ic13;              // active low
  st.menu = (~ic17) & 0x03;      // only D0/D1 used
  return st;
}

int32_t encoderCount() { return encSteps / 4; }
bool encoderButton() { return digitalRead(PIN_ENC_SW) == LOW; }
uint32_t encoderEdges() { return encEdges; }

}  // namespace inputs
