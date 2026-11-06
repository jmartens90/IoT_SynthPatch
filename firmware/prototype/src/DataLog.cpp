#include "DataLog.h"

#include <LittleFS.h>

#include <algorithm>

static const char *PATH = "/log.bin";

bool DataLog::begin() {
  // true = format the partition if it can't be mounted (first boot after the new flash layout)
  if (!LittleFS.begin(true)) return false;

  if (LittleFS.exists(PATH)) {
    file_ = LittleFS.open(PATH, "r+");  // read + write, keeps the contents
    if (file_ && file_.read(reinterpret_cast<uint8_t *>(&h_), sizeof(h_)) == sizeof(h_) &&
        h_.magic == MAGIC && h_.capacity == CAPACITY) {
      return ok_ = true;
    }
    if (file_) file_.close();
  }
  // No (valid) log yet: start an empty one.
  file_ = LittleFS.open(PATH, "w+");
  if (!file_) return false;
  h_ = Header{MAGIC, 1, CAPACITY, 0, 0, {}};
  writeHeader();
  return ok_ = true;
}

void DataLog::clear() {
  if (!ok_) return;
  file_.close();
  LittleFS.remove(PATH);
  begin();
}

void DataLog::writeHeader() {
  file_.seek(0);
  file_.write(reinterpret_cast<const uint8_t *>(&h_), sizeof(h_));
}

bool DataLog::append(const LogPoint &p) {
  if (!ok_) return false;
  LogRecord r = pack(p);
  file_.seek(sizeof(Header) + h_.head * sizeof(LogRecord));
  if (file_.write(reinterpret_cast<const uint8_t *>(&r), sizeof(r)) != sizeof(r)) return false;
  h_.head = (h_.head + 1) % CAPACITY;
  if (h_.count < CAPACITY) h_.count++;
  writeHeader();
  file_.flush();  // really write it now, so a power cut loses at most this one record
  return true;
}

bool DataLog::readAt(uint32_t slot, LogRecord &r) {
  file_.seek(sizeof(Header) + slot * sizeof(LogRecord));
  return file_.read(reinterpret_cast<uint8_t *>(&r), sizeof(r)) == sizeof(r);
}

uint32_t DataLog::oldestTime() {
  LogRecord r;
  return h_.count && readAt(slotOf(0), r) ? r.t : 0;
}

uint32_t DataLog::newestTime() {
  LogRecord r;
  return h_.count && readAt(slotOf(h_.count - 1), r) ? r.t : 0;
}

void DataLog::forEach(uint32_t since, uint32_t maxPoints, const std::function<void(const LogPoint &)> &fn) {
  if (!ok_ || h_.count == 0 || maxPoints == 0) return;

  // Binary search for the first record with t >= since. Records are in time order, so we can
  // halve the range each step: 400 000 records need only ~19 reads instead of 400 000.
  uint32_t lo = 0, hi = h_.count;
  while (lo < hi) {
    uint32_t mid = lo + (hi - lo) / 2;
    LogRecord r;
    if (readAt(slotOf(mid), r) && r.t < since) lo = mid + 1;
    else hi = mid;
  }
  const uint32_t n = h_.count - lo;
  const uint32_t step = (n + maxPoints - 1) / maxPoints;  // records per output point (rounded up)

  // Average `step` records into one point. Sums and counts per field, so a missing value in
  // one record doesn't pull the average to zero.
  double sum[5] = {};
  uint32_t cnt[5] = {}, inBucket = 0, tLast = 0;
  auto flush = [&]() {
    LogPoint p{tLast, NAN, NAN, NAN, NAN, NAN};
    float *f[5] = {&p.tempC, &p.rh, &p.hz, &p.drift, &p.volts};
    for (int k = 0; k < 5; k++) {
      if (cnt[k]) *f[k] = sum[k] / cnt[k];
      sum[k] = 0;
      cnt[k] = 0;
    }
    inBucket = 0;
    fn(p);
  };

  // Read in blocks of 256 records (4 kB) instead of one by one: one seek per block makes a
  // 14-day query ~100x faster on the flash.
  static LogRecord buf[256];
  for (uint32_t i = lo; i < h_.count;) {
    const uint32_t slot = slotOf(i);
    uint32_t len = std::min<uint32_t>({256, h_.count - i, CAPACITY - slot});  // don't cross the wrap point
    file_.seek(sizeof(Header) + slot * sizeof(LogRecord));
    if (file_.read(reinterpret_cast<uint8_t *>(buf), len * sizeof(LogRecord)) != len * sizeof(LogRecord)) break;
    for (uint32_t j = 0; j < len; j++) {
      LogPoint p = unpack(buf[j]);
      const float v[5] = {p.tempC, p.rh, p.hz, p.drift, p.volts};
      for (int k = 0; k < 5; k++)
        if (!std::isnan(v[k])) {
          sum[k] += v[k];
          cnt[k]++;
        }
      tLast = p.t;
      if (++inBucket == step) flush();
    }
    i += len;
    yield();  // long loop: let WiFi and the watchdog breathe
  }
  if (inBucket) flush();
}

LogRecord DataLog::pack(const LogPoint &p) {
  auto i16 = [](float v, float scale) {
    return std::isnan(v) ? NO_VALUE : static_cast<int16_t>(constrain(lroundf(v * scale), -32767L, 32767L));
  };
  LogRecord r;
  r.t = p.t;
  r.tempC100 = i16(p.tempC, 100);
  r.rh100 = std::isnan(p.rh) ? 0xFFFF : static_cast<uint16_t>(constrain(lroundf(p.rh * 100), 0L, 10000L));
  r.hz100 = p.hz > 0 ? static_cast<uint32_t>(lroundf(p.hz * 100)) : 0;
  r.drift100 = i16(p.drift, 100);
  r.adcMv = static_cast<uint16_t>(constrain(lroundf(p.volts * 1000), 0L, 65535L));
  return r;
}

LogPoint DataLog::unpack(const LogRecord &r) {
  return {r.t,
          r.tempC100 == NO_VALUE ? NAN : r.tempC100 / 100.0f,
          r.rh100 == 0xFFFF ? NAN : r.rh100 / 100.0f,
          r.hz100 ? r.hz100 / 100.0f : NAN,
          r.drift100 == NO_VALUE ? NAN : r.drift100 / 100.0f,
          r.adcMv / 1000.0f};
}
