#include "Device.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTimer>
#include <QUrl>

#include <limits>

static constexpr int CV_KEEP = 500;  // 10 s of CV at one sample per 20 ms

Device::Device(QObject *parent) : QObject(parent) {
  connect(&link_, &BleLink::message, this, &Device::onMessage);
  connect(&link_, &BleLink::stateChanged, this, [this](BleLink::State s, const QString &info) {
    status_ = info;
    emit statusChanged();
    if (s == BleLink::State::Connected) {
      // Give the ESP our clock (it has none without internet), then fetch everything.
      post(QStringLiteral("/api/time?t=%1").arg(QDateTime::currentSecsSinceEpoch()));
      refreshAll();
    } else {
      pending_.clear();  // answers to old requests will never come
      // cvNext_ stays: after a reconnect the ESP goes on counting, so the time we were away
      // shows as a gap in the plot (see onCv).
    }
  });
}

void Device::useDemo() {
  demo_ = new DemoEsp(this);  // `this` as parent: Qt deletes it together with the Device
  connect(demo_, &DemoEsp::message, this, &Device::onMessage);
}

void Device::start() {
  if (!demo_) {
    link_.start();
    return;
  }
  status_ = tr("demo mode (no Bluetooth)");
  emit statusChanged();
  refreshAll();
}

// ---------------------------------------------------------------------------------------------
// Request / reply
// ---------------------------------------------------------------------------------------------

