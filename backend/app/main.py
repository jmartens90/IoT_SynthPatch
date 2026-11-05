"""main.py - starts the MQTT logger and the web server.

    http://localhost:8000/                       history page (static/index.html)
    GET  /api/devices                            modules seen, with online/offline
    GET  /api/telemetry?device=proto&hours=1     stored telemetry rows
    GET  /api/events?device=proto&hours=1        stored note changes
    GET  /api/settings?device=proto              latest LFO/envelope settings
    POST /api/cmd?device=proto  (JSON body)      forwards a command to the module over MQTT
"""
import os
import time

from flask import Flask, jsonify, request, send_from_directory

from db import Database
from mqtt_logger import MqttLogger

# Settings come from environment variables (set in docker-compose.yml), with defaults so you
# can also run it without Docker:  python main.py
MQTT_HOST = os.environ.get("MQTT_HOST", "localhost")
MQTT_PORT = int(os.environ.get("MQTT_PORT", "1883"))
DB_PATH = os.environ.get("DB_PATH", "synthpatch.db")

db = Database(DB_PATH)
logger = MqttLogger(db, MQTT_HOST, MQTT_PORT)
app = Flask(__name__, static_folder="static")


def since():
    """Start of the requested time range: ?hours=... back from now (default 1 hour)."""
    return time.time() - float(request.args.get("hours", 1)) * 3600


@app.get("/")
def index():
    return send_from_directory(app.static_folder, "index.html")


@app.get("/api/devices")
def devices():
    return jsonify(db.devices())


@app.get("/api/telemetry")
def telemetry():
    return jsonify(db.telemetry(request.args.get("device", "proto"), since()))


@app.get("/api/events")
def events():
    return jsonify(db.events(request.args.get("device", "proto"), since()))


@app.get("/api/settings")
def settings():
    return jsonify(db.latest_settings(request.args.get("device", "proto")))


@app.post("/api/cmd")
def cmd():
    command = request.get_json(silent=True)
    if not isinstance(command, dict):
        return jsonify(error="send a JSON object"), 400
    logger.send_command(request.args.get("device", "proto"), command)
    return jsonify(ok=True)


if __name__ == "__main__":
    logger.start()
    print(f"backend: MQTT {MQTT_HOST}:{MQTT_PORT}, database {DB_PATH}, web http://localhost:8000")
    app.run(host="0.0.0.0", port=8000)
