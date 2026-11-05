"""db.py - everything that touches the database.

SQLite is a whole SQL database in one file (here /data/synthpatch.db). No server to install,
and Python has it built in (the sqlite3 module).

Four tables, one per kind of MQTT message:
    telemetry  one row every 2 s: temperature, frequency, note, drift, output voltages
    events     one row per note change
    settings   the LFO/envelope settings, one row each time they change (so you see the history)
    status     "online" / "offline" (the ESP's last will), one row per change

Every row gets the time the BACKEND received it (ts, seconds since 1970). The ESP has no clock.
"""
import json
import sqlite3
import threading
import time

SCHEMA = """
CREATE TABLE IF NOT EXISTS telemetry (
    device TEXT, ts REAL,
    temp_c REAL, rh REAL, hz REAL, note INTEGER, cents_off REAL,
    drift_cents REAL, drift_per_min REAL, adc_v REAL, lfo_v REAL, env_v REAL, voct_v REAL
);
CREATE INDEX IF NOT EXISTS telemetry_by_time ON telemetry (device, ts);
CREATE TABLE IF NOT EXISTS events   (device TEXT, ts REAL, from_note INTEGER, to_note INTEGER);
CREATE TABLE IF NOT EXISTS settings (device TEXT, ts REAL, json TEXT);
CREATE TABLE IF NOT EXISTS status   (device TEXT, ts REAL, state TEXT);
"""

# The telemetry JSON keys we store, in table-column order.
TELEMETRY_FIELDS = ["temp_c", "rh", "hz", "note", "cents_off", "drift_cents",
                    "drift_per_min", "adc_v", "lfo_v", "env_v", "voct_v"]


class Database:
    def __init__(self, path):
        # check_same_thread=False: the MQTT thread writes, the web thread reads.
        # The lock makes sure they take turns.
        self.conn = sqlite3.connect(path, check_same_thread=False)
        self.conn.row_factory = sqlite3.Row  # rows behave like dicts: row["temp_c"]
        self.lock = threading.Lock()
        with self.lock:
            self.conn.executescript(SCHEMA)

    # ---------- writing (called by the MQTT logger) ----------

    def add_telemetry(self, device, data):
        values = [data.get(k) for k in TELEMETRY_FIELDS]  # missing key -> None -> NULL
        # The ? placeholders let sqlite3 insert the values safely (no SQL injection).
        sql = f"INSERT INTO telemetry VALUES (?, ?, {', '.join('?' * len(values))})"
        self._write(sql, [device, time.time(), *values])

    def add_event(self, device, data):
        self._write("INSERT INTO events VALUES (?, ?, ?, ?)",
                    [device, time.time(), data.get("from"), data.get("to")])

    def add_settings(self, device, data):
        self._write("INSERT INTO settings VALUES (?, ?, ?)", [device, time.time(), json.dumps(data)])

    def add_status(self, device, state):
        self._write("INSERT INTO status VALUES (?, ?, ?)", [device, time.time(), state])

    def _write(self, sql, params):
        with self.lock:
            self.conn.execute(sql, params)
            self.conn.commit()

    # ---------- reading (called by the web API) ----------

    def devices(self):
        """Every device we ever heard from, with its latest status."""
        rows = self._read("""
            SELECT device, MAX(ts) AS last_seen FROM telemetry GROUP BY device""")
        result = []
        for r in rows:
            st = self._read("SELECT state FROM status WHERE device = ? ORDER BY ts DESC LIMIT 1",
                            [r["device"]])
            result.append({"device": r["device"], "last_seen": r["last_seen"],
                           "status": st[0]["state"] if st else "unknown"})
        return result

    def telemetry(self, device, since):
        rows = self._read("SELECT * FROM telemetry WHERE device = ? AND ts >= ? ORDER BY ts",
                          [device, since])
        return [dict(r) for r in rows]

    def events(self, device, since):
        rows = self._read("SELECT ts, from_note, to_note FROM events WHERE device = ? AND ts >= ? ORDER BY ts",
                          [device, since])
        return [dict(r) for r in rows]

    def latest_settings(self, device):
        rows = self._read("SELECT ts, json FROM settings WHERE device = ? ORDER BY ts DESC LIMIT 1", [device])
        return {"ts": rows[0]["ts"], "settings": json.loads(rows[0]["json"])} if rows else None

    def _read(self, sql, params=()):
        with self.lock:
            return self.conn.execute(sql, params).fetchall()
