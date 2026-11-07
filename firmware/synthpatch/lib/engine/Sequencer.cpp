#include "Sequencer.h"

#include <algorithm>

namespace synth {

void Sequencer::tick(float dt) {
  Events ev;
  {  // this block limits the lock: lock_guard unlocks at the closing brace
    std::lock_guard<std::mutex> lock(m_);

    if (stop_) {
      stop_ = false;
      running_ = false;
      if (gate_) ev.add({Event::GateOff, 0});
    }
    if (restart_) {
      restart_ = false;
      running_ = true;
      pos_ = 0;
      t_ = 0.0f;
      if (gate_) ev.add({Event::GateOff, 0});  // close first, so the new gate is a real new edge
      enterStep(ev);
    } else if (running_) {
      t_ += dt;
      const float step = stepSeconds();
      // When does the gate close inside this step?
      //   Retrig: after gateLength of every step.
      //   Legato: only on the last step, just before the loop starts again.
      const bool closeHere = pat_.mode == Mode::Retrig || pos_ == pat_.length - 1;
      const float closeAt = (pat_.mode == Mode::Retrig ? pat_.gateLength : 0.95f) * step;
      // EPS: adding 0.001 fifty times gives 0.04999.., not 0.05. Without it the gate would close
      // one tick late now and then (float rounding).
      constexpr float EPS = 1e-4f;
      if (gate_ && closeHere && t_ >= closeAt - EPS) ev.add({Event::GateOff, 0});

      if (t_ >= step - EPS) {
        t_ -= step;  // keep the remainder, so the tempo doesn't slowly drift
        pos_ = (pos_ + 1) % pat_.length;
        enterStep(ev);
      }
    }
    for (int i = 0; i < ev.n; i++)
      if (ev.e[i].kind != Event::Note) gate_ = ev.e[i].kind == Event::GateOn;
  }
  emit(ev);  // outside the lock
}

void Sequencer::enterStep(Events &out) {
  const Step &s = pat_.steps[pos_];
  if (s.on) out.add({Event::Note, std::clamp(s.note + transpose_, 0, 127)});
  if (pat_.mode == Mode::Retrig) {
    if (s.on) out.add({Event::GateOn, 0});
  } else if (pos_ == 0) {
    out.add({Event::GateOn, 0});  // legato: one gate per loop
  }
}

void Sequencer::emit(const Events &ev) {
  for (int i = 0; i < ev.n; i++) {
    const Event &e = ev.e[i];
    if (e.kind == Event::Note) noteOut.emit(e.note);
    else gateOut.emit(e.kind == Event::GateOn);
  }
}

void Sequencer::start() {
  std::lock_guard<std::mutex> lock(m_);
  if (!running_) restart_ = true;
}

void Sequencer::stop() {
  std::lock_guard<std::mutex> lock(m_);
  restart_ = false;
  stop_ = true;  // tick() closes the gate, so all output happens in the engine task
}

void Sequencer::trigger() {
  std::lock_guard<std::mutex> lock(m_);
  stop_ = false;
  restart_ = true;
}

void Sequencer::trigger(int rootNote) {
  std::lock_guard<std::mutex> lock(m_);
  transpose_ = rootNote - ROOT_NOTE;
  stop_ = false;
  restart_ = true;
}

int Sequencer::transpose() {
  std::lock_guard<std::mutex> lock(m_);
  return transpose_;
}

Sequencer::Status Sequencer::status() {
  std::lock_guard<std::mutex> lock(m_);
  bool run = (running_ && !stop_) || restart_;
  return {run, running_ ? pos_ : -1, pat_.length, stepSeconds(), t_, transpose_};
}

bool Sequencer::running() {
  std::lock_guard<std::mutex> lock(m_);
  return (running_ && !stop_) || restart_;
}

int Sequencer::position() {
  std::lock_guard<std::mutex> lock(m_);
  return running_ ? pos_ : -1;
}

Sequencer::Pattern Sequencer::pattern() {
  std::lock_guard<std::mutex> lock(m_);
  return pat_;  // a copy
}

void Sequencer::setPattern(const Pattern &p) {
  std::lock_guard<std::mutex> lock(m_);
  pat_ = p;
  pat_.length = std::clamp(p.length, 1, MAX_STEPS);
  pat_.bpm = std::clamp(p.bpm, 20.0f, 300.0f);
  pat_.gateLength = std::clamp(p.gateLength, 0.05f, 0.95f);
  for (auto &s : pat_.steps) s.note = std::clamp(s.note, 0, 127);
  pos_ %= pat_.length;  // a shorter pattern: stay inside it
}

}  // namespace synth
