#include "BleLink.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QLowEnergyDescriptor>
#include <QtGlobal>

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QPermissions>
#endif

// Must match firmware/prototype/src/BleLink.cpp
static const QBluetoothUuid SERVICE(QStringLiteral("6e5a0001-5b1d-4c2a-9e43-53594e544850"));
static const QBluetoothUuid RX(QStringLiteral("6e5a0002-5b1d-4c2a-9e43-53594e544850"));
static const QBluetoothUuid TX(QStringLiteral("6e5a0003-5b1d-4c2a-9e43-53594e544850"));

BleLink::BleLink(QObject *parent) : QObject(parent) {
  agent_.setLowEnergyDiscoveryTimeout(10000);  // scan 10 s, then try again

  // A device was found: is it ours? (our service UUID in its advertisement, or the name)
  connect(&agent_, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered, this, [this](const QBluetoothDeviceInfo &info) {
    if (state_ != State::Scanning) return;
    if (info.serviceUuids().contains(SERVICE) || info.name() == QLatin1String("SynthPatch")) {
      agent_.stop();
      connectTo(info);
    }
  });
  connect(&agent_, &QBluetoothDeviceDiscoveryAgent::finished, this, [this] {
    if (state_ == State::Scanning) retry_.start(500);  // nothing found: scan again
  });
  connect(&agent_, &QBluetoothDeviceDiscoveryAgent::errorOccurred, this,
          [this](QBluetoothDeviceDiscoveryAgent::Error) {
            setState(State::Idle, tr("Bluetooth: %1").arg(agent_.errorString()));
            retry_.start(3000);
          });

  retry_.setSingleShot(true);
  connect(&retry_, &QTimer::timeout, this, &BleLink::scan);
}

void BleLink::start() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  // Android 12+ asks the user "allow SynthPatch to find nearby devices?" once.
  QBluetoothPermission perm;
  perm.setCommunicationModes(QBluetoothPermission::Access);
  switch (qApp->checkPermission(perm)) {
    case Qt::PermissionStatus::Undetermined:
      qApp->requestPermission(perm, this, [this](const QPermission &) { start(); });
      return;
    case Qt::PermissionStatus::Denied:
      setState(State::Idle, tr("Bluetooth permission denied (Android settings > Apps > SynthPatch)"));
      return;
    case Qt::PermissionStatus::Granted:
      break;
  }
#endif
  scan();
}

void BleLink::scan() {
  if (state_ == State::Connected || state_ == State::Connecting) return;
  setState(State::Scanning, tr("Bluetooth: looking for SynthPatch…"));
  agent_.start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
}

void BleLink::connectTo(const QBluetoothDeviceInfo &info) {
  name_ = info.name();
  setState(State::Connecting, tr("connecting to %1…").arg(name_));
  if (ctl_) ctl_->deleteLater();
  ctl_ = QLowEnergyController::createCentral(info, this);

  connect(ctl_, &QLowEnergyController::connected, ctl_, &QLowEnergyController::discoverServices);
  connect(ctl_, &QLowEnergyController::discoveryFinished, this, &BleLink::setupService);
  connect(ctl_, &QLowEnergyController::disconnected, this, [this] {
    setState(State::Idle, tr("connection lost, reconnecting…"));
    retry_.start(2000);
  });
  connect(ctl_, &QLowEnergyController::errorOccurred, this, [this](QLowEnergyController::Error) {
    setState(State::Idle, tr("error: %1").arg(ctl_ ? ctl_->errorString() : QString()));
    retry_.start(3000);
  });
  ctl_->connectToDevice();
}

void BleLink::setupService() {
  svc_ = ctl_->createServiceObject(SERVICE, ctl_);
  if (!svc_) {
    setState(State::Idle, tr("this device has no SynthPatch service"));
    return;
  }
  connect(svc_, &QLowEnergyService::stateChanged, this, [this](QLowEnergyService::ServiceState s) {
    if (s != QLowEnergyService::RemoteServiceDiscovered) return;
    rx_ = svc_->characteristic(RX);
    tx_ = svc_->characteristic(TX);
    // Switch notifications on: write 0x0001 into TX's "client characteristic configuration".
    QLowEnergyDescriptor cccd = tx_.descriptor(QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration);
    svc_->writeDescriptor(cccd, QLowEnergyCharacteristic::CCCDEnableNotification);
    rxBuf_.clear();
    setState(State::Connected, tr("Bluetooth: connected to %1").arg(name_));
  });
  connect(svc_, &QLowEnergyService::characteristicChanged, this,
          [this](const QLowEnergyCharacteristic &, const QByteArray &v) { onBytes(v); });
  svc_->discoverDetails();
}

// Glue the pieces together and cut at '\n': every line is one JSON message.
void BleLink::onBytes(const QByteArray &bytes) {
  rxBuf_ += bytes;
  int nl;
  while ((nl = rxBuf_.indexOf('\n')) >= 0) {
    const QByteArray line = rxBuf_.left(nl);
    rxBuf_.remove(0, nl + 1);
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(line, &err);
    if (err.error == QJsonParseError::NoError && doc.isObject()) emit message(doc.object());
  }
  if (rxBuf_.size() > 200000) rxBuf_.clear();  // lost a newline somewhere: start over
}

void BleLink::send(const QByteArray &line) {
  if (state_ != State::Connected || !svc_) return;
  const QByteArray data = line + '\n';
  const int piece = qMax(20, (ctl_ ? ctl_->mtu() : 23) - 3);
  for (int i = 0; i < data.size(); i += piece)
    svc_->writeCharacteristic(rx_, data.mid(i, piece), QLowEnergyService::WriteWithoutResponse);
}

void BleLink::disconnectDevice() {
  retry_.stop();
  if (ctl_) ctl_->disconnectFromDevice();
  setState(State::Idle, tr("disconnected"));
}

void BleLink::setState(State s, const QString &info) {
  state_ = s;
  emit stateChanged(s, info);
}
