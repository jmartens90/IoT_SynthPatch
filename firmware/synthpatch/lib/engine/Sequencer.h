// Sequencer.h - a 16-step note sequencer, clocked by the 1 kHz engine task.
//
// Each step has a note (MIDI number) and an on/off switch (off = rest). The sequencer does not
// touch any hardware: it only EMITS what should happen, and the app connects that to the V/oct
// DAC, the envelope and the gate output (Observer pattern again, see SignalSlot.h).
//
//   noteOut(n)   -> set V/oct to note n
//   gateOut(on)  -> envelope gate + gate output pin
//
// Two gate modes:
//   Retrig  every step that is on gets its own gate: on at the start of the step, off after
//           gateLength (5..95 % of the step). The envelope restarts for every note.
//   Legato  one gate per pass through the loop: on at step 1, off just before the loop starts
//           again. In between only the pitch changes, so the envelope plays once per loop.
//           A step that is off holds the previous note.
//
// trigger() restarts at step 1 and starts playing (the BOOT button, the gate input, the web
// TRIG button). So a gate can (re)start the loop in sync with other gear.
// trigger(note) does the same AND transposes: the steps are written relative to C3 (MIDI 48,
// the bottom C of the web keyboard), so pressing E3 plays the whole sequence 4 semitones higher.
//
// Steps are 16th notes: one step = 60 / bpm / 4 seconds.
//
// C++ lesson: std::array (a fixed-size array that knows its size and can be copied), a std::mutex
// because the web task writes the settings while the engine task reads them, and COLLECTING the
// events under the lock but emitting them AFTER unlocking, so a slot can never deadlock by
// calling back into the sequencer.
#pragma once

#include <array>
#include <mutex>

#include "SignalSlot.h"

namespace synth {

class Sequencer {
 public:
  static constexpr int MAX_STEPS = 16;
  static constexpr int ROOT_NOTE = 48;  // C3: a key press of C3 plays the steps as written
  enum class Mode { Retrig, Legato };

  struct Step {
    int note = 60;   // MIDI note, 60 = C4
    bool on = true;  // false = rest
  };

  // A copy of all settings in one struct, so the web page can read or write them in one go.
  struct Pattern {
    std::array<Step, MAX_STEPS> steps{};
    int length = 8;          // 1..16 steps
    float bpm = 120.0f;      // 20..300
    float gateLength = 0.5f; // 0.05..0.95 of a step (Retrig mode)
    Mode mode = Mode::Retrig;
  };

  Signal<int> noteOut;
  Signal<bool> gateOut;

  // ---- called from the engine task ----
  void tick(float dt);

  // ---- called from any task ----
  void start();
  void stop();     // also closes the gate
  void trigger();             // restart at step 1 and run, same transposition
  void trigger(int rootNote);  // restart at step 1, transposed by rootNote - ROOT_NOTE
  int transpose();             // semitones

  // Everything the page needs for the running light, read under one lock (so it is consistent).
  struct Status {
    bool running;
    int pos;          // step playing now, -1 when stopped
    int length;
    float stepS;      // seconds per step
    float inStepS;    // seconds since this step began
    int transpose;
  };
  Status status();
  bool running();
  int position();  // step that is playing now, -1 when stopped

  Pattern pattern();
  void setPattern(const Pattern &p);  // values are clamped to the allowed ranges

  static const char *modeName(Mode m) { return m == Mode::Legato ? "legato" : "retrig"; }

 private:
  struct Event {
    enum Kind { Note, GateOn, GateOff } kind;
    int note;
  };
  // Events of one tick (at most a few): a plain array, no heap allocation in the 1 kHz task.
  struct Events {
    Event e[4];
    int n = 0;
    void add(Event ev) {
      if (n < 4) e[n++] = ev;
    }
  };
  void enterStep(Events &out);  // the start of step pos_
  void emit(const Events &ev);
  float stepSeconds() const { return 60.0f / pat_.bpm / 4.0f; }

  std::mutex m_;
  Pattern pat_;
  bool running_ = false;
  bool gate_ = false;        // what we told gateOut last
  bool restart_ = false;     // trigger() asks tick() to start at step 1
  bool stop_ = false;        // stop() asks tick() to stop and close the gate
  int pos_ = 0;              // current step
  float t_ = 0.0f;           // seconds since the current step began
  int transpose_ = 0;        // semitones added to every step note
};

}  // namespace synth