void Device::request(const QString &method, const QString &path, const QByteArray &body, Reply done) {
  const int id = nextId_++;
  if (done) pending_.insert(id, std::move(done));
  if (demo_) {  // answer a little later, like a real reply over the air
    const QJsonObject r = demo_->handle(method, path, body);
    QTimer::singleShot(15, this, [this, id, r] { onMessage({{"type", "reply"}, {"id", id}, {"status", r["status"]}, {"body", r["body"]}}); });
    return;
  }
  QJsonObject o{{"id", id}, {"m", method}, {"p", path}};
  if (!body.isEmpty()) o.insert("b", QString::fromUtf8(body));
  link_.send(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

void Device::onMessage(const QJsonObject &m) {
  const QString type = m.value("type").toString();
  if (type == QLatin1String("reply")) {
    Reply done = pending_.take(m.value("id").toInt());
    if (done) done(m.value("status").toInt(), m.value("body"));
  } else if (type == QLatin1String("live")) {
    live_ = m.toVariantMap();
    emit liveChanged();
  } else if (type == QLatin1String("seq")) {
    seqState_ = m.toVariantMap();
    seqStamp_ = QDateTime::currentMSecsSinceEpoch();
    emit seqStateChanged();
  } else if (type == QLatin1String("cv")) {
    onCv(m);
  } else if (type == QLatin1String("notes")) {
    notes_.append(m.value("list").toArray().toVariantList());
    if (notes_.size() > 32) notes_ = notes_.mid(notes_.size() - 32);
    emit notesChanged();
  }
}

// "cv": {"i":1234,"dt":20,"lfo":[..],"env":[..],"voct":[..]}. "i" is the ESP's running number of
// the first sample. Samples are put at their own index: a hole (a lost message, or the time the
// link was down) becomes NaN in the lists, so the plot leaves a gap there instead of gluing two
// different moments together (that made the jump in the sine).
void Device::onCv(const QJsonObject &m) {
  static const char *keys[3] = {"lfo", "env", "voct"};
  QVariantList lists[3];
  for (int k = 0; k < 3; k++) lists[k] = cv_.value(keys[k]).toList();
  QJsonArray in[3];
  for (int k = 0; k < 3; k++) in[k] = m.value(keys[k]).toArray();
  int n = in[0].size(), skip = 0;
  const QJsonValue iv = m.value("i");
  if (iv.isDouble() && cvNext_ >= 0) {  // an older ESP sends no "i": then just append
    const qint64 i = qint64(iv.toDouble());
    if (i > cvNext_) {  // a hole: fill it with NaN (at most one screen)
      const int gap = int(qMin<qint64>(i - cvNext_, CV_KEEP));
      for (auto &l : lists)
        for (int g = 0; g < gap; g++) l.append(std::numeric_limits<double>::quiet_NaN());
    } else if (i < cvNext_ && cvNext_ - i < n) {
      skip = int(cvNext_ - i);  // overlap: drop what we already have
    } else if (i < cvNext_) {  // far back: the ESP restarted, start over
      for (auto &l : lists) l.clear();
    }
  }
  if (iv.isDouble()) cvNext_ = qint64(iv.toDouble()) + n;
  else cvNext_ = qMax<qint64>(cvNext_, 0) + n;
  for (int k = 0; k < 3; k++) {
    for (int j = skip; j < in[k].size(); j++) lists[k].append(in[k].at(j).toDouble());
    if (lists[k].size() > CV_KEEP) lists[k] = lists[k].mid(lists[k].size() - CV_KEEP);
    cv_.insert(keys[k], lists[k]);
  }
  cv_.insert("end", double(cvNext_));  // index after the newest sample: the plot scrolls on it
  cv_.insert("dt", m.value("dt").toInt(20));
  emit cvChanged();
}

// ---------------------------------------------------------------------------------------------
// Loading the state
// ---------------------------------------------------------------------------------------------

void Device::refreshAll() {
  loadSettings();
  loadPattern();
  loadPatches();
}

// /api/settings: {"lfo1":{"type":"lfo","shape":"sine","params":[{name,unit,min,max,value}]},..}
// flattened for QML: settings.lfo1.rate = 0.5, ranges["lfo1.rate"] = {min,max,unit}
void Device::loadSettings() {
  request("GET", "/api/settings", {}, [this](int status, const QJsonValue &body) {
    if (status != 200) return;
    settings_.clear();
    ranges_.clear();
    const QJsonObject all = body.toObject();
    for (auto it = all.begin(); it != all.end(); ++it) {
      const QJsonObject mod = it.value().toObject();
      QVariantMap values;
      if (mod.contains("shape")) values.insert("shape", mod.value("shape").toString());
      for (const QJsonValue &pv : mod.value("params").toArray()) {
        const QJsonObject p = pv.toObject();
        values.insert(p.value("name").toString(), p.value("value").toDouble());
        ranges_.insert(it.key() + "." + p.value("name").toString(),
                       QVariantMap{{"min", p.value("min").toDouble()}, {"max", p.value("max").toDouble()},
                                   {"unit", p.value("unit").toString()}});
      }
      settings_.insert(it.key(), values);
    }
    emit settingsChanged();
  });
}

void Device::loadPattern() {
  request("GET", "/api/seq", {}, [this](int status, const QJsonValue &body) {
    if (status != 200) return;
    pattern_ = body.toObject().toVariantMap();
    emit patternChanged();
  });
}

void Device::loadPatches() {
  request("GET", "/api/patches", {}, [this](int status, const QJsonValue &body) {
    if (status != 200) return;
    patches_ = body.toArray().toVariantList();
    emit patchesChanged();
  });
}

void Device::loadHistory(double hours) {
  const QString path = hours <= 0 ? QStringLiteral("/api/history")
                                  : QStringLiteral("/api/log?hours=%1&points=400").arg(hours);
  request("GET", path, {}, [this](int status, const QJsonValue &body) {
    if (status != 200) return;
    history_ = body.toObject().toVariantMap();
    emit historyChanged();
  });
}

// ---------------------------------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------------------------------

void Device::setParam(const QString &target, const QString &name, double value) {
  QVariantMap mod = settings_.value(target).toMap();  // update our copy at once (no flicker)
  mod.insert(name, value);
  settings_.insert(target, mod);
  emit settingsChanged();
  post(QStringLiteral("/api/set?target=%1&name=%2&value=%3").arg(target, name).arg(value, 0, 'f', 4));
}

void Device::setShape(const QString &target, const QString &shape) {
  QVariantMap mod = settings_.value(target).toMap();
  mod.insert("shape", shape);
  settings_.insert(target, mod);
  emit settingsChanged();
  post(QStringLiteral("/api/shape?target=%1&shape=%2").arg(target, shape));
}

void Device::gate(bool on) { post(QStringLiteral("/api/gate?on=%1").arg(on ? 1 : 0)); }

// tone=1: the ESP also plays the note on the test tone (GPIO 17), like the web page's default,
// so with 17 jumpered to 13 the pitch tracker (Monitor tab) has something to hear.
void Device::playNote(int note, bool on) {
  post(QStringLiteral("/api/note?n=%1&on=%2&tone=1").arg(note).arg(on ? 1 : 0));
}

void Device::seqRun(bool on) { post(QStringLiteral("/api/seq/run?on=%1").arg(on ? 1 : 0)); }

void Device::seqTrig(int note) {
  post(note < 0 ? QStringLiteral("/api/seq/trig") : QStringLiteral("/api/seq/trig?note=%1").arg(note));
}

void Device::seqSet(const QVariantMap &changes) {
  for (auto it = changes.begin(); it != changes.end(); ++it) pattern_.insert(it.key(), it.value());
  emit patternChanged();
  request("POST", "/api/seq", QJsonDocument(QJsonObject::fromVariantMap(changes)).toJson(QJsonDocument::Compact));
}

void Device::loadPatch(int slot) {
  post(QStringLiteral("/api/patch/load?slot=%1").arg(slot), [this, slot](int status, const QJsonValue &) {
    if (status != 200) {
      emit info(tr("slot is empty"));
      return;
    }
    loadSettings();  // the engine has the new values now
    loadPattern();
    emit info(tr("loaded %1").arg(patches_.value(slot).toMap().value("label").toString()));
  });
}

void Device::savePatch(int slot, const QString &name) {
  const QString enc = QString::fromUtf8(QUrl::toPercentEncoding(name));
  post(QStringLiteral("/api/patch/save?slot=%1&name=%2").arg(slot).arg(enc), [this](int status, const QJsonValue &) {
    emit info(status == 200 ? tr("saved") : tr("save failed"));
    loadPatches();
  });
}

void Device::deletePatch(int slot) {
  post(QStringLiteral("/api/patch/delete?slot=%1").arg(slot), [this](int, const QJsonValue &) { loadPatches(); });
}

QString Device::noteName(int midi) const {
  static const char *names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
  if (midi < 0) return QStringLiteral("-");
  return QString::fromLatin1(names[midi % 12]) + QString::number(midi / 12 - 1);
}
