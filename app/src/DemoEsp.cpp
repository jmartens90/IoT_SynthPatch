#include "DemoEsp.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QUrlQuery>
#include <QtMath>

static qint64 nowMs() { return QDateTime::currentMSecsSinceEpoch(); }

static QJsonObject param(const char *name, const char *unit, double min, double max, double value) {
  return {{"name", name}, {"unit", unit}, {"min", min}, {"max", max}, {"value", value}};
}

DemoEsp::DemoEsp(QObject *parent) : QObject(parent), t0_(nowMs()) {
  settings_ = {
      {"lfo1", QJsonObject{{"type", "lfo"}, {"shape", "sine"},
                           {"params", QJsonArray{param("rate", "Hz", 0.01, 50, 0.8), param("depth", "V", 0, 1.65, 1.2),
                                                 param("offset", "V", 0, 3.3, 1.65)}}}},
      {"env1", QJsonObject{{"type", "env"},
                           {"params", QJsonArray{param("attack", "s", 0.001, 10, 0.01), param("decay", "s", 0.001, 10, 0.15),
                                                 param("sustain", "", 0, 1, 0.6), param("release", "s", 0.001, 10, 0.2),
                                                 param("peak", "V", 0, 3.3, 3.3)}}}}};
  QJsonArray steps;
  const int melody[16] = {48, 55, 60, 63, 60, 55, 51, 53, 48, 55, 60, 65, 63, 60, 58, 55};
  for (int i = 0; i < 16; i++) steps.append(QJsonObject{{"n", melody[i]}, {"on", i % 8 != 6}});
  pattern_ = {{"bpm", 120}, {"gate", 0.5}, {"mode", "retrig"}, {"length", 8}, {"steps", steps}};
  for (int s = 0; s < 32; s++) {
    QJsonValue name = s == 0 ? QJsonValue("Slow wobble") : s == 1 ? QJsonValue("Bass arp") : s == 9 ? QJsonValue("Drone")
                                                                                                    : QJsonValue();
    patches_.append(QJsonObject{{"slot", s}, {"label", QString("%1%2").arg(QChar('A' + s / 8)).arg(s % 8 + 1)}, {"name", name}});
  }
  run_ = true;  // the demo starts playing, so the running light and the CV scope move
  runStart_ = nowMs();
  connect(&timer_, &QTimer::timeout, this, &DemoEsp::tick20ms);
  timer_.start(20);
}

double DemoEsp::lfoAt(double t) const {
  QJsonObject lfo;
  for (const QJsonValue &p : settings_["lfo1"].toObject()["params"].toArray()) lfo[p["name"].toString()] = p["value"];
  const double x = std::fmod(t * lfo["rate"].toDouble(), 1.0);
  const QString shape = settings_["lfo1"].toObject()["shape"].toString();
  const double w = shape == "triangle" ? (x < .25 ? 4 * x : x < .75 ? 2 - 4 * x : 4 * x - 4)
                   : shape == "saw"    ? 2 * x - 1
                   : shape == "square" ? (x < .5 ? 1 : -1)
                                       : std::sin(2 * M_PI * x);
  return std::clamp(lfo["offset"].toDouble() + lfo["depth"].toDouble() * w, 0.0, 3.3);  // the DAC clips at 0 and 3.3 V
}

QJsonObject DemoEsp::seqMsg() const {
  const double stepMs = 60000.0 / pattern_["bpm"].toDouble() / 4;
  const int len = pattern_["length"].toInt();
  const double el = nowMs() - runStart_;
  const int pos = run_ ? int(el / stepMs) % len : -1;
  return {{"type", "seq"}, {"run", run_}, {"pos", pos}, {"len", len}, {"step_ms", stepMs},
          {"in_ms", run_ ? std::fmod(el, stepMs) : 0.0}, {"tr", transpose_}};
}

