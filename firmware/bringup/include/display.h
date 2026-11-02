// DFRobot DFR0664 2.0" IPS (ST7789, 240x320) on J18 + its micro-SD slot.
#pragma once
#include <Arduino.h>

namespace display {

bool begin();
void testPattern();
void backlight(uint8_t level);  // 0..255 (PWM on BLK)
void status(const char *line1, const char *line2 = nullptr);
// Draws one waveform (volts) as a scope trace, +-vRange full height.
void scope(const float *v, int n, float vRange, const char *label);
bool sdTest(Print &out);

}  // namespace display
