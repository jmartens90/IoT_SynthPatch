// PCM1808 stereo ADC over I2S (ESP32-S3 is master: MCLK 256 fs, BCK 64 fs, 48 kHz).
// Inputs J26 (left) and J28 (right) via the TL072 front end on the analog board.
// Note: the ADC input is AC coupled (1 uF + on-chip HPF), so it measures audio-rate
// signals (oscillators, fast LFOs), not DC control voltages.
#pragma once
#include <Arduino.h>

namespace audio {

constexpr uint32_t SAMPLE_RATE = 48000;
constexpr int BLOCK = 2048;  // frames per analysis block (~43 ms)

struct Stats {
  float freqHz;   // 0 if no stable period found
  float vpp;      // peak-to-peak at the input jack, volts (nominal scale, see setFrontEndGain)
  float vrms;     // AC RMS at the input jack, volts
  float dbfs;     // RMS relative to ADC full scale
  float dcRaw;    // mean as fraction of full scale (should be ~0)
  uint32_t blocks;
};

bool begin();                 // installs the I2S driver and starts a capture task
void end();
bool running();
Stats stats(int ch);          // 0 = left (J26), 1 = right (J28)
// Copies the latest block of one channel as volts at the jack; returns frames copied.
int snapshot(int ch, float *dst, int maxFrames);
void setFrontEndGain(float g);  // 1.0 (jumper on R125 100k) or 0.27 (R124 27k)
float frontEndGain();

}  // namespace audio
