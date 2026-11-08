#include "Api.h"

#include <sys/time.h>  // settimeofday

#include <cmath>

#include "App.h"
#include "SeqJson.h"

using synth::Command;

// ---------------------------------------------------------------------------------------------
// Request parsing (for transports that get "path?query" as one string)
// ---------------------------------------------------------------------------------------------

// %41 -> 'A', '+' -> ' '  (how browsers and Qt's QUrlQuery encode special characters)
static std::string urlDecode(const std::string &s) {
  std::string out;
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] == '+') out += ' ';
    else if (s[i] == '%' && i + 2 < s.size()) {
      out += static_cast<char>(strtol(s.substr(i + 1, 2).c_str(), nullptr, 16));
      i += 2;
    } else out += s[i];
  }
  return out;
}

ApiRequest ApiRequest::parse(bool post, const std::string &pathAndQuery, const std::string &body) {
  ApiRequest r;
  r.post = post;
  r.body = body;
  size_t q = pathAndQuery.find('?');
  r.path = pathAndQuery.substr(0, q);
  if (q == std::string::npos) return r;
  std::string query = pathAndQuery.substr(q + 1);
  size_t start = 0;
  while (start <= query.size()) {
    size_t amp = query.find('&', start);
    std::string pair = query.substr(start, amp == std::string::npos ? std::string::npos : amp - start);
    size_t eq = pair.find('=');
    if (!pair.empty()) r.args[urlDecode(pair.substr(0, eq))] = eq == std::string::npos ? "" : urlDecode(pair.substr(eq + 1));
    if (amp == std::string::npos) break;
    start = amp + 1;
  }
  return r;
}

// ---------------------------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------------------------

String Api::toString(const JsonDocument &d) {
  String out;
  serializeJson(d, out);
  return out;
}

static ApiReply json(const JsonDocument &d) {
  ApiReply r;
  r.type = "application/json";
  r.body = "";
  serializeJson(d, r.body);
  return r;
}
static ApiReply text(int status, const char *msg) {
  ApiReply r;
  r.status = status;
  r.body = msg;
  return r;
}

// {"t":[..],"temp":[..],...} built as text: 800 points as a JsonDocument would need ~60 kB RAM.
struct Columns {
  String t, temp, rh, hz, drift, volts;
  static void add(String &s, float v, int dec) {
    if (s.length()) s += ',';
    if (std::isnan(v)) s += "null";
    else s += String(v, dec);
  }
  void push(uint32_t time, float tc, float h, float f, float d, float v) {
    if (t.length()) t += ',';
    t += time;
    add(temp, tc, 2);
    add(rh, h, 1);
    add(hz, f, 2);
    add(drift, d, 2);
    add(volts, v, 3);
  }
  String json() const {
    return "{\"t\":[" + t + "],\"temp\":[" + temp + "],\"rh\":[" + rh + "],\"hz\":[" + hz + "],\"drift\":[" + drift +
           "],\"volts\":[" + volts + "]}";
  }
};

// ---------------------------------------------------------------------------------------------
// The commands. GET = read something, POST = change something.
// ---------------------------------------------------------------------------------------------

