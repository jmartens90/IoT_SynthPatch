#include "BleLink.h"

#include <NimBLEDevice.h>

// Our own random 128-bit UUIDs (any UUID works, as long as the app uses the same ones).
static const char *SERVICE_UUID = "6e5a0001-5b1d-4c2a-9e43-53594e544850";
static const char *RX_UUID = "6e5a0002-5b1d-4c2a-9e43-53594e544850";
static const char *TX_UUID = "6e5a0003-5b1d-4c2a-9e43-53594e544850";

// ---- NimBLE callback classes: they only forward to BleLink ----
class ServerCallbacks : public NimBLEServerCallbacks {
 public:
  explicit ServerCallbacks(BleLink &l) : link_(l) {}
  void onConnect(NimBLEServer *) override { link_.onConnect(); }
  void onDisconnect(NimBLEServer *) override { link_.onDisconnect(); }  // advertising restarts by itself
  void onMTUChange(uint16_t mtu, ble_gap_conn_desc *) override { link_.onMtu(mtu); }

 private:
  BleLink &link_;
};

class RxCallbacks : public NimBLECharacteristicCallbacks {
 public:
  explicit RxCallbacks(BleLink &l) : link_(l) {}
  void onWrite(NimBLECharacteristic *c) override { link_.onWrite(c->getValue()); }

 private:
  BleLink &link_;
};

void BleLink::begin(const char *name) {
  NimBLEDevice::init(name);
  NimBLEDevice::setMTU(247);  // ask for big packets (the phone decides)
  NimBLEServer *server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks(*this));  // NimBLE keeps these for the whole run
  NimBLEService *svc = server->createService(SERVICE_UUID);
  NimBLECharacteristic *rx = svc->createCharacteristic(RX_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  rx->setCallbacks(new RxCallbacks(*this));
  tx_ = svc->createCharacteristic(TX_UUID, NIMBLE_PROPERTY::NOTIFY);
  svc->start();
  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);  // the app scans for this UUID
  adv->setScanResponse(true);         // the name goes in the scan response
  adv->start();
  push_.cvEvery = 1;  // all CV packs: ~300 bytes every 200 ms is fine for BLE
}

void BleLink::onConnect() {
  std::lock_guard<std::mutex> lock(m_);
  connected_ = true;
  justConnected_ = true;
  rxBuf_.clear();
  rxLines_.clear();
}

void BleLink::onDisconnect() {
  std::lock_guard<std::mutex> lock(m_);
  connected_ = false;
  mtu_ = 23;
}

void BleLink::onWrite(const std::string &data) {
  std::lock_guard<std::mutex> lock(m_);
  rxBuf_ += data;
  size_t nl;
  while ((nl = rxBuf_.find('\n')) != std::string::npos) {
    rxLines_.push_back(rxBuf_.substr(0, nl));
    rxBuf_.erase(0, nl + 1);
  }
  if (rxBuf_.size() > 4096) rxBuf_.clear();  // garbage without newline: drop it
}

void BleLink::loop() {
  if (!connected_) {
    txBuf_.clear();
    return;
  }
  bool fresh;
  {
    std::lock_guard<std::mutex> lock(m_);
    fresh = justConnected_;
    justConnected_ = false;
  }
  if (fresh) api_.snapshot(push_, [this](const String &m) { send(m); });

  // Requests (at most a few per loop, so WiFi and the shell stay responsive).
  for (int i = 0; i < 4; i++) {
    std::string line;
    {
      std::lock_guard<std::mutex> lock(m_);
      if (rxLines_.empty()) break;
      line = rxLines_.front();
      rxLines_.pop_front();
    }
    handleLine(line);
  }

  // Pushes, unless the queue is still full of old ones (a slow link: skip, don't pile up).
  if (txBuf_.size() < 2000) api_.collect(push_, [this](const String &m) { send(m); });
  drain();
}

void BleLink::handleLine(const std::string &line) {
  JsonDocument in;
  if (deserializeJson(in, line)) return;
  ApiRequest r = ApiRequest::parse(strcmp(in["m"] | "GET", "POST") == 0, in["p"] | "", in["b"] | "");
  r.source = "ble";
  ApiReply rep = api_.handle(r);

  JsonDocument out;
  out["type"] = "reply";
  out["id"] = in["id"];
  out["status"] = rep.status;
  // A JSON body is inserted as JSON (not as a string with escaped quotes): serialized() says
  // "this text is already JSON, copy it as it is".
  if (strcmp(rep.type, "application/json") == 0) out["body"] = serialized(rep.body);
  else out["body"] = rep.body;
  String s;
  serializeJson(out, s);
  send(s);
}

void BleLink::send(const String &msg) {
  txBuf_ += msg.c_str();
  txBuf_ += '\n';
}

// Notify a few pieces every 10 ms. NimBLE has a small number of packet buffers: sending
// everything at once would drop pieces. ~4 x 244 bytes / 10 ms = ~90 kB/s at most.
void BleLink::drain() {
  uint32_t now = millis();
  if (txBuf_.empty() || now - lastDrainMs_ < 10) return;
  lastDrainMs_ = now;
  const size_t piece = mtu_ > 3 ? mtu_ - 3 : 20;
  for (int i = 0; i < 4 && !txBuf_.empty(); i++) {
    size_t n = std::min(piece, txBuf_.size());
    tx_->notify(reinterpret_cast<const uint8_t *>(txBuf_.data()), n);
    txBuf_.erase(0, n);
  }
}
