// Engine.h - owns all modulators and runs them.
//
// C++ lesson: OWNERSHIP WITH unique_ptr, CONTAINERS, MUTEX + RAII
// ---------------------------------------------------------------
// * The engine OWNS the modulators: std::vector<std::unique_ptr<Modulator>>. When the engine is
//   destroyed, the vector is destroyed, each unique_ptr deletes its Modulator. No manual delete.
//   Others (TFT, MQTT) only get a non-owning reference/pointer and never delete it.
// * The vector holds pointers to the BASE class Modulator, so Lfo and Envelope objects sit in the
//   same list (polymorphism).
// * std::queue<Command> is a FIFO: first command in, first executed.
// * std::mutex protects the queue, because post() is called from other tasks. std::lock_guard
//   locks in its constructor and unlocks in its destructor (RAII: "resource acquisition is
//   initialisation"), so you can't forget to unlock, even with an early return.
#pragma once

#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

#include "Command.h"
#include "CvOutput.h"
#include "Modulator.h"
#include "Signal.h"

namespace synth {

class Engine {
 public:
  // Add a modulator driving one output. The engine takes ownership (note the std::move at the
  // call site). Returns a plain pointer for convenience; the engine still owns it.
  Modulator *add(std::unique_ptr<Modulator> mod, CvOutput &out);

  // Thread-safe: may be called from any task.
  void post(Command cmd);

  // Called by the 1 kHz engine task: run queued commands, advance all modulators by dt seconds,
  // and write their outputs.
  void tick(float dt);

  Modulator *find(const std::string &id);
  size_t size() const { return slots_.size(); }

  // Emitted (from the engine task) after a command changed something.
  Signal<const ParamEvent &> paramChanged;
  // Emitted when a command was rejected (unknown id/param/shape), with a reason.
  Signal<const std::string &> error;

 private:
  void execute(const Command &c);

  struct Slot {
    std::unique_ptr<Modulator> mod;
    CvOutput *out;  // not owned: the outputs live as long as the program
  };
  std::vector<Slot> slots_;
  std::queue<Command> pending_;
  std::mutex mutex_;
};

}  // namespace synth
