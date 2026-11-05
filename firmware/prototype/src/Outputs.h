// Outputs.h - the prototype's voltage outputs, all behind the engine's CvOutput interface.
//
// C++ lesson: ADAPTER PATTERN (again) + INHERITANCE
// -------------------------------------------------
// The engine only knows CvOutput::setVolts(). Each class below adapts one piece of hardware to
// that call: an MCP4725 wants a 12-bit code over I2C, the AD5693R a 16-bit code with its own
// command byte, the PWM pin a duty cycle. Swap the breadboard for the PCB and only these
// adapters change (to the DAC124S085 and DAC8562 ones); the engine and the web GUI don't.
#pragma once

#include <Arduino.h>

#include "CvOutput.h"
#include "I2cBus.h"

// MCP4725: 12-bit, output 0..VDD. "Fast write" = 2 bytes: [0 0 PD1 PD0 D11..D8] [D7..D0].
class Mcp4725Output final : public synth::CvOutput {
 public:
  Mcp4725Output(uint8_t addr, float vdd) : addr_(addr), vdd_(vdd) {}

  bool begin() { return ok_ = I2cBus::instance().probe(addr_); }
  bool ok() const { return ok_; }

  void setVolts(float v) override {
    last_ = clamp(v);
    uint16_t code = static_cast<uint16_t>(last_ / vdd_ * 4095.0f + 0.5f);
    if (code == lastCode_ || !ok_) return;  // skip the bus write if nothing changed
    lastCode_ = code;
    uint8_t b[2] = {static_cast<uint8_t>((code >> 8) & 0x0F), static_cast<uint8_t>(code & 0xFF)};
    I2cBus::instance().write(addr_, b, 2);
  }
  float minVolts() const override { return 0.0f; }
  float maxVolts() const override { return vdd_; }
  float last() const { return last_; }

 private:
  uint8_t addr_;
  float vdd_;
  bool ok_ = false;
  float last_ = 0.0f;
  uint16_t lastCode_ = 0xFFFF;
};

// AD5693R: 16-bit, internal 2.5 V reference, 0..2.5 V (gain 1) or 0..5 V (gain 2).
//   control register: [0x40] [DB15 reset, DB14:13 power-down, DB12 ref off, DB11 gain] [0]
//   write + update:   [0x30] [MSB] [LSB]
// Calibration: volts = (code - offset) / codesPerVolt, measured with a multimeter (see `voct cal`).
class Ad5693Output final : public synth::CvOutput {
 public:
  Ad5693Output(uint8_t addr, bool gain2) : addr_(addr), gain2_(gain2) { resetCalibration(); }

  bool begin() {
    auto &bus = I2cBus::instance();
    const uint8_t reset[3] = {0x40, 0x80, 0x00};  // control register DB15 = software reset
    bus.write(addr_, reset, 3);
    delay(2);
    const uint8_t ctrl[3] = {0x40, static_cast<uint8_t>(gain2_ ? 0x08 : 0x00), 0x00};
    return ok_ = bus.write(addr_, ctrl, 3);
  }
  bool ok() const { return ok_; }

  void setVolts(float v) override {
    last_ = clamp(v);
    writeCode(static_cast<uint16_t>(std::clamp(last_ * codesPerVolt_ + offset_, 0.0f, 65535.0f) + 0.5f));
  }
  // Returns false if the chip did not acknowledge (wrong address, wiring, or not powered).
  bool writeCode(uint16_t code) {
    code_ = code;
    if (!ok_) return false;
    const uint8_t b[3] = {0x30, static_cast<uint8_t>(code >> 8), static_cast<uint8_t>(code & 0xFF)};
    return I2cBus::instance().write(addr_, b, 3);
  }
  float minVolts() const override { return 0.0f; }
  float maxVolts() const override { return gain2_ ? 5.0f : 2.5f; }

  // Two measured points (code1 -> volts1, code2 -> volts2) give the real slope and offset.
  bool calibrate(uint16_t code1, float volts1, uint16_t code2, float volts2) {
    if (volts2 == volts1) return false;
    codesPerVolt_ = (code2 - code1) / (volts2 - volts1);
    offset_ = code1 - volts1 * codesPerVolt_;
    return true;
  }
  void setCalibration(float codesPerVolt, float offset) {
    codesPerVolt_ = codesPerVolt;
    offset_ = offset;
  }
  void resetCalibration() { setCalibration(65536.0f / maxVolts(), 0.0f); }
  float codesPerVolt() const { return codesPerVolt_; }
  float offset() const { return offset_; }
  uint16_t code() const { return code_; }
  float last() const { return last_; }

 private:
  uint8_t addr_;
  bool gain2_;
  bool ok_ = false;
  float codesPerVolt_ = 0.0f, offset_ = 0.0f;
  float last_ = 0.0f;
  uint16_t code_ = 0;
};

// PWM on a GPIO, 11 bits at ~19.5 kHz (12 bits would need a faster timer clock than the S3 has). Add a 10k + 1 uF RC low-pass to get a DC voltage
// (0..3.3 V, ~10 ms smoothing: fine for an envelope, and an LED on the pin shows it directly).
class PwmOutput final : public synth::CvOutput {
 public:
  PwmOutput(int pin, int channel) : pin_(pin), ch_(channel) {}
  void begin() {
    ledcSetup(ch_, 19531, 11);
    ledcAttachPin(pin_, ch_);
  }
  void setVolts(float v) override {
    last_ = clamp(v);
    ledcWrite(ch_, static_cast<uint32_t>(last_ / 3.3f * 2047.0f + 0.5f));
  }
  float minVolts() const override { return 0.0f; }
  float maxVolts() const override { return 3.3f; }
  float last() const { return last_; }

 private:
  int pin_, ch_;
  float last_ = 0.0f;
};
