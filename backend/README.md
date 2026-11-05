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

## Raspberry Pi (without Docker)

A Pi 2 or newer is enough: Mosquitto, the Python backend and SQLite together use well under
100 MB of RAM. On a Pi 2 (32-bit, 1 GB) install them directly instead of with Docker, which is
slower to build there. A Pi 2 has no WiFi: use Ethernet to your router, or a USB WiFi dongle.

1. Flash **Raspberry Pi OS Lite** with Raspberry Pi Imager. In its settings set user `pi`, a
   password, your WiFi (if you have a dongle) and enable SSH.
2. Log in (`ssh pi@raspberrypi.local`) and install:
   ```
   sudo apt update
   sudo apt install -y mosquitto git python3-venv
   sudo cp /etc/mosquitto/mosquitto.conf /etc/mosquitto/mosquitto.conf.bak
   echo -e "listener 1883\nallow_anonymous true" | sudo tee /etc/mosquitto/conf.d/synthpatch.conf
   sudo systemctl restart mosquitto
   ```
3. Get the code and the Python libraries:
   ```
   git clone -b prototype-setup https://github.com/jmartens90/IoT_SynthPatch.git
   python3 -m venv ~/synthpatch-venv
   ~/synthpatch-venv/bin/pip install -r IoT_SynthPatch/backend/app/requirements.txt
   ```
4. Start it at every boot:
   ```
   sudo cp IoT_SynthPatch/backend/pi/synthpatch-backend.service /etc/systemd/system/
   sudo systemctl enable --now synthpatch-backend
   systemctl status synthpatch-backend        (should say "active (running)")
   ```
5. On the ESP: `mqtt set raspberrypi.local` doesn't work (the ESP can't look up .local names for
   MQTT), so use the Pi's IP from `hostname -I`. Open `http://<pi IP>:8000` on any PC or phone.

Logs: `journalctl -u synthpatch-backend -f`. After a `git pull`: `sudo systemctl restart synthpatch-backend`.
