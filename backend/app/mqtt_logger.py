"""mqtt_logger.py - listens to the broker and puts every message in the database.

Topics (the ESP publishes these, <device> is "proto" for the prototype):
    synthpatch/<device>/telemetry   JSON every 2 s
    synthpatch/<device>/event       JSON per note change
    synthpatch/<device>/settings    JSON, retained
    synthpatch/<device>/status      "online" / "offline", retained (last will)

"+" in a subscription is a wildcard for exactly one level, so "synthpatch/+/telemetry"
matches every device. More modules later = no code change.
"""
import json

import paho.mqtt.client as mqtt

SUBSCRIPTIONS = ["synthpatch/+/telemetry", "synthpatch/+/event",
                 "synthpatch/+/settings", "synthpatch/+/status"]


class MqttLogger:
    def __init__(self, db, host, port=1883):
        self.db = db
        self.client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="synthpatch-backend")
        # paho calls these functions when something happens (callbacks, like Qt slots).
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        self.client.connect_async(host, port)

    def start(self):
        # loop_start() runs the network in a background thread and reconnects by itself
        # when the broker restarts.
        self.client.loop_start()

    def on_connect(self, client, userdata, flags, reason_code, properties):
        print(f"MQTT connected: {reason_code}")
        # Subscribe here, not in __init__: after a reconnect the subscriptions are gone.
        for topic in SUBSCRIPTIONS:
            client.subscribe(topic, qos=1)

    def on_message(self, client, userdata, msg):
        # "synthpatch/proto/telemetry" -> ["synthpatch", "proto", "telemetry"]
        _, device, kind = msg.topic.split("/", 2)
        text = msg.payload.decode("utf-8", errors="replace")
        try:
            if kind == "status":
                self.db.add_status(device, text)
                return
            data = json.loads(text)
            if kind == "telemetry":
                self.db.add_telemetry(device, data)
            elif kind == "event":
                self.db.add_event(device, data)
            elif kind == "settings":
                self.db.add_settings(device, data)
        except (ValueError, TypeError) as e:
            # A broken message must never stop the logger: report it and carry on.
            print(f"skipped bad message on {msg.topic}: {e}")

    def send_command(self, device, command):
        """Publish a command to the module, e.g. {"target": "lfo1", "name": "rate", "value": 2}."""
        self.client.publish(f"synthpatch/{device}/cmd", json.dumps(command), qos=1)
