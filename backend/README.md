# Backend: MQTT broker + database + history page

Runs on your laptop with Docker. No Raspberry Pi needed.

```
ESP32 ──MQTT──> Mosquitto (broker) ──MQTT──> backend (Python) ──> SQLite file (backend/data/)
                     ^                            │
                     └──── commands (cmd) <───────┴── history page http://localhost:8000
```

| File | What |
|---|---|
| `docker-compose.yml` | Starts both containers: `mosquitto` and `backend` |
| `mosquitto/mosquitto.conf` | Opens port 1883 to the network, no password (demo only) |
| `app/Dockerfile` | How the backend image is built (Python 3.12 + 2 libraries) |
| `app/main.py` | Starts the MQTT logger and the web server (Flask), defines the REST API |
| `app/mqtt_logger.py` | Subscribes to `synthpatch/+/...` and stores each message |
| `app/db.py` | The SQLite tables and every SQL query |
| `app/static/index.html` | History chart, note changes, online/offline, controls that send MQTT commands |

## Run it

1. Install **Docker Desktop** and start it.
2. In this folder: `docker compose up -d` (the first time it downloads the images, ~1 minute).
3. Find your laptop's IP: `ipconfig` (Windows), the "IPv4 Address" of your WiFi adapter.
4. On the ESP's serial shell: `mqtt set <that IP>` (it reboots and connects).
5. Open http://localhost:8000. Within seconds the status turns **online** and samples come in.

Windows firewall: the first time, allow Docker Desktop on private networks, otherwise the ESP can't
reach port 1883. Tip for the demo: put the laptop and the ESP on your phone's hotspot.

Useful commands:

```
docker compose logs -f backend      what the backend prints (connected, skipped messages)
docker compose down                 stop (the data in backend/data stays)
docker compose up -d --build        restart after you changed the Python code
```

Without Docker (for debugging): install Mosquitto, then `pip install -r app/requirements.txt` and
`python app/main.py` (it uses `localhost` and `synthpatch.db` by default).

## Look at the MQTT traffic

**MQTT Explorer** (free) → connect to `localhost:1883`. You see `synthpatch/proto/telemetry`,
`event`, `settings` and `status` live, and you can publish a command to `synthpatch/proto/cmd`, e.g.
`{"target":"lfo1","name":"rate","value":3}`.

## REST API

| Request | Returns |
|---|---|
| `GET /api/devices` | modules seen, last seen, online/offline |
| `GET /api/telemetry?device=proto&hours=1` | stored samples |
| `GET /api/events?device=proto&hours=24` | note changes |
| `GET /api/settings?device=proto` | latest LFO/envelope settings |
| `POST /api/cmd?device=proto` + JSON body | forwards the command to the module |

## Show that history is stored (rubric)

`docker compose down`, then `docker compose up -d`: the chart still shows everything from before,
because the database file is in `backend/data/` on your laptop, outside the container.

## Secure it (nice-to-have, later)

Add a password file to Mosquitto (`mosquitto_passwd`), set `allow_anonymous false`, and give the
ESP the user/password with `mqtt set <ip> 1883 <user> <pass>`. Keep the passwords out of git.
