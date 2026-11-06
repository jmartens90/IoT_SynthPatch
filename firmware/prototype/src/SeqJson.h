// SeqJson.h - the sequencer pattern to and from JSON (for the web page and the patch files).
//
//   {"bpm":120,"gate":0.5,"mode":"retrig","length":8,"steps":[{"n":60,"on":true},...16]}
//
// fromJson only changes what is in the JSON, so the page can send {"bpm":140} alone.
// `inline` because these functions live in a header that more than one .cpp includes.
#pragma once

#include <ArduinoJson.h>

#include <cstring>

#include "Sequencer.h"

inline void seqToJson(const synth::Sequencer::Pattern &p, JsonObject o) {
  o["bpm"] = p.bpm;
  o["gate"] = p.gateLength;
  o["mode"] = synth::Sequencer::modeName(p.mode);
  o["length"] = p.length;
  JsonArray steps = o["steps"].to<JsonArray>();
  for (const auto &s : p.steps) {
    JsonObject j = steps.add<JsonObject>();
    j["n"] = s.note;
    j["on"] = s.on;
  }
}

inline void seqFromJson(JsonObjectConst o, synth::Sequencer::Pattern &p) {
  if (o["bpm"].is<float>()) p.bpm = o["bpm"];
  if (o["gate"].is<float>()) p.gateLength = o["gate"];
  if (o["length"].is<int>()) p.length = o["length"];
  if (o["mode"].is<const char *>())
    p.mode = strcmp(o["mode"], "legato") == 0 ? synth::Sequencer::Mode::Legato : synth::Sequencer::Mode::Retrig;
  JsonArrayConst steps = o["steps"];
  for (size_t i = 0; i < steps.size() && i < p.steps.size(); i++) {
    if (steps[i]["n"].is<int>()) p.steps[i].note = steps[i]["n"];
    if (steps[i]["on"].is<bool>()) p.steps[i].on = steps[i]["on"];
  }
}
