// BleLink.h - the Bluetooth Low Energy connection to the SynthPatch (the "how bytes travel").
//
// It finds the module (it advertises our service UUID), connects, switches on notifications and
// then only does two things:
//   * send(line)        write one JSON line to the RX characteristic (cut into MTU-sized pieces)
//   * message(json)     emitted for every complete JSON line the ESP notifies on TX
// What the messages MEAN is Device's job (Device.h). Same protocol as firmware/prototype/src/BleLink.h.
//
// Qt lesson: SIGNALS AND SLOTS + an async state machine. Bluetooth is slow and asynchronous:
// every step (scan, connect, discover services, discover details) finishes later with a signal.
// We connect a lambda to each signal that starts the next step. Nothing blocks the GUI.
#pragma once

#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothUuid>
#include <QJsonObject>
#include <QLowEnergyController>
#include <QLowEnergyService>
#include <QObject>
#include <QPointer>
#include <QTimer>

class BleLink : public QObject {
  Q_OBJECT
 public:
  enum class State { Idle, Scanning, Connecting, Connected };
  Q_ENUM(State)

  explicit BleLink(QObject *parent = nullptr);
  void start();                         // scan and connect (asks for the permission on Android)
  void disconnectDevice();
  void send(const QByteArray &line);    // one JSON message, without '\n'
  State state() const { return state_; }
  QString deviceName() const { return name_; }

 signals:
  void stateChanged(BleLink::State state, const QString &info);
  void message(const QJsonObject &msg);

 private:
  void scan();
  void connectTo(const QBluetoothDeviceInfo &info);
  void setupService();
  void setState(State s, const QString &info = {});
  void onBytes(const QByteArray &bytes);

  QBluetoothDeviceDiscoveryAgent agent_;
  QPointer<QLowEnergyController> ctl_;   // QPointer: becomes null by itself when deleted
  QPointer<QLowEnergyService> svc_;
  QLowEnergyCharacteristic rx_, tx_;
  QByteArray rxBuf_;
  State state_ = State::Idle;
  QString name_;
  QTimer retry_;                         // reconnect 2 s after the link drops
};
