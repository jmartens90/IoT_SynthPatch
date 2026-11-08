// BleLink.h - Bluetooth Low Energy link for the Android (Qt) app.
//
// One BLE "service" with two "characteristics" (think: two mailboxes):
//
//   RX  6e5a0002-...  the app WRITES requests here
//   TX  6e5a0003-...  the ESP NOTIFIES the app here (answers + pushes)
//
// Everything is text: one JSON message per line ('\n' at the end). A BLE packet only carries
// MTU-3 bytes (20 to 244), so a long message is cut into pieces and the app glues them back
// together until it sees the '\n'.
//
//   app -> ESP   {"id":7,"m":"GET","p":"/api/settings"}
//                {"id":8,"m":"POST","p":"/api/set?target=lfo1&name=rate&value=2.5"}
//                {"id":9,"m":"POST","p":"/api/seq","b":"{\"bpm\":140}"}
//   ESP -> app   {"type":"reply","id":7,"status":200,"body":{...}}   (body: JSON, or a string)
//                {"type":"live",...} {"type":"seq",...} {"type":"cv",...} {"type":"notes",...}
//
// The requests are the same /api/... paths as the web page uses, handled by the same Api.
// WiFi and BLE run at the same time (the ESP32-S3 shares its radio between them).
//
// C++ lesson: CALLBACK CLASSES. NimBLE calls our code from its own task when something happens
// (a phone connects, writes...). We derive from its callback classes and override the virtual
// functions we care about. Those callbacks only put data in a queue; the real work happens in
// loop(), so no two tasks touch the Api at the same time.
#pragma once

#include <Arduino.h>

#include <deque>
#include <mutex>
#include <string>

#include "Api.h"

class NimBLECharacteristic;

class BleLink {
 public:
  explicit BleLink(Api &api) : api_(api) {}
  void begin(const char *name);
  void loop();  // from Arduino loop(): handle requests, push, send queued pieces
  bool connected() const { return connected_; }

  // Called by the NimBLE callbacks (NimBLE's own task).
  void onConnect(uint16_t connHandle);
  void onSubscribe(bool on);
  void onDisconnect();
  void onMtu(uint16_t mtu) { mtu_ = mtu; }
  void onWrite(const std::string &data);

 private:
  void handleLine(const std::string &line);
  void send(const String &msg);  // queue one message (adds '\n')
  void drain();                  // send a few queued pieces

  Api &api_;
  NimBLECharacteristic *tx_ = nullptr;
  Api::PushState push_;

  std::mutex m_;                 // protects the fields written by the NimBLE task:
  std::string rxBuf_;            //   bytes received, not yet a full line
  std::deque<std::string> rxLines_;
  volatile bool connected_ = false, subscribed_ = false, justConnected_ = false;
  volatile uint16_t connHandle_ = 0;
  volatile uint16_t mtu_ = 23;   // BLE default; the phone usually asks for more

  std::string txBuf_;            // bytes waiting to be notified (loop task only)
  uint32_t lastDrainMs_ = 0;
};
