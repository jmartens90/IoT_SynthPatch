// config.h - every pin and I2C address of the prototype in one place.
//
// The I2C pins are the same as J21 on the PCB (IO38 SDA, IO14 SCL), so the SHT45 cable moves
// over unchanged. The rest are free pins on a DevKitC; on the real board the DAC outputs
// become the DAC124S085/DAC8562 adapters and the frequency input becomes the PCM1808.
#pragma once
#include <stdint.h>

namespace cfg {

// I2C bus: SHT45 + MCP4725 (+ a 2nd MCP4725) + AD5693R
constexpr int PIN_SDA = 38;
constexpr int PIN_SCL = 14;
constexpr uint32_t I2C_HZ = 400000;  // fast mode: one 12-bit DAC write takes ~70 us

constexpr uint8_t ADDR_SHT45 = 0x44;
constexpr uint8_t ADDR_MCP4725_LFO = 0x60;  // A0 low
constexpr uint8_t ADDR_MCP4725_ENV = 0x61;  // A0 high, optional 2nd board
constexpr uint8_t ADDR_AD5693R = 0x4C;      // A0 low (0x4E with A0 high)

// DAC supply voltages. The MCP4725 output range is 0..VDD.
// The AD5693R uses its internal 2.5 V reference: gain 1 = 0..2.5 V (2.5 octaves), gain 2 = 0..5 V.
// The Adafruit breakout has its own 3.3 V regulator, so the chip runs on 3.3 V whatever VIN is:
// with gain 2 the output tops out at ~3.3 V (C2..A#4). For 5 octaves add an op-amp with gain 2.
constexpr float MCP4725_VDD = 3.3f;
constexpr bool AD5693R_GAIN2 = true;

// Envelope fallback when there is no 2nd MCP4725: PWM + RC filter (10k + 1 uF) on this pin.
constexpr int PIN_ENV_PWM = 2;

// Temperature fallback 1: DS18B20 data pin (4.7k pull-up to 3V3).
constexpr int PIN_DS18B20 = 21;
// Temperature fallback 2: DHT11 / DHT22 data pin (most breakouts have the pull-up on board).
constexpr int PIN_DHT = 16;

// Frequency input (instead of the PCM1808): a 0..3.3 V square wave, e.g. a 40106/555 oscillator.
constexpr int PIN_FREQ_IN = 13;
// Test tone out: jumper it to PIN_FREQ_IN to try the pitch tracker with no oscillator.
constexpr int PIN_TEST_TONE = 17;

// Analog input (ADC1): read back a DAC output or a pot. Max 3.1 V!
constexpr int PIN_ADC_IN = 8;

// Gate button for the envelope: the BOOT button of the dev kit (active low).
constexpr int PIN_GATE_BTN = 0;

// LEDC channels: 0/1 share timer 0, 2/3 share timer 1, so the PWM and the tone don't interfere.
constexpr int LEDC_ENV = 0;
constexpr int LEDC_TONE = 2;

}  // namespace cfg
