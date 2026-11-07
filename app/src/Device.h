// Device.h - the SynthPatch as the QML pages see it (the "what").
//
// QML reads its properties (live values, settings, sequence, patches...) and calls its
// Q_INVOKABLE functions (setParam, seqRun, savePatch...). Underneath, every call becomes one of
// the device's /api/... requests, sent over Bluetooth by BleLink:
//
//   setParam("lfo1", "rate", 2.5)  ->  {"id":12,"m":"POST","p":"/api/set?target=lfo1&name=rate&value=2.5"}
//                                  <-  {"type":"reply","id":12,"status":200,"body":"ok"}
//
// The ESP also pushes on its own: "live" (1 per s), "seq" (when the sequencer changes), "cv"
// (the 3 DAC outputs, 5 packs per s) and "notes" (each V/oct note). Each updates a property, and
// QML redraws whatever is bound to it.
//
// Qt lesson: Q_PROPERTY with a NOTIFY signal is what makes QML bindings work: when the signal
// fires, every QML expression that used the property is re-evaluated by itself.
#pragma once

#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <functional>

#include "BleLink.h"
#include "DemoEsp.h"

class Device : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool connected READ connected NOTIFY statusChanged)
  Q_PROPERTY(QString status READ status NOTIFY statusChanged)
  Q_PROPERTY(QVariantMap live READ live NOTIFY liveChanged)              // /api/live fields
  Q_PROPERTY(QVariantMap settings READ settings NOTIFY settingsChanged)  // {lfo1:{shape,rate,..}, env1:{..}}
  Q_PROPERTY(QVariantMap ranges READ ranges NOTIFY settingsChanged)      // {"lfo1.rate":{min,max,unit}}
  Q_PROPERTY(QVariantMap pattern READ pattern NOTIFY patternChanged)     // {bpm,gate,mode,length,steps:[{n,on}]}
  Q_PROPERTY(QVariantMap seqState READ seqState NOTIFY seqStateChanged)  // {run,pos,len,step_ms,in_ms,tr}
  Q_PROPERTY(double seqStamp READ seqStamp NOTIFY seqStateChanged)       // when seqState arrived (ms)
  Q_PROPERTY(QVariantList patches READ patches NOTIFY patchesChanged)    // [{slot,label,name}]
  Q_PROPERTY(QVariantList notes READ notes NOTIFY notesChanged)          // V/oct note log, newest last
  Q_PROPERTY(QVariantMap cv READ cv NOTIFY cvChanged)                    // {lfo:[..500],env:[..],voct:[..]}
  Q_PROPERTY(QVariantMap history READ history NOTIFY historyChanged)     // {t:[..],temp:[..],..}

 public:
  explicit Device(QObject *parent = nullptr);

  bool connected() const { return demo_ || link_.state() == BleLink::State::Connected; }
  void useDemo();  // talk to a pretend ESP inside the app (see DemoEsp.h), call before start()
  QString status() const { return status_; }
  QVariantMap live() const { return live_; }
  QVariantMap settings() const { return settings_; }
  QVariantMap ranges() const { return ranges_; }
  QVariantMap pattern() const { return pattern_; }
  QVariantMap seqState() const { return seqState_; }
  double seqStamp() const { return seqStamp_; }
  QVariantList patches() const { return patches_; }
  QVariantList notes() const { return notes_; }
  QVariantMap cv() const { return cv_; }
  QVariantMap history() const { return history_; }

  // ---- called from QML ----
  Q_INVOKABLE void start();
  Q_INVOKABLE void setParam(const QString &target, const QString &name, double value);
  Q_INVOKABLE void setShape(const QString &target, const QString &shape);
  Q_INVOKABLE void gate(bool on);
  Q_INVOKABLE void playNote(int note, bool on);
  Q_INVOKABLE void seqRun(bool on);
  Q_INVOKABLE void seqTrig(int note = -1);           // -1 = restart as it is
  Q_INVOKABLE void seqSet(const QVariantMap &changes);  // e.g. {bpm:140} or {steps:[...]}
  Q_INVOKABLE void loadPatch(int slot);
  Q_INVOKABLE void savePatch(int slot, const QString &name);
  Q_INVOKABLE void deletePatch(int slot);
  Q_INVOKABLE void loadHistory(double hours);       // 0 = last 10 minutes (RAM)
  Q_INVOKABLE void refreshAll();
  Q_INVOKABLE QString noteName(int midi) const;

 signals:
  void statusChanged();
  void liveChanged();
  void settingsChanged();
  void patternChanged();
  void seqStateChanged();
  void patchesChanged();
  void notesChanged();
  void cvChanged();
  void historyChanged();
  void info(const QString &text);  // a short message for the user ("saved to A3")

 private:
  using Reply = std::function<void(int status, const QJsonValue &body)>;
  void request(const QString &method, const QString &path, const QByteArray &body = {}, Reply done = {});
  void post(const QString &path, Reply done = {}) { request(QStringLiteral("POST"), path, {}, std::move(done)); }
  void onMessage(const QJsonObject &m);
  void loadSettings();
  void loadPattern();
  void loadPatches();

  BleLink link_;
  DemoEsp *demo_ = nullptr;  // only with --demo
  QString status_ = tr("not connected");
  int nextId_ = 1;
  QHash<int, Reply> pending_;  // request id -> what to do with the answer
  QVariantMap live_, settings_, ranges_, pattern_, seqState_, cv_, history_;
  QVariantList patches_, notes_;
  double seqStamp_ = 0;
};
