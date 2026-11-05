// I2cBus.h - one I2C bus shared by several tasks.
//
// The engine task writes the DACs 1000 times a second, while the main loop reads the SHT45.
// Two tasks talking on the same two wires at once would garble both messages, so every
// transaction takes the bus lock first.
//
// C++ lesson: SINGLETON + RAII LOCK
// ---------------------------------
// * There is exactly one physical bus, so there is exactly one I2cBus object: instance()
//   returns a reference to a function-local static, created on first use (a "Meyers singleton").
// * I2cBus::Lock is a tiny RAII class: the constructor takes the mutex, the destructor gives
//   it back. Write `I2cBus::Lock lock;` at the top of a block and the bus is yours until the `}`.
#pragma once

#include <Wire.h>

#include <mutex>

class I2cBus {
 public:
  static I2cBus &instance() {
    static I2cBus bus;  // created once, the first time this line runs
    return bus;
  }

  void begin(int sda, int scl, uint32_t hz) { Wire.begin(sda, scl, hz); }

  // true if a device answers on this address
  bool probe(uint8_t addr) {
    Lock lock;
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
  }

  // Write n bytes in one transaction. Returns true on ACK.
  bool write(uint8_t addr, const uint8_t *data, size_t n) {
    Lock lock;
    Wire.beginTransmission(addr);
    Wire.write(data, n);
    return Wire.endTransmission() == 0;
  }

  // Read n bytes. Returns true if all arrived.
  bool read(uint8_t addr, uint8_t *data, size_t n) {
    Lock lock;
    if (Wire.requestFrom(static_cast<int>(addr), static_cast<int>(n)) != static_cast<int>(n)) return false;
    for (size_t i = 0; i < n; i++) data[i] = Wire.read();
    return true;
  }

  class Lock {
   public:
    Lock() { I2cBus::instance().mutex_.lock(); }
    ~Lock() { I2cBus::instance().mutex_.unlock(); }
    Lock(const Lock &) = delete;
    Lock &operator=(const Lock &) = delete;
  };

 private:
  I2cBus() = default;  // private constructor: nobody else can make a second bus object
  std::mutex mutex_;
};
