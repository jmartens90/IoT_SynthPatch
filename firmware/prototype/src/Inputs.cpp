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
  pinMode(pin_, INPUT);
  attachInterrupt(digitalPinToInterrupt(pin_), onEdge, RISING);
}

float FrequencyInput::measure() {
  portENTER_CRITICAL(&mux_);
  uint32_t n = edges_;
  int64_t span = last_ - first_;
  edges_ = 0;
  portEXIT_CRITICAL(&mux_);
  if (n < 3 || span <= 0) return 0.0f;
  return (n - 1) * 1e6f / static_cast<float>(span);
}
