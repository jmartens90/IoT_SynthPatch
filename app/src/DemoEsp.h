// DemoEsp.h - a pretend SynthPatch inside the app, for trying the pages without the hardware.
//
// Start the app with --demo (or set SYNTHPATCH_DEMO=1) and Device talks to this object instead
// of Bluetooth. It answers the same /api/... requests and pushes the same live/seq/cv/notes
// messages as the ESP (firmware/prototype/src/Api.cpp), with made-up sensor values.
//
// C++ lesson: the same INTERFACE twice. Device does not care who answers: the real ESP over
// BleLink, or this class. Both take a request and hand back JSON. That is also how you can test
// app code on a PC long before the hardware works.
#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QTimer>

class DemoEsp : public QObject {
  Q_OBJECT
 public:
  explicit DemoEsp(QObject *parent = nullptr);
  // One request -> {status, body}. Same paths and JSON as the firmware.
  QJsonObject handle(const QString &method, const QString &pathAndQuery, const QByteArray &body);

 signals:
  void message(const QJsonObject &msg);  // pushes, like the ESP's notifications

 private:
  void tick20ms();
  QJsonObject seqMsg() const;
  double lfoAt(double t) const;

  QJsonObject settings_;  // /api/settings format
  QJsonObject pattern_;   // /api/seq format
  QJsonArray patches_;
  QTimer timer_;
  qint64 t0_;             // ms when the demo started
  bool run_ = false;
  qint64 runStart_ = 0;   // ms when step 1 started
  int transpose_ = 0;
  int lastPos_ = -1, noteN_ = 0, curNote_ = 48, ticks_ = 0;
  double env_ = 0;
  bool gate_ = false;
  qint64 gateOff_ = 0;
  QJsonArray cv_[3], notes_;
  qint64 cvIndex_ = 0;  // running number of the next CV sample, like the ESP's "i"
};