ApiReply Api::handle(const ApiRequest &r) {
  const std::string &p = r.path;

  if (!r.post) {
    if (p == "/api/live") {
      JsonDocument d;
      liveJson(d);
      return json(d);
    }
    if (p == "/api/history") {  // last 10 minutes from RAM, 1 per second
      Columns c;
      app_.history().forEach([&](const Sample &x) { c.push(x.t, x.tempC, x.rh, x.hz, x.drift, x.volts); });
      ApiReply rep = text(200, "");
      rep.type = "application/json";
      rep.body = c.json();
      return rep;
    }
    if (p == "/api/log") {  // flash log: ?hours=336&points=800, averaged down to `points`
      float hours = r.has("hours") ? r.argFloat("hours") : 24;
      uint32_t points = r.has("points") ? r.argInt("points") : 800;
      uint32_t since = App::timeValid() ? time(nullptr) - static_cast<uint32_t>(hours * 3600) : 0;
      Columns c;
      app_.log().forEach(since, constrain(points, 10u, 3000u),
                         [&](const LogPoint &x) { c.push(x.t, x.tempC, x.rh, x.hz, x.drift, x.volts); });
      ApiReply rep = text(200, "");
      rep.type = "application/json";
      rep.body = c.json();
      return rep;
    }
    if (p == "/api/events") {  // notes heard on the frequency INPUT
      JsonDocument d;
      JsonArray a = d.to<JsonArray>();
      app_.noteEvents().forEach([&](const NoteEvent &e) {
        JsonObject o = a.add<JsonObject>();
        o["t"] = e.t;
        o["from"] = e.from;
        o["to"] = e.to;
      });
      return json(d);
    }
    if (p == "/api/settings") {
      JsonDocument d;
      app_.settings().toJson(d.to<JsonObject>());
      return json(d);
    }
    if (p == "/api/patches") {
      JsonDocument d;
      app_.patches().list(d.to<JsonArray>());
      return json(d);
    }
    if (p == "/api/seq") {
      JsonDocument d;
      seqToJson(app_.seq().pattern(), d.to<JsonObject>());
      d["pos"] = app_.seq().position();
      d["run"] = app_.seq().running();
      return json(d);
    }
    if (p == "/api/seq/pos") {
      JsonDocument d;
      seqJson(d);
      return json(d);
    }
    return text(404, "not found");
  }

  // ---- POST: changes. Engine changes go through its command queue (thread-safe). ----
  if (p == "/api/set") {
    app_.post(Command::setParam(r.arg("target"), r.arg("name"), r.argFloat("value"), r.source));
    return text(200, "ok");
  }
  if (p == "/api/shape") {
    app_.post(Command::setShape(r.arg("target"), r.arg("shape"), r.source));
    return text(200, "ok");
  }
  if (p == "/api/patch/save") {
    bool ok = app_.patches().save(r.argInt("slot"), r.arg("name"), app_);
    return ok ? text(200, "saved") : text(400, "save failed");
  }
  if (p == "/api/patch/load") {
    bool ok = app_.patches().load(r.argInt("slot"), app_);
    return ok ? text(200, "loaded") : text(404, "empty slot");
  }
  if (p == "/api/patch/delete") {
    bool ok = app_.patches().remove(r.argInt("slot"));
    return ok ? text(200, "deleted") : text(404, "empty slot");
  }
  if (p == "/api/seq") {  // body = JSON with only the fields that change, e.g. {"bpm":140}
    JsonDocument in;
    if (deserializeJson(in, r.body)) return text(400, "bad json");
    synth::Sequencer::Pattern pat = app_.seq().pattern();
    seqFromJson(in.as<JsonObjectConst>(), pat);
    app_.seq().setPattern(pat);
    return text(200, "ok");
  }
  if (p == "/api/seq/run") {
    if (r.arg("on") == "1") app_.seq().start();
    else app_.seq().stop();
    return text(200, "ok");
  }
  if (p == "/api/seq/trig") {  // ?note=52 = restart transposed, no note = restart as it is
    if (r.has("note")) app_.seq().trigger(r.argInt("note"));
    else app_.seq().trigger();
    return text(200, "ok");
  }
  if (p == "/api/time") {  // the client's clock, when there is no NTP (NTP always wins)
    long t = atol(r.arg("t").c_str());
    if (App::timeValid() || t <= 1700000000) return text(200, "time already valid");
    timeval tv{t, 0};
    settimeofday(&tv, nullptr);
    return text(200, "time set from the client");
  }
  if (p == "/api/gate") {
    app_.gate(r.arg("on") == "1", r.source);
    return text(200, "ok");
  }
  if (p == "/api/note") {
    int n = r.argInt("n");
    bool on = r.arg("on") == "1";
    app_.playNote(n, on, r.source);
    if (on && r.arg("tone") == "1") app_.setToneHz(440.0f * powf(2.0f, (n - 69) / 12.0f));
    return text(200, "ok");
  }
  if (p == "/api/tone") {
    app_.setToneHz(r.argFloat("hz"));
    return text(200, "ok");
  }
  return text(404, "not found");
}

// ---------------------------------------------------------------------------------------------
// Pushes
// ---------------------------------------------------------------------------------------------

