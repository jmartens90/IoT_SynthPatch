// Sensirion SHT45 temperature / humidity sensor on the STEMMA QT connector J21.
#pragma once
#include <Arduino.h>

namespace sht45 {

struct Reading {
  bool ok;
  float tempC;
  float rh;        // %RH, clamped 0..100
  uint32_t serial;
  const char *error;
};

void begin();
void i2cScan(Print &out);
Reading read();          // high-precision measurement, ~9 ms
uint32_t readSerial();   // 0 on error

}  // namespace sht45
