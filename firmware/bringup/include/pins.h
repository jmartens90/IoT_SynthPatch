// IoT_SYNTHPATCH pin map, traced from the Altium schematics in hardware/
// (main board IoT_SYNTHPATCH.PrjPcb + analog board ANALOG/IoT_SYNTHPATCH_ANALOG.PrjPcb).
// Lines marked [H1]/[H3] cross the board-to-board headers to the analog board.
#pragma once
#include <stdint.h>

// ---------- Shared "CV bus" SPI (write only) ----------
constexpr int PIN_BUS_SCLK = 18;   // [H3-2]  via R1 33R. DAC124S085 x2, DAC8562 (via 74HCT125), TLC5916
constexpr int PIN_BUS_MOSI = 17;   // [H3-4]
constexpr int PIN_CS_DAC_LFO1 = 15; // IC7  DAC124S085 SYNC -> outputs J10 J12 J14 J16
constexpr int PIN_CS_DAC_LFO2 = 21; // IC11 DAC124S085 SYNC -> outputs J11 J13 J15 J17
constexpr int PIN_CS_DAC_VOCT = 8;  // IC4  DAC8562 SYNC (through IC10 74HCT125) -> J23 (A), J22 (B)
constexpr int PIN_TLC_LE = 16;      // [H3-5] IC12 TLC5916 latch enable (mute LEDs DS4..DS11)
constexpr int PIN_TLC_OE = 2;       // [H1-38] IC12 output enable, active LOW

// ---------- 74HC165 chain (analog board): menu -> mute -> gates -> ESP ----------
constexpr int PIN_165_PL = 3;   // [H1-37] parallel load, active LOW (10k pull-up R1 on analog)
constexpr int PIN_165_CP = 47;  // [H3-6]  clock
constexpr int PIN_165_Q7 = 48;  // [H3-7]  serial data out of IC6

// ---------- PCM1808 audio ADC (analog board), ESP is I2S master ----------
constexpr int PIN_I2S_MCLK = 35; // [H3-9]  SCKI, via R20 33R
constexpr int PIN_I2S_BCK = 36;  // [H3-10] via R21 33R
constexpr int PIN_I2S_LRCK = 37; // [H3-11]
constexpr int PIN_I2S_DIN = 42;  // [H3-12] PCM1808 DOUT

// ---------- Display J18 (DFRobot DFR0664, ST7789 240x320) on its own SPI ----------
constexpr int PIN_TFT_SCLK = 12;
constexpr int PIN_TFT_MOSI = 11;
constexpr int PIN_TFT_MISO = 9;
constexpr int PIN_TFT_CS = 10;
constexpr int PIN_TFT_DC = 6;
constexpr int PIN_TFT_RST = 5;
constexpr int PIN_TFT_BLK = 4;
constexpr int PIN_SD_CS = 7;  // micro-SD slot on the display module

// ---------- Front panel ----------
constexpr int PIN_ENC_A = 39;   // ENC1, RC filtered, 10k pull-up
constexpr int PIN_ENC_B = 40;
constexpr int PIN_ENC_SW = 41;  // active LOW
constexpr int PIN_STATUS_LED = 1; // DS3, active HIGH

// ---------- MIDI in (J20 -> HCPL-0700 Q9) ----------
constexpr int PIN_MIDI_RX = 13;

// ---------- STEMMA QT / Qwiic J21 (SHT45 temperature/humidity sensor) ----------
constexpr int PIN_I2C_SDA = 38;  // J21-3
constexpr int PIN_I2C_SCL = 14;  // J21-4

// ---------- CV output map, in jack order J10..J17 ----------
// Each DAC124S085 channel is amplified x2 by a TL074 on the analog board.
//   channels A and C: gain 2 to GND      -> 0 .. +10 V   (unipolar, envelopes)
//   channels B and D: gain 2 minus VREF_5 -> -5 .. +5 V   (bipolar, LFOs)
struct CvOut {
  const char *jack;
  uint8_t dac;      // 0 = IC7 (SYNC IO15), 1 = IC11 (SYNC IO21)
  uint8_t channel;  // 0..3 = A..D
  bool bipolar;
};
constexpr int NUM_CV = 8;
constexpr CvOut CV_OUTS[NUM_CV] = {
    {"J10", 0, 0, false}, {"J11", 1, 0, false},
    {"J12", 0, 1, true},  {"J13", 1, 1, true},
    {"J14", 0, 2, false}, {"J15", 1, 2, false},
    {"J16", 0, 3, true},  {"J17", 1, 3, true},
};

// V/oct outputs (DAC8562 0..5 V, x2, minus VOUT_REF chosen by toggle SW3)
//   SW3 -> GND      : 0 .. +10 V
//   SW3 -> VREF_BUF : -5 .. +5 V
constexpr const char *VOCT_JACK[2] = {"J23", "J22"};  // DAC8562 A, B

// 74HC165 chain: 24 bits, first bit read = IC6 D7.
// After readInputs(): bit n of the returned value = data input n in the order below.
//   bits 0..7  : IC6  D0..D7  (gate inputs, inverted by NPN: 0 = gate high)
//   bits 8..15 : IC13 D0..D7  (MUTE_SW4..SW11, 0 = pressed)
//   bits 16..23: IC17 D0..D7  (D0 = MENU_SW12, D1 = MENU_SW13, rest pulled high)
// Gate jack J2..J9 -> IC6 input:
constexpr uint8_t GATE_BIT[8] = {0, 4, 1, 5, 2, 6, 3, 7};