void Api::collect(PushState &st, const Send &send) {
  uint32_t now = millis();
  JsonDocument d;

  synth::Sequencer::Status s = app_.seq().status();
  if (s.epoch != st.seqEpoch || now - st.lastSeq >= 5000) {  // changed, or 5 s against drift
    st.seqEpoch = s.epoch;
    st.lastSeq = now;
    seqJson(d);
    send(toString(d));
  }
  // CV scope: every sample after the last one this client got, with the index of the first
  // one ("i"), so the receiver can put each sample at its true time and see any gap.
  CvScope &scope = app_.scope();
  if (scope.count() - st.cvSent >= static_cast<uint32_t>(CV_PACK)) {
    float buf[CV_MAX][3];
    uint32_t from = st.cvSent;
    const int n = scope.read(from, CV_MAX, buf);
    st.cvSent = from + n;
    d.clear();
    d["type"] = "cv";
    d["dt"] = CvScope::DT_MS;
    d["i"] = from;
    const char *names[3] = {"lfo", "env", "voct"};
    for (int k = 0; k < 3; k++) {
      JsonArray a = d[names[k]].to<JsonArray>();
      for (int j = 0; j < n; j++) a.add(roundf(buf[j][k] * 1000) / 1000);  // mV is enough
    }
    send(toString(d));
  }
  if (app_.voctCount() != st.notesSent) {  // new notes on the V/oct output
    d.clear();
    notesJson(d, st.notesSent);
    st.notesSent = app_.voctCount();
    send(toString(d));
  }
  if (now - st.lastLive >= 1000) {
    st.lastLive = now;
    d.clear();
    liveJson(d);
    send(toString(d));
  }
}

void Api::snapshot(PushState &st, const Send &send) {
  st = PushState{};
  st.cvSent = app_.scope().count();  // CV from now on (the plot starts empty anyway)
  JsonDocument d;
  notesJson(d, 0);  // the whole note log (last 32)
  st.notesSent = app_.voctCount();
  send(toString(d));
  collect(st, send);  // live + seq right away
}

void Api::liveJson(JsonDocument &d) {
  Live l = app_.live();
  d["type"] = "live";
  d["temp_c"] = l.tempC;  // NAN becomes null in the JSON
  d["temp_sensor"] = l.tempSensor;
  d["rh"] = l.humidity;
  d["hz"] = l.hz;
  d["note"] = l.note;
  d["cents_off"] = l.centsOff;
  d["drift"] = l.drift;
  d["drift_per_min"] = l.driftPerMin;
  d["pitch_state"] = l.pitchState;
  d["volts"] = l.volts;
  d["lfo_v"] = l.lfoV;
  d["env_v"] = l.envV;
  d["env_out"] = l.envOut;
  d["voct_v"] = l.voctV;
  d["voct_note"] = l.voctNote;
  d["tone_hz"] = l.toneHz;
  d["events_total"] = app_.noteEventCount();
  d["uptime"] = millis() / 1000;
  d["time"] = App::timeValid() ? static_cast<uint32_t>(time(nullptr)) : 0;
  d["log_count"] = app_.log().count();
  if (netInfo) netInfo(d);
}

void Api::notesJson(JsonDocument &d, uint32_t after) {
  d["type"] = "notes";
  JsonArray list = d["list"].to<JsonArray>();
  app_.voctLog().forEach([&](const VoctEvent &e) {
    if (e.n <= after) return;
    JsonObject o = list.add<JsonObject>();
    o["n"] = e.n;
    o["ms"] = e.ms;
    o["note"] = e.note;
    o["v"] = e.volts;
    o["code"] = e.code;
    o["src"] = e.src;
  });
}

// Where the sequencer is: the client counts the next steps itself with its own clock.
void Api::seqJson(JsonDocument &d) {
  synth::Sequencer::Status s = app_.seq().status();
  d["type"] = "seq";
  d["run"] = s.running;
  d["pos"] = s.pos;
  d["len"] = s.length;
  d["step_ms"] = s.stepS * 1000;
  d["in_ms"] = s.inStepS * 1000;
  d["tr"] = s.transpose;
}
