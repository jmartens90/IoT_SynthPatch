#include "Inputs.h"

volatile uint32_t FrequencyInput::edges_ = 0;
volatile int64_t FrequencyInput::first_ = 0;
volatile int64_t FrequencyInput::last_ = 0;
portMUX_TYPE FrequencyInput::mux_ = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR FrequencyInput::onEdge() {
  int64_t now = esp_timer_get_time();  // microseconds since boot
  portENTER_CRITICAL_ISR(&mux_);
  if (edges_ == 0) first_ = now;
  last_ = now;
  edges_ = edges_ + 1;
  portEXIT_CRITICAL_ISR(&mux_);
}

void FrequencyInput::begin() {
  // Pull-down: with nothing plugged in, a floating pin picks up noise (e.g. from the I2C clock
  // next to it) and can fire this interrupt hundreds of thousands of times a second.
  pinMode(pin_, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(pin_), onEdge, RISING);
}

// Called every 50 ms.
float FrequencyInput::measure() {
  // Storm protection: more than 20 kHz worth of edges is noise, not an oscillator. Every edge is
  // an interrupt, and so many of them leave the CPU no time for anything else (the "Interrupt
  // wdt timeout" crash). So the interrupt is switched off for a second, then tried again.
  if (noisy_) {
    if (millis() - pausedAt_ < 1000) return 0.0f;
    noisy_ = false;
    portENTER_CRITICAL(&mux_);
    edges_ = 0;
    portEXIT_CRITICAL(&mux_);
    attachInterrupt(digitalPinToInterrupt(pin_), onEdge, RISING);
    return 0.0f;
  }
  portENTER_CRITICAL(&mux_);
  uint32_t n = edges_;
  int64_t span = last_ - first_;
  edges_ = 0;
  portEXIT_CRITICAL(&mux_);
  if (n > 1000) {  // > 1000 edges in 50 ms = > 20 kHz
    detachInterrupt(digitalPinToInterrupt(pin_));
    noisy_ = true;
    pausedAt_ = millis();
    return 0.0f;
  }
  if (n < 3 || span <= 0) return 0.0f;
  return (n - 1) * 1e6f / static_cast<float>(span);
}
