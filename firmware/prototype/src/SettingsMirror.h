// SettingsMirror.h - a thread-safe copy of all LFO/envelope settings for the web page and MQTT.
//
// Why a copy? The modulators belong to the engine task. If the web server read lfo1's shape
// while the engine was swapping it, it could touch a deleted object. So the engine *tells*
// this mirror about every change (Observer: it is connected to engine.paramChanged), and the
// web/MQTT code only ever reads the mirror, under its own mutex.
#pragma once

#include <ArduinoJson.h>

#include <mutex>
#include <string>
#include <vector>

#include "Engine.h"

class SettingsMirror {
 public:
  // Take a snapshot of every modulator once, before the engine task starts.
  void build(synth::Engine &engine, const std::vector<std::string> &ids) {
    std::lock_guard<std::mutex> lock(m_);
    mods_.clear();
    for (const auto &id : ids) {
      synth::Modulator *m = engine.find(id);
      if (!m) continue;
      Mod mod{m->id(), m->type(), m->shape(), {}};
      for (const synth::Param *p : m->params()) mod.params.push_back({p->name(), p->unit(), p->min(), p->max(), p->get()});
      mods_.push_back(mod);
    }
  }

  // Slot for engine.paramChanged (runs in the engine task).
  void apply(const synth::ParamEvent &e) {
    std::lock_guard<std::mutex> lock(m_);
    for (auto &mod : mods_) {
      if (mod.id != e.target) continue;
      if (e.name == "shape") {
        mod.shape = e.text;
      } else {
        for (auto &p : mod.params)
          if (p.name == e.name) p.value = e.value;
      }
    }
    version_++;
  }

  // {"lfo1":{"type":"lfo","shape":"sine","params":[{"name":"rate","unit":"Hz","min":..}]},..}
  void toJson(JsonObject out) {
    std::lock_guard<std::mutex> lock(m_);
    for (const auto &mod : mods_) {
      JsonObject o = out[mod.id].to<JsonObject>();
      o["type"] = mod.type;
      if (!mod.shape.empty()) o["shape"] = mod.shape;
      JsonArray ps = o["params"].to<JsonArray>();
      for (const auto &p : mod.params) {
        JsonObject j = ps.add<JsonObject>();
        j["name"] = p.name;
        j["unit"] = p.unit;
        j["min"] = p.min;
        j["max"] = p.max;
        j["value"] = p.value;
      }
    }
  }

  // Goes up on every change, so MQTT can see "something changed, publish again".
  unsigned version() {
    std::lock_guard<std::mutex> lock(m_);
    return version_;
  }

 private:
  struct P {
    std::string name, unit;
    float min, max, value;
  };
  struct Mod {
    std::string id, type, shape;
    std::vector<P> params;
  };
  std::vector<Mod> mods_;
  unsigned version_ = 0;
  std::mutex m_;
};
