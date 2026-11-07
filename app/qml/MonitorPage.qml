// MonitorPage.qml - what the ESP measures and what it outputs.
//
//   NOW      live values, pushed by the ESP every second
//   HISTORY  temperature + one chosen series: the last 10 minutes (RAM, 1 per second) or up to
//            46 days from the flash log (averaged down to 400 points so Bluetooth stays quick)
//
// QML lesson: a JavaScript function inside a QML item (fmt, drawLine) is a normal function you
// can call from bindings and from onPaint.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {   // no scrolling: everything fits the screen, so dragging a knob never moves the page
    id: page
    readonly property var lv: device.live
    readonly property bool shown: SwipeView.isCurrentItem
    function fmt(v, dec, unit) { return (v === undefined || v === null || isNaN(v)) ? "–" : Number(v).toFixed(dec) + (unit || "") }
    function since(s) { s = Math.floor(s || 0); return s < 3600 ? Math.floor(s / 60) + " min" : Math.floor(s / 3600) + " h " + Math.floor(s % 3600 / 60) + " min" }

    // the chosen range: 0 = live from RAM, else hours of the flash log
    property real hours: 0
    onShownChanged: if (shown) device.loadHistory(hours)
    onHoursChanged: device.loadHistory(hours)
    Timer {   // the live range keeps growing: fetch it again every 5 s while you look at it
        interval: 5000; repeat: true; running: page.shown && page.hours === 0 && device.connected
        onTriggered: device.loadHistory(0)
    }

    ColumnLayout {
        anchors { fill: parent; margins: 10 }
        spacing: 10

        Panel {
            title: "NOW"
            subtitle: (lv.wifi ? "ESP on WiFi: " + lv.wifi : "") + (lv.uptime ? " · up " + page.since(lv.uptime) : "")
            GridLayout {
                columns: 3; Layout.fillWidth: true; columnSpacing: 6; rowSpacing: 6
                Repeater {
                    model: [
                        { k: "TEMP", v: page.fmt(lv.temp_c, 1, " °C") },
                        { k: "HUMIDITY", v: page.fmt(lv.rh, 0, " %") },
                        { k: "ADC IN", v: page.fmt(lv.volts, 3, " V") },
                        { k: "FREQUENCY", v: lv.hz > 0 ? page.fmt(lv.hz, 2, " Hz") : "no signal" },
                        { k: "NOTE IN", v: lv.hz > 0 && lv.note >= 0 ? device.noteName(lv.note) + " " + (lv.cents_off >= 0 ? "+" : "") + page.fmt(lv.cents_off, 0, "c") : "–" },
                        { k: "DRIFT", v: page.fmt(lv.drift, 1, " c") + (lv.drift_per_min !== undefined ? " (" + page.fmt(lv.drift_per_min, 1, "/min") + ")" : "") },
                        { k: "LFO OUT", v: page.fmt(lv.lfo_v, 2, " V") },
                        { k: "ENV OUT", v: page.fmt(lv.env_v, 2, " V") },
                        { k: "V/OCT OUT", v: page.fmt(lv.voct_v, 3, " V") + (lv.voct_note >= 0 ? " " + device.noteName(lv.voct_note) : "") },
                        { k: "LOG POINTS", v: lv.log_count !== undefined ? String(lv.log_count) : "–" },
                        { k: "CLOCK", v: lv.time ? new Date(lv.time * 1000).toLocaleTimeString(Qt.locale(), "HH:mm:ss") : "not set" },
                        { k: "MQTT", v: lv.mqtt === undefined ? "–" : lv.mqtt ? "connected" : "off" }
                    ]
                    Rectangle {
                        Layout.fillWidth: true; Layout.preferredHeight: 48; radius: 6; color: "#1c1f23"
                        Column {
                            anchors { left: parent.left; verticalCenter: parent.verticalCenter; leftMargin: 8 }
                            Text { text: modelData.k; color: "#8a929c"; font.pixelSize: 9; font.letterSpacing: 1 }
                            Text { text: modelData.v; color: "#e6e6e6"; font.pixelSize: 13; font.family: "monospace"
                                   width: parent.parent.width - 12; fontSizeMode: Text.HorizontalFit; minimumPixelSize: 9 }
                        }
                    }
                }
            }
        }

        Panel {
            title: "HISTORY"; Layout.fillHeight: true
            subtitle: (device.history.t || []).length + " points"
            RowLayout {
                Layout.fillWidth: true
                ComboBox {
                    id: range; Layout.fillWidth: true
                    textRole: "l"; valueRole: "h"
                    model: [ { l: "live (10 min)", h: 0 }, { l: "1 hour", h: 1 }, { l: "24 hours", h: 24 },
                             { l: "7 days", h: 168 }, { l: "14 days", h: 336 }, { l: "46 days", h: 1104 } ]
                    onActivated: page.hours = currentValue
                }
                ComboBox {
                    id: series; Layout.fillWidth: true
                    textRole: "l"; valueRole: "k"
                    model: [ { l: "drift (cents)", k: "drift" }, { l: "frequency (Hz)", k: "hz" },
                             { l: "ADC (V)", k: "volts" }, { l: "humidity (%)", k: "rh" } ]
                    onActivated: chart.requestPaint()
                }
            }
            Canvas {
                id: chart
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 120
                Connections { target: device; function onHistoryChanged() { chart.requestPaint() } }
                onWidthChanged: requestPaint()
                // One line, scaled to its own min..max. Gaps (null = no reading) break the line.
                function drawLine(g, ts, vs, col, top, h, t0, t1, label) {
                    const ok = vs.filter(v => v !== null && !isNaN(v))
                    g.fillStyle = "#667"; g.fillText(label, 4, top + 10)
                    if (!ok.length) { g.fillText("no data", width / 2 - 20, top + h / 2); return }
                    let lo = Math.min(...ok), hi = Math.max(...ok)
                    if (hi - lo < 1e-6) { lo -= 0.5; hi += 0.5 }
                    const X = t => 40 + (t - t0) / Math.max(1, t1 - t0) * (width - 46), Y = v => top + h - 4 - (v - lo) / (hi - lo) * (h - 18)
                    g.fillText(hi.toFixed(1), 2, Y(hi) + 3); g.fillText(lo.toFixed(1), 2, Y(lo))
                    g.strokeStyle = col; g.lineWidth = 1.5; g.beginPath()
                    let pen = false
                    for (let i = 0; i < ts.length; i++) {
                        if (vs[i] === null || isNaN(vs[i])) { pen = false; continue }
                        pen ? g.lineTo(X(ts[i]), Y(vs[i])) : g.moveTo(X(ts[i]), Y(vs[i])); pen = true
                    }
                    g.stroke()
                }
                onPaint: {
                    const g = getContext("2d"), Hd = device.history, ts = Hd.t || []
                    g.reset(); g.fillStyle = "#0d0e10"; g.fillRect(0, 0, width, height); g.font = "10px sans-serif"
                    if (!ts.length) { g.fillStyle = "#667"; g.fillText(device.connected ? "loading…" : "not connected", 10, 20); return }
                    const t0 = ts[0], t1 = ts[ts.length - 1], half = height / 2
                    drawLine(g, ts, Hd.temp || [], "#ff8a5c", 0, half, t0, t1, "temperature °C")
                    g.strokeStyle = "#222"; g.beginPath(); g.moveTo(0, half); g.lineTo(width, half); g.stroke()
                    drawLine(g, ts, Hd[series.currentValue] || [], "#4ea1ff", half, half, t0, t1, series.currentText)
                    // time axis: clock times for the flash log (real time), seconds ago for RAM (uptime)
                    g.fillStyle = "#667"
                    const lab = t => t > 1e9 ? new Date(t * 1000).toLocaleString(Qt.locale(), page.hours > 24 ? "dd/MM HH:mm" : "HH:mm")
                                             : "-" + Math.round(t1 - t) + " s"
                    g.fillText(lab(t0), 40, height - 2); g.textAlign = "right"; g.fillText(lab(t1), width - 4, height - 2)
                }
            }
        }
    }
}
