// Shared write-only SPI bus: 2x DAC124S085 (8 CV outs), DAC8562 (2 V/oct outs), TLC5916 (mute LEDs).
#pragma once
#include <Arduino.h>

namespace cvbus {

void begin(uint32_t hz = 8000000);
void setClock(uint32_t hz);

// Raw access
void dac124Write(uint8_t dac, uint8_t channel, uint16_t code);  // code 0..4095
void dac8562Init();
void dac8562Write(uint8_t channel, uint16_t code);              // 0 = A (J23), 1 = B (J22)
void ledsWrite(uint8_t mask);                                    // bit n = mute LED DS(4+n)
void ledsEnable(bool on);

// Calibrated helpers (nominal, before calibration)
void cvSetVolts(uint8_t out, float volts);   // out 0..7 = J10..J17
void cvSetCode(uint8_t out, uint16_t code);
uint16_t cvCode(uint8_t out);                // last code written
float cvVoltsFromCode(uint8_t out, uint16_t code);

void voctSetVolts(uint8_t ch, float volts);  // uses voctBipolar() to pick the offset
void voctSetBipolar(bool bipolar);           // must match toggle switch SW3
bool voctBipolar();
uint16_t voctCode(uint8_t ch);

// Two-point calibration. idx 0..7 = J10..J17, 8 = V/oct A (J23), 9 = V/oct B (J22).
// Write code1, measure volts1 with a meter, write code2, measure volts2, then call this.
bool calibrate(uint8_t idx, float code1, float volts1, float code2, float volts2);
void calibrationReset();
void calibrationPrint(Print &out);

uint8_t leds();
uint32_t transfers();

}  // namespace cvbus
