#include "DataLog.h"

#include <LittleFS.h>

#include <algorithm>

static const char *DIR = "/log";
static const char *HDR = "/log/hdr.bin";
static const char *OLD_PATH = "/log.bin";  // the first version: one big file, header at the start

String DataLog::chunkPath(uint32_t chunk) {
  char p[20];
  snprintf(p, sizeof(p), "/log/%03u.bin", static_cast<unsigned>(chunk));
  return p;
}

bool DataLog::begin() {
  // true = format the partition if it can't be mounted (first boot after the new flash layout)
  if (!LittleFS.begin(true)) return false;
  LittleFS.mkdir(DIR);  // fails harmlessly if it is already there

  fs::File f = LittleFS.open(HDR, "r");
  if (f && f.read(reinterpret_cast<uint8_t *>(&h_), sizeof(h_)) == sizeof(h_) && h_.magic == MAGIC &&
      h_.capacity == CAPACITY && h_.head < CAPACITY && h_.count <= CAPACITY) {
    f.close();
    ok_ = true;
  } else {
    if (f) f.close();
    h_ = Header{MAGIC, 2, CAPACITY, 0, 0, {}};
    ok_ = writeHeader();
  }
  if (ok_ && LittleFS.exists(OLD_PATH)) migrateOldFile();
  return ok_;
}

// Once, after the update: copy the records of the old single-file log into the chunks.
void DataLog::migrateOldFile() {
  fs::File old = LittleFS.open(OLD_PATH, "r");
  Header oh{};
  if (old && old.read(reinterpret_cast<uint8_t *>(&oh), sizeof(oh)) == sizeof(oh) && oh.magic == MAGIC &&
      oh.capacity == 400000 && oh.count <= oh.capacity && h_.count == 0) {
    Serial.printf("flash log: moving %u records to the new layout...\n", static_cast<unsigned>(oh.count));
    static LogRecord buf[256];
    for (uint32_t i = 0; i < oh.count;) {
      const uint32_t slot = (oh.head + oh.capacity - oh.count + i) % oh.capacity;
      uint32_t len = std::min<uint32_t>({256, oh.count - i, oh.capacity - slot});
      old.seek(sizeof(Header) + slot * sizeof(LogRecord));
      if (old.read(reinterpret_cast<uint8_t *>(buf), len * sizeof(LogRecord)) != len * sizeof(LogRecord)) break;
      // write in pieces that stay inside one chunk
      for (uint32_t j = 0; j < len;) {
        const uint32_t off = h_.head % CHUNK, m = std::min(len - j, CHUNK - off);
        if (!openForAppend(h_.head / CHUNK, off == 0)) break;
        wfile_.write(reinterpret_cast<const uint8_t *>(buf + j), m * sizeof(LogRecord));
        h_.head = (h_.head + m) % CAPACITY;
        h_.count = std::min(h_.count + m, CAPACITY - (CHUNK - (off + m)));
        j += m;
      }
      i += len;
      yield();
    }
    wfile_.flush();
    writeHeader();
    Serial.printf("flash log: done, %u records\n", static_cast<unsigned>(h_.count));
  }
  if (old) old.close();
  LittleFS.remove(OLD_PATH);
}

void DataLog::clear() {
  if (!ok_) return;
  if (wfile_) wfile_.close();
  if (rfile_) rfile_.close();
  wChunk_ = rChunk_ = -1;
  for (uint32_t c = 0; c < CHUNKS; c++) {
    const String p = chunkPath(c);
    if (LittleFS.exists(p)) LittleFS.remove(p);
  }
  h_ = Header{MAGIC, 2, CAPACITY, 0, 0, {}};
  writeHeader();
}

// A tiny file: LittleFS keeps it inside its directory block, so rewriting it is cheap.
bool DataLog::writeHeader() {
  fs::File f = LittleFS.open(HDR, "w");
  if (!f) return false;
  const bool ok = f.write(reinterpret_cast<const uint8_t *>(&h_), sizeof(h_)) == sizeof(h_);
  f.close();
  return ok;
}

// Make wfile_ the file of `chunk`, positioned where slot head goes. fresh = start the chunk
// empty (a new chunk, or the ring came round to it again).
bool DataLog::openForAppend(uint32_t chunk, bool fresh) {
  if (fresh || wChunk_ != static_cast<int32_t>(chunk) || !wfile_) {
    if (wfile_) wfile_.close();
    if (rChunk_ == static_cast<int32_t>(chunk)) {  // don't keep reading an old copy of it
      rfile_.close();
      rChunk_ = -1;
    }
    const String p = chunkPath(chunk);
    wfile_ = LittleFS.open(p, fresh || !LittleFS.exists(p) ? "w+" : "r+");
    wChunk_ = wfile_ ? static_cast<int32_t>(chunk) : -1;
    if (!wfile_) return false;
  }
  // Normally this is the end of the file. After a power cut between the record and the header
  // there may be one record more: it is simply written over (at the very end: still cheap).
  return wfile_.seek((h_.head % CHUNK) * sizeof(LogRecord));
}

bool DataLog::append(const LogPoint &p) {
  if (!ok_) return false;
  const uint32_t off = h_.head % CHUNK;
  if (!openForAppend(h_.head / CHUNK, off == 0)) return false;
  LogRecord r = pack(p);
  if (wfile_.write(reinterpret_cast<const uint8_t *>(&r), sizeof(r)) != sizeof(r)) return false;
  wfile_.flush();  // really write it now, so a power cut loses at most this one record
  h_.head = (h_.head + 1) % CAPACITY;
  // The rest of this chunk is empty (it was started fresh): once the ring has come round, the
  // old records that were there no longer count.
  h_.count = std::min(h_.count + 1, CAPACITY - (CHUNK - (off + 1)));
  return writeHeader();
}

bool DataLog::readRange(uint32_t slot, uint32_t n, LogRecord *out) {
  const uint32_t chunk = slot / CHUNK;
  fs::File *f;
  if (wChunk_ == static_cast<int32_t>(chunk) && wfile_) {
    f = &wfile_;  // the chunk being written: read through the same handle (always up to date)
  } else {
    if (rChunk_ != static_cast<int32_t>(chunk) || !rfile_) {
      if (rfile_) rfile_.close();
      rfile_ = LittleFS.open(chunkPath(chunk), "r");
      rChunk_ = rfile_ ? static_cast<int32_t>(chunk) : -1;
      if (!rfile_) return false;
    }
    f = &rfile_;
  }
  if (!f->seek((slot % CHUNK) * sizeof(LogRecord))) return false;
  return f->read(reinterpret_cast<uint8_t *>(out), n * sizeof(LogRecord)) == n * sizeof(LogRecord);
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
  // records per output point (rounded up). In 64 bits: with maxPoints = UINT32_MAX ("all", the
  // CSV export) n + maxPoints overflowed 32 bits, step became 0 and everything was averaged
  // into a single point.
  const uint32_t step = n <= maxPoints ? 1 : static_cast<uint32_t>((uint64_t(n) + maxPoints - 1) / maxPoints);

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
    // don't cross a chunk (or the wrap point, which is a chunk boundary too)
    uint32_t len = std::min<uint32_t>({256, h_.count - i, CHUNK - slot % CHUNK});
    if (!readRange(slot, len, buf)) break;
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
