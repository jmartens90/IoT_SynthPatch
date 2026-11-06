// DataLog.h - weeks of measurements in the ESP32's own flash (LittleFS), as a ring buffer.
//
// One record every 10 s, 16 bytes each. CAPACITY = 400 000 records = 6.4 MB = ~46 days; when the
// file is full the oldest record is overwritten. The data survives reboots and power cuts.
//
// File layout (/log.bin):
//   [Header 32 bytes][record 0][record 1] ... [record CAPACITY-1]
//   head  = where the NEXT record goes, count = how many records are valid.
//   The oldest record is at (head - count), wrapping around at CAPACITY.
//
// C++ lesson: a PACKED STRUCT as a binary file format (no padding bytes between the fields, so
// it is exactly 16 bytes on disk), fixed-point numbers (temperature * 100 in an int16 instead of
// a 4-byte float), static_assert (checked by the compiler), and binary search over a file.
#pragma once

#include <Arduino.h>
#include <FS.h>

#include <cmath>
#include <functional>

struct __attribute__((packed)) LogRecord {
  uint32_t t;          // unix time, seconds (from NTP)
  int16_t tempC100;    // temperature * 100, NO_VALUE = no reading
  uint16_t rh100;      // humidity * 100, 0xFFFF = no reading
  uint32_t hz100;      // input frequency * 100, 0 = no signal
  int16_t drift100;    // drift in cents * 100
  uint16_t adcMv;      // ADC input in millivolts
};
static_assert(sizeof(LogRecord) == 16, "LogRecord must be 16 bytes on disk");

// The same values as plain floats, for the code that uses them.
struct LogPoint {
  uint32_t t;
  float tempC, rh, hz, drift, volts;  // NAN where there was no reading
};

class DataLog {
 public:
  static constexpr uint32_t CAPACITY = 400000;  // ~46 days at one record per 10 s
  static constexpr int16_t NO_VALUE = INT16_MIN;

  bool begin();  // mount LittleFS and open (or create) the log file
  bool ok() const { return ok_; }
  bool append(const LogPoint &p);
  void clear();

  uint32_t count() const { return h_.count; }
  uint32_t oldestTime();
  uint32_t newestTime();

  // Calls fn for the records with t >= since, oldest first. If there are more than maxPoints,
  // neighbouring records are averaged so that at most maxPoints come out (for charts).
  void forEach(uint32_t since, uint32_t maxPoints, const std::function<void(const LogPoint &)> &fn);

 private:
  struct Header {
    uint32_t magic, version, capacity, head, count;
    uint32_t reserved[3];
  };
  static_assert(sizeof(Header) == 32, "Header must be 32 bytes");
  static constexpr uint32_t MAGIC = 0x53504C47;  // "SPLG"

  bool readAt(uint32_t slot, LogRecord &r);
  uint32_t slotOf(uint32_t i) const { return (h_.head + CAPACITY - h_.count + i) % CAPACITY; }  // i-th oldest
  void writeHeader();
  static LogRecord pack(const LogPoint &p);
  static LogPoint unpack(const LogRecord &r);

  fs::File file_;
  Header h_{};
  bool ok_ = false;
};