// The 1 kHz engine of the real ESP, at 50 Hz: sequencer, gate, envelope, CV scope, pushes.
void DemoEsp::tick20ms() {
  const qint64 now = nowMs();
  if (run_) {
    const QJsonObject s = seqMsg();
    const int pos = s["pos"].toInt();
    if (pos != lastPos_) {
      lastPos_ = pos;
      const QJsonObject st = pattern_["steps"].toArray()[pos].toObject();
      const bool legato = pattern_["mode"] == "legato";
      if (st["on"].toBool()) {
        curNote_ = std::clamp(st["n"].toInt() + transpose_, 0, 127);
        const double v = (curNote_ - 36) / 12.0;
        notes_ = {QJsonObject{{"n", ++noteN_}, {"ms", now - t0_}, {"note", curNote_}, {"v", v},
                              {"code", int(v / 5.0 * 65535)}, {"src", "seq"}}};
        emit message({{"type", "notes"}, {"list", notes_}});
        if (!legato) gate_ = true, gateOff_ = now + qint64(s["step_ms"].toDouble() * pattern_["gate"].toDouble());
      }
      if (legato && pos == 0) gate_ = true, gateOff_ = now + qint64(s["step_ms"].toDouble() * (s["len"].toInt() - .05));
    }
    if (gate_ && now >= gateOff_) gate_ = false;
  }
  env_ += ((gate_ ? 2.6 : 0.0) - env_) * (gate_ ? 0.6 : 0.25);  // quick attack, short release
  const double t = (now - t0_) / 1000.0;
  cv_[0].append(qRound(lfoAt(t) * 1000) / 1000.0);
  cv_[1].append(qRound(env_ * 1000) / 1000.0);
  cv_[2].append((curNote_ - 36) / 12.0);
  if (cv_[0].size() >= 10) {
    emit message({{"type", "cv"}, {"dt", 20}, {"lfo", cv_[0]}, {"env", cv_[1]}, {"voct", cv_[2]}});
    for (auto &c : cv_) c = QJsonArray();
  }
  if (ticks_ % 250 == 0) emit message(seqMsg());  // at the start and every 5 s, like the ESP
  if (++ticks_ % 50 == 0) {  // once per second
    emit message({{"type", "live"}, {"temp_c", 21.4 + std::sin(t / 60) * 0.3}, {"temp_sensor", "DHT11"},
                  {"rh", 48}, {"hz", 261.6 + std::sin(t / 20)}, {"note", 60}, {"cents_off", std::sin(t / 20) * 6},
                  {"drift", std::sin(t / 20) * 6}, {"drift_per_min", 0.4}, {"pitch_state", "stable"},
                  {"volts", 1.23}, {"lfo_v", lfoAt(t)}, {"env_v", env_}, {"env_out", gate_ ? "sustain" : "idle"},
                  {"voct_v", (curNote_ - 36) / 12.0}, {"voct_note", curNote_}, {"uptime", qRound(t)},
                  {"time", QDateTime::currentSecsSinceEpoch()}, {"log_count", 52840}, {"wifi", "demo"}, {"rssi", -55},
                  {"mqtt", false}});
  }
}

