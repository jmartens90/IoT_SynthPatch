#include "TemperatureSensor.h"

#include <Arduino.h>
#include <DallasTemperature.h>
#include <OneWire.h>

#include "config.h"
#include "I2cBus.h"

// ---------- SHT45 ----------

static uint8_t crc8(const uint8_t *d, int n) {
  uint8_t crc = 0xFF;  // Sensirion CRC-8: polynomial 0x31, start 0xFF
  for (int i = 0; i < n; i++) {
    crc ^= d[i];
    for (int b = 0; b < 8; b++) crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : (crc << 1);
  }
  return crc;
}

bool Sht45Sensor::begin() { return I2cBus::instance().probe(cfg::ADDR_SHT45); }

bool Sht45Sensor::read(float &celsius) {
  auto &bus = I2cBus::instance();
  const uint8_t cmd = 0xFD;  // measure T + RH, high precision
  if (!bus.write(cfg::ADDR_SHT45, &cmd, 1)) return false;
  delay(10);  // the sensor measures; the bus is free meanwhile, so the DACs keep running
  uint8_t b[6];
  if (!bus.read(cfg::ADDR_SHT45, b, 6)) return false;
  if (crc8(b, 2) != b[2] || crc8(b + 3, 2) != b[5]) return false;
  celsius = -45.0f + 175.0f * ((b[0] << 8) | b[1]) / 65535.0f;
  rh_ = constrain(-6.0f + 125.0f * ((b[3] << 8) | b[4]) / 65535.0f, 0.0f, 100.0f);
  return true;
}

// ---------- DS18B20 ----------

struct Ds18b20Sensor::Impl {
  OneWire wire{cfg::PIN_DS18B20};
  DallasTemperature dallas{&wire};
};

Ds18b20Sensor::Ds18b20Sensor() : impl_(std::make_unique<Impl>()) {}
Ds18b20Sensor::~Ds18b20Sensor() = default;  // defined here, where Impl is a complete type

bool Ds18b20Sensor::begin() {
  impl_->dallas.begin();
  if (impl_->dallas.getDeviceCount() == 0) return false;
  impl_->dallas.setResolution(12);
  impl_->dallas.setWaitForConversion(false);  // don't block 750 ms
  impl_->dallas.requestTemperatures();
  return true;
}

bool Ds18b20Sensor::read(float &celsius) {
  float t = impl_->dallas.getTempCByIndex(0);
  impl_->dallas.requestTemperatures();  // start the next one
  if (t == DEVICE_DISCONNECTED_C || t == 85.0f) return false;  // 85.0 = power-on value, not real
  celsius = t;
  return true;
}

// ---------- internal ----------

bool InternalSensor::read(float &celsius) {
  celsius = temperatureRead();
  return !std::isnan(celsius);
}

// ---------- factory ----------

std::unique_ptr<TemperatureSensor> TemperatureSensor::detect() {
  std::unique_ptr<TemperatureSensor> s = std::make_unique<Sht45Sensor>();
  if (s->begin()) return s;
  s = std::make_unique<Ds18b20Sensor>();  // the old object is deleted here, automatically
  if (s->begin()) return s;
  s = std::make_unique<InternalSensor>();
  s->begin();
  return s;
}
