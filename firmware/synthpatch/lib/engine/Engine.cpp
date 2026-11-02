#include "Engine.h"

namespace synth {

Modulator *Engine::add(std::unique_ptr<Modulator> mod, CvOutput &out) {
  Modulator *raw = mod.get();  // non-owning view, before we move the ownership away
  slots_.push_back({std::move(mod), &out});
  return raw;
}

void Engine::post(Command cmd) {
  std::lock_guard<std::mutex> lock(mutex_);  // locked from here...
  pending_.push(std::move(cmd));
}  // ...unlocked here, when `lock` goes out of scope

Modulator *Engine::find(const std::string &id) {
  for (auto &s : slots_)
    if (s.mod->id() == id) return s.mod.get();
  return nullptr;
}

void Engine::tick(float dt) {
  // Take all pending commands out under the lock, then execute them without holding it,
  // so a slow slot (e.g. the TFT redraw) never blocks another task's post().
  std::queue<Command> todo;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    std::swap(todo, pending_);
  }
  while (!todo.empty()) {
    execute(todo.front());
    todo.pop();
  }

  // The polymorphic loop: one call, the right tick() for each kind of modulator.
  for (auto &s : slots_) s.out->setVolts(s.mod->tick(dt));
}

void Engine::execute(const Command &c) {
  Modulator *m = find(c.target);
  if (!m) {
    error.emit("unknown modulator '" + c.target + "'");
    return;
  }
  switch (c.type) {
    case Command::Type::Gate:
      m->gate(c.value > 0.5f);
      return;  // gates are not settings, no event
    case Command::Type::SetShape: {
      // Virtual call: Lfo really changes its shape, every other modulator returns false.
      if (!m->setShape(c.text)) {
        error.emit("cannot set shape '" + c.text + "' on " + c.target);
        return;
      }
      paramChanged.emit({c.target, "shape", 0.0f, m->shape(), c.source});
      return;
    }
    case Command::Type::SetParam: {
      if (!m->setParam(c.name, c.value)) {
        error.emit("unknown parameter '" + c.name + "' on " + c.target);
        return;
      }
      float now = c.value;
      m->getParam(c.name, now);  // report the value after clamping, not the requested one
      std::string text = c.name == "shape" ? m->shape() : "";
      paramChanged.emit({c.target, c.name, now, text, c.source});
      return;
    }
  }
}

}  // namespace synth
