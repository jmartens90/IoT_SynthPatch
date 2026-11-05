// TemperatureSensor.h - "something that tells the temperature", with three implementations.
//
// C++ lesson: INTERFACE + STRATEGY + FACTORY
// ------------------------------------------
// The rest of the firmware only calls read(). Which sensor is behind it is decided once, at
// startup, by detect(): the SHT45 if it answers on I2C, else a DS18B20 if one is on its pin,
// else a DHT11, else the ESP32's own chip sensor. The code that logs and graphs the temperature never
// changes when you swap sensors; detect() is the only place that knows the concrete classes.
// Adding a BME280 = one more class + one more line in detect().
#pragma once

#include <cmath>
#include <memory>

class TemperatureSensor {
 public:
  virtual ~TemperatureSensor() = default;
  virtual bool begin() = 0;
  // Start a measurement and return the newest value. false = no valid reading this time.
  virtual bool read(float &celsius) = 0;
  virtual const char *name() const = 0;
  // Only some sensors also measure humidity; the default says "not available".
  virtual float humidity() const { return NAN; }

  // FACTORY: try the sensors in order of quality and return the first that works.
  static std::unique_ptr<TemperatureSensor> detect();
};

// Sensirion SHT45 on I2C (0x44): +-0.1 C, every reading CRC-checked.
class Sht45Sensor final : public TemperatureSensor {
 public:
  bool begin() override;
  bool read(float &celsius) override;
  const char *name() const override { return "SHT45"; }
  float humidity() const override { return rh_; }

 private:
  float rh_ = 0.0f;
};

// Maxim DS18B20 1-Wire: +-0.5 C. Conversion takes 750 ms, so read() returns the previous
// result and starts the next conversion (no waiting).
class Ds18b20Sensor final : public TemperatureSensor {
 public:
  Ds18b20Sensor();
  ~Ds18b20Sensor() override;
  bool begin() override;
  bool read(float &celsius) override;
  const char *name() const override { return "DS18B20"; }

 private:
  struct Impl;                  // "pimpl": the OneWire/Dallas objects are hidden in the .cpp
  std::unique_ptr<Impl> impl_;
};

// DHT11 (+-2 C, 1 C steps) or DHT22 (+-0.5 C): one data pin. Good enough to show the
// temperature curve; too coarse to correlate with drift precisely. The library reads at most
// every 2 s and returns the cached value in between.
class DhtSensor final : public TemperatureSensor {
 public:
  DhtSensor();
  ~DhtSensor() override;
  bool begin() override;
  bool read(float &celsius) override;
  const char *name() const override { return "DHT11"; }
  float humidity() const override { return rh_; }

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  float rh_ = NAN;
};

// The ESP32-S3's built-in sensor. Measures the CHIP (it rises with WiFi load and sits several
// degrees above the room), so only a last resort.
class InternalSensor final : public TemperatureSensor {
 public:
  bool begin() override { return true; }
  bool read(float &celsius) override;
  const char *name() const override { return "ESP32 internal"; }
};