QJsonObject DemoEsp::handle(const QString &method, const QString &pathAndQuery, const QByteArray &body) {
  const QUrl url(pathAndQuery);
  const QString p = url.path();
  const QUrlQuery q(url);
  auto ok = [](const QJsonValue &b = "ok") { return QJsonObject{{"status", 200}, {"body", b}}; };
  const bool post = method == "POST";

  if (!post && p == "/api/settings") return ok(settings_);
  if (!post && p == "/api/patches") return ok(patches_);
  if (!post && p == "/api/seq") return ok(pattern_);
  if (!post && (p == "/api/history" || p == "/api/log")) {
    const double hours = q.queryItemValue("hours").toDouble();
    const int n = p == "/api/history" ? 600 : 400;
    const qint64 end = p == "/api/history" ? (nowMs() - t0_) / 1000 + 600 : QDateTime::currentSecsSinceEpoch();
    const double span = p == "/api/history" ? 600 : hours * 3600;
    QJsonArray t, temp, rh, hz, drift, volts;
    for (int i = 0; i < n; i++) {
      const double ti = end - span + span * i / (n - 1), day = std::sin(ti / 86400 * 2 * M_PI);
      t.append(qint64(ti));
      temp.append(21 + 2.5 * day + 0.2 * std::sin(i * 0.05));
      rh.append(50 - 8 * day);
      hz.append(261.6 + 0.8 * day);
      drift.append(5.3 * day + std::sin(i * 0.02));
      volts.append(1.2 + 0.05 * std::sin(i * 0.05));
    }
    return ok(QJsonObject{{"t", t}, {"temp", temp}, {"rh", rh}, {"hz", hz}, {"drift", drift}, {"volts", volts}});
  }
  if (post && p == "/api/set") {
    QJsonObject mod = settings_[q.queryItemValue("target")].toObject();
    QJsonArray params = mod["params"].toArray();
    for (int i = 0; i < params.size(); i++) {
      QJsonObject pr = params[i].toObject();
      if (pr["name"] == q.queryItemValue("name")) pr["value"] = q.queryItemValue("value").toDouble(), params[i] = pr;
    }
    mod["params"] = params;
    settings_[q.queryItemValue("target")] = mod;
    return ok();
  }
  if (post && p == "/api/shape") {
    QJsonObject mod = settings_[q.queryItemValue("target")].toObject();
    mod["shape"] = q.queryItemValue("shape");
    settings_[q.queryItemValue("target")] = mod;
    return ok();
  }
  if (post && p == "/api/seq") {
    const QJsonObject ch = QJsonDocument::fromJson(body).object();
    const QJsonObject before = seqMsg();
    for (auto it = ch.begin(); it != ch.end(); ++it) pattern_[it.key()] = it.value();
    if (run_) runStart_ = nowMs() - qint64(before["pos"].toInt() * seqMsg()["step_ms"].toDouble());  // keep the step
    emit message(seqMsg());
    return ok();
  }
  if (post && (p == "/api/seq/run" || p == "/api/seq/trig")) {
    const bool on = p == "/api/seq/trig" || q.queryItemValue("on") == "1";
    if (q.hasQueryItem("note")) transpose_ = q.queryItemValue("note").toInt() - 48;
    if (on && (!run_ || p == "/api/seq/trig")) runStart_ = nowMs(), lastPos_ = -1;
    run_ = on;
    if (!on) gate_ = false;
    emit message(seqMsg());
    return ok();
  }
  if (post && p == "/api/patch/save") {
    const int s = q.queryItemValue("slot").toInt();
    QJsonObject e = patches_[s].toObject();
    e["name"] = q.queryItemValue("name", QUrl::FullyDecoded);
    patches_[s] = e;
    return ok();
  }
  if (post && p == "/api/patch/delete") {
    const int s = q.queryItemValue("slot").toInt();
    QJsonObject e = patches_[s].toObject();
    e["name"] = QJsonValue();
    patches_[s] = e;
    return ok();
  }
  if (post && p == "/api/patch/load") {
    const bool full = !patches_[q.queryItemValue("slot").toInt()].toObject()["name"].isNull();
    return full ? ok() : QJsonObject{{"status", 404}, {"body", "empty"}};
  }
  if (post && p == "/api/gate") {
    gate_ = q.queryItemValue("on") == "1", gateOff_ = gate_ ? LLONG_MAX : 0;
    return ok();
  }
  if (post && p == "/api/note") {
    if (q.queryItemValue("on") == "1") curNote_ = q.queryItemValue("n").toInt();
    gate_ = q.queryItemValue("on") == "1", gateOff_ = gate_ ? LLONG_MAX : 0;
    return ok();
  }
  if (post) return ok();  // /api/time, /api/tone: nothing to pretend
  return {{"status", 404}, {"body", "not found"}};
}
