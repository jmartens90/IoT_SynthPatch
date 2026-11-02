#include "sht45.h"

#include <Wire.h>

#include "pins.h"

namespace sht45 {

static constexpr uint8_t ADDR = 0x44;

static uint8_t crc8(const uint8_t *d, int n) {
  uint8_t crc = 0xFF;  // Sensirion CRC-8: poly 0x31, init 0xFF
  for (int i = 0; i < n; i++) {
    crc ^= d[i];
    for (int b = 0; b < 8; b++) crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : (crc << 1);
  }
  return crc;
}

static bool command(uint8_t cmd, uint8_t *buf, int n, int waitMs) {
  Wire.beginTransmission(ADDR);
  Wire.write(cmd);
  if (Wire.endTransmission() != 0) return false;
  delay(waitMs);
  if (Wire.requestFrom((int)ADDR, n) != n) return false;
  for (int i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}

void begin() { Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 100000); }

void i2cScan(Print &out) {
  int found = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      out.printf("  device at 0x%02X%s\n", a, a == ADDR ? " (SHT4x)" : "");
      found++;
    }
  }
  if (!found) out.println("  no I2C devices found (check cable on J21, 3.3 V on J21-2)");
}

Reading read() {
  Reading r{};
  uint8_t b[6];
  if (!command(0xFD, b, 6, 10)) {  // 0xFD = measure T & RH, high precision
    r.error = "no ACK from 0x44";
    return r;
  }
  if (crc8(b, 2) != b[2] || crc8(b + 3, 2) != b[5]) {
    r.error = "CRC mismatch";
    return r;
  }
  uint16_t t = (b[0] << 8) | b[1];
  uint16_t h = (b[3] << 8) | b[4];
  r.tempC = -45.0f + 175.0f * t / 65535.0f;
  r.rh = constrain(-6.0f + 125.0f * h / 65535.0f, 0.0f, 100.0f);
  r.ok = true;
  return r;
}

uint32_t readSerial() {
  uint8_t b[6];
  if (!command(0x89, b, 6, 2)) return 0;
  if (crc8(b, 2) != b[2] || crc8(b + 3, 2) != b[5]) return 0;
  return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | (b[3] << 8) | b[4];
}

}  // namespace sht45
