// SynthPage.qml - LFO and envelope: the same controls as on the web page.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {   // no scrolling: everything fits the screen, so dragging a knob never moves the page
    id: page
    readonly property bool shown: SwipeView.isCurrentItem   // the live dot only moves when you look
    readonly property var lfo: device.settings.lfo1 || {}
    readonly property var env: device.settings.env1 || {}
    function range(key) { return device.ranges[key] || { min: 0, max: 1 } }
    readonly property real mid: 1.65            // the LFO swings around the middle of 0..3.3 V
    readonly property var waves: ({
        sine: x => Math.sin(2 * Math.PI * x),
        triangle: x => x < .25 ? 4 * x : x < .75 ? 2 - 4 * x : 4 * x - 4,
        saw: x => 2 * x - 1,
        square: x => x < .5 ? 1 : -1 })

    ColumnLayout {
        anchors { fill: parent; margins: 10 }
        spacing: 10

        Panel {
            Layout.fillHeight: true
            title: "LFO"; subtitle: clip ? "CLIP! · MCP4725 0..3.3 V" : "MCP4725 0..3.3 V"
            // offset ± depth outside 0..3.3 V: the DAC clips there
            property bool clip: (page.lfo.offset || 0) + (page.lfo.depth || 0) > 3.3 + 1e-3
                                || (page.lfo.offset || 0) - (page.lfo.depth || 0) < -1e-3

            Canvas {   // two cycles of the wave, red where the DAC clips, live output as a dot
                id: scope
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 50
                property real out: device.live.lfo_v || 0
                onOutChanged: if (page.shown) requestPaint()
                Connections { target: page; function onShownChanged() { if (page.shown) scope.requestPaint() } }
                Connections { target: device; function onSettingsChanged() { scope.requestPaint() } }
                onPaint: {
                    const g = getContext("2d"), W = width, H = height, L = page.lfo
                    const Y = v => H - 6 - v / 3.3 * (H - 12)
                    g.reset(); g.fillStyle = "#0d0e10"; g.fillRect(0, 0, W, H)
                    g.strokeStyle = "#333"; g.beginPath(); g.moveTo(0, Y(1.65)); g.lineTo(W, Y(1.65)); g.stroke()
                    if (L.depth === undefined) return
                    const f = page.waves[L.shape] || page.waves.sine
                    g.lineWidth = 2
                    for (let i = 0; i < W; i += 2) {
                        const raw = L.offset + L.depth * f((i / W * 2) % 1), v = Math.max(0, Math.min(3.3, raw))
                        g.strokeStyle = raw !== v ? "#ff4d4d" : "#f0a020"
                        g.beginPath(); g.moveTo(i, Y(v)); g.lineTo(i + 2, Y(v)); g.stroke()
                    }
                    g.fillStyle = "#4ea1ff"; g.beginPath(); g.arc(W - 8, Y(out), 4, 0, 7); g.fill()
                }
            }
            RowLayout {
                Repeater {
                    model: ["sine", "triangle", "saw", "square"]
                    Button {
                        text: modelData; checkable: true; checked: page.lfo.shape === modelData
                        onClicked: device.setShape("lfo1", modelData)
                        Layout.fillWidth: true; font.pixelSize: 11
                    }
                }
            }
            RowLayout {
                Layout.alignment: Qt.AlignHCenter; spacing: 24
                Knob {
                    label: "RATE"; logScale: true; defaultValue: 0.5
                    from: page.range("lfo1.rate").min; to: page.range("lfo1.rate").max
                    value: page.lfo.rate || 0.5
                    format: v => v < 1 ? v.toFixed(2) + " Hz" : v.toFixed(1) + " Hz"
                    onMoved: v => device.setParam("lfo1", "rate", v)
                }
                Knob {
                    label: "DEPTH"; defaultValue: page.mid
                    from: page.range("lfo1.depth").min; to: page.range("lfo1.depth").max
                    value: page.lfo.depth || 0
                    format: v => "±" + v.toFixed(2) + " V"
                    onMoved: v => device.setParam("lfo1", "depth", v)
                }
                Knob {
                    label: "OFFSET"; bipolar: true; defaultValue: page.mid
                    from: page.range("lfo1.offset").min; to: page.range("lfo1.offset").max
                    value: page.lfo.offset || page.mid
                    format: v => (v >= page.mid ? "+" : "") + (v - page.mid).toFixed(2) + " V"   // shown around 1.65 V
                    onMoved: v => device.setParam("lfo1", "offset", v)
                }
            }
        }

        Panel {
            Layout.fillHeight: true
            title: "ENVELOPE"; subtitle: device.live.env_out || ""
            Canvas {   // the ADSR shape
                id: adsr
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 50
                Connections { target: device; function onSettingsChanged() { adsr.requestPaint() } }
                onPaint: {
                    const g = getContext("2d"), W = width, H = height, E = page.env
                    g.reset(); g.fillStyle = "#0d0e10"; g.fillRect(0, 0, W, H)
                    if (E.attack === undefined) return
                    const hold = .25 * (E.attack + E.decay + E.release) + .1, tot = E.attack + E.decay + hold + E.release
                    const X = t => 6 + t / tot * (W - 12), Y = v => H - 6 - v / 3.3 * (H - 12)
                    g.strokeStyle = "#f0a020"; g.lineWidth = 2; g.beginPath()
                    g.moveTo(X(0), Y(0)); g.lineTo(X(E.attack), Y(E.peak))
                    g.lineTo(X(E.attack + E.decay), Y(E.sustain * E.peak))
                    g.lineTo(X(E.attack + E.decay + hold), Y(E.sustain * E.peak)); g.lineTo(X(tot), Y(0)); g.stroke()
                }
            }
            GridLayout {
                columns: 5; Layout.alignment: Qt.AlignHCenter; columnSpacing: 0
                Repeater {
                    model: [ { n: "attack", l: "ATTACK", log: true, d: .05 }, { n: "decay", l: "DECAY", log: true, d: .2 },
                             { n: "sustain", l: "SUSTAIN", log: false, d: .6 }, { n: "release", l: "RELEASE", log: true, d: .8 },
                             { n: "peak", l: "PEAK", log: false, d: 3.3 } ]
                    Knob {
                        label: modelData.l; logScale: modelData.log; defaultValue: modelData.d
                        from: page.range("env1." + modelData.n).min; to: page.range("env1." + modelData.n).max
                        value: page.env[modelData.n] || modelData.d
                        format: v => modelData.n === "sustain" ? Math.round(v * 100) + " %"
                                   : modelData.n === "peak" ? v.toFixed(2) + " V"
                                   : v < 1 ? Math.round(v * 1000) + " ms" : v.toFixed(2) + " s"
                        onMoved: v => device.setParam("env1", modelData.n, v)
                    }
                }
            }
            Button {
                text: "GATE (hold)"; Layout.fillWidth: true
                onPressed: device.gate(true); onReleased: device.gate(false)
            }
        }
    }
}
