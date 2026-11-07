// SeqPage.qml - the step sequencer: transport, gate mode, BPM/GATE/STEPS and a piano roll.
//
// Piano roll (same as the web page):
//   tap an empty cell     -> that step plays this note
//   drag up/down          -> move the note
//   tap a block           -> delete it (the step becomes a rest)
//   tap a step number     -> select it; the keyboard below then writes notes into that step
// With no step selected, a key RESTARTS the sequence transposed: the bottom C (C3) = your key.
//
// The running light: the ESP only says "step 3, 40 ms into it, 125 ms per step" when something
// changes. A Timer here counts the next steps with the phone's own clock (no polling).
//
// QML lesson: a Canvas is redrawn only when you call requestPaint(). Bindings decide WHEN
// (pattern changed, playhead moved), onPaint decides WHAT.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: page
    contentWidth: availableWidth
    readonly property var pat: device.pattern
    readonly property bool running: device.seqState.run === true
    property int selStep: -1          // step chosen for keyboard entry, -1 = keys transpose
    property int playPos: -1          // step lit by the running light

    // Our own copy of the steps: the roll edits it while you drag, then sends it in one go.
    // JSON round trip = a deep copy (the property from C++ is read-only).
    property var steps: []
    property bool dragging: false
    function copySteps() { if (!dragging) steps = JSON.parse(JSON.stringify(pat.steps || [])) }
    Connections { target: device; function onPatternChanged() { page.copySteps(); roll.requestPaint() } }
    Component.onCompleted: copySteps()

    function sendSteps() { device.seqSet({ steps: steps }) }
    // Hear a note you just wrote, only when stopped (while running the sequencer owns V/oct).
    function preview(n) { if (running) return; device.playNote(n, true); noteOff.note = n; noteOff.restart() }
    Timer { id: noteOff; property int note; interval: 200; onTriggered: device.playNote(note, false) }

    // Running light: position = start step + steps passed since the ESP told us where it was.
    Timer {
        interval: 25; repeat: true; running: page.visible
        onTriggered: {
            const s = device.seqState
            let p = -1
            if (s.run && s.pos >= 0 && s.step_ms > 0)
                p = (s.pos + Math.floor((s.in_ms + Date.now() - device.seqStamp) / s.step_ms)) % s.len
            if (p !== page.playPos) { page.playPos = p; roll.requestPaint() }
        }
    }

    ColumnLayout {
        width: page.availableWidth
        spacing: 12

        Panel {
            title: "SEQUENCER"; Layout.margins: 12
            subtitle: device.seqState.tr ? "transpose " + (device.seqState.tr > 0 ? "+" : "") + device.seqState.tr
                                           + " (C3 → " + device.noteName(48 + device.seqState.tr) + ")" : "transpose 0"
            RowLayout {
                Layout.fillWidth: true
                Button {
                    text: page.running ? "■ STOP" : "▶ PLAY"; Layout.fillWidth: true
                    highlighted: page.running
                    onClicked: device.seqRun(!page.running)
                }
                Button { text: "↺ TRIG"; Layout.fillWidth: true; onClicked: device.seqTrig() }
            }
            RowLayout {
                Layout.fillWidth: true
                Button {
                    text: "RETRIG"; checkable: true; checked: page.pat.mode !== "legato"; Layout.fillWidth: true
                    onClicked: device.seqSet({ mode: "retrig" })
                }
                Button {
                    text: "LEGATO"; checkable: true; checked: page.pat.mode === "legato"; Layout.fillWidth: true
                    onClicked: device.seqSet({ mode: "legato" })
                }
            }
            Text {
                text: page.pat.mode === "legato" ? "one gate per loop, GATE knob not used" : "new gate every step"
                color: "#8a929c"; font.pixelSize: 11
            }
            RowLayout {
                Layout.alignment: Qt.AlignHCenter; spacing: 24
                Knob {
                    label: "BPM"; from: 20; to: 300; defaultValue: 120
                    value: page.pat.bpm || 120
                    format: v => Math.round(v)
                    onMoved: v => device.seqSet({ bpm: Math.round(v) })
                }
                Knob {
                    label: "GATE"; from: 0.05; to: 0.95; defaultValue: 0.5
                    value: page.pat.gate || 0.5
                    enabled: page.pat.mode !== "legato"; opacity: enabled ? 1 : 0.4
                    format: v => Math.round(v * 100) + " %"
                    onMoved: v => device.seqSet({ gate: v })
                }
                Knob {
                    label: "STEPS"; from: 1; to: 16; defaultValue: 8
                    value: page.pat.length || 8
                    format: v => Math.round(v)
                    onMoved: v => device.seqSet({ length: Math.round(v) })
                }
            }
        }

        Panel {
            title: "PIANO ROLL"; Layout.margins: 12
            subtitle: page.selStep >= 0 ? "step " + (page.selStep + 1) + " selected: keys write notes" : "keys play transposed"

            Canvas {
                id: roll
                Layout.fillWidth: true
                // C3..C5, wider when the pattern uses notes outside that
                readonly property var used: page.steps.slice(0, page.pat.length || 8).filter(s => s.on).map(s => s.n)
                readonly property int lo: Math.min(48, ...used)
                readonly property int hi: Math.max(72, ...used)
                readonly property real labelW: 30
                readonly property real headH: 20
                readonly property real colW: (width - labelW) / 16
                readonly property real rowH: 14
                Layout.preferredHeight: headH + (hi - lo + 1) * rowH
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
                Connections { target: page; function onSelStepChanged() { roll.requestPaint() } }

                function rowOf(n) { return hi - n }   // top row = highest note
                function black(n) { return [1, 3, 6, 8, 10].includes(n % 12) }

                onPaint: {
                    const g = getContext("2d"), len = page.pat.length || 8, leg = page.pat.mode === "legato"
                    g.reset()
                    g.fillStyle = "#0d0e10"; g.fillRect(0, 0, width, height)
                    g.font = "10px sans-serif"; g.textBaseline = "middle"
                    // note rows
                    for (let n = hi; n >= lo; n--) {
                        const y = headH + rowOf(n) * rowH
                        g.fillStyle = black(n) ? "#15171a" : "#1c1f23"; g.fillRect(labelW, y, width - labelW, rowH - 1)
                        if (n % 12 === 0 || n === hi || n === lo) { g.fillStyle = "#8a929c"; g.fillText(device.noteName(n), 2, y + rowH / 2) }
                    }
                    for (let i = 0; i < 16; i++) {
                        const x = labelW + i * colW
                        // step header (tap to select)
                        g.fillStyle = i === page.selStep ? "#f0a020" : i === page.playPos ? "#59ff7a" : "#24272c"
                        g.fillRect(x + 1, 0, colW - 2, headH - 2)
                        g.fillStyle = i === page.selStep || i === page.playPos ? "#000" : "#8a929c"
                        g.textAlign = "center"; g.fillText(i + 1, x + colW / 2, headH / 2); g.textAlign = "start"
                        if (i % 4 === 0) { g.fillStyle = "#3b4048"; g.fillRect(x, headH, 1, height - headH) }   // bar lines
                        if (i === page.playPos) { g.fillStyle = "rgba(89,255,122,0.15)"; g.fillRect(x, headH, colW, height - headH) }
                        if (i >= len) { g.fillStyle = "rgba(0,0,0,0.6)"; g.fillRect(x, headH, colW, height - headH) }   // greyed out
                        // the block of this step
                        const s = page.steps[i]
                        if (s && s.on && s.n >= lo && s.n <= hi) {
                            const w = leg ? colW : colW * (page.pat.gate || 0.5)
                            g.fillStyle = i < len ? (leg ? "#4ea1ff" : "#f0a020") : "#555"
                            g.fillRect(x + 1, headH + rowOf(s.n) * rowH + 1, Math.max(3, w - 2), rowH - 3)
                        }
                    }
                }

                // Touch: which step and note is under the finger?
                MouseArea {
                    anchors.fill: parent
                    preventStealing: true          // the ScrollView must not take our vertical drag
                    property int step: -1
                    property int note0: 0
                    property real y0: 0
                    property bool had: false
                    property bool moved: false
                    onPressed: mouse => {
                        step = Math.floor((mouse.x - roll.labelW) / roll.colW)
                        if (step < 0 || step > 15) { step = -1; return }
                        if (mouse.y < roll.headH) { page.selStep = page.selStep === step ? -1 : step; step = -1; return }
                        const n = roll.hi - Math.floor((mouse.y - roll.headH) / roll.rowH)
                        const s = page.steps[step]
                        had = s.on && s.n === n; moved = false; note0 = n; y0 = mouse.y
                        page.dragging = true
                        if (!had) { s.n = n; s.on = true; page.preview(n); roll.requestPaint() }
                    }
                    onPositionChanged: mouse => {
                        if (step < 0) return
                        const n = Math.max(roll.lo, Math.min(roll.hi, note0 + Math.round((y0 - mouse.y) / roll.rowH)))
                        if (n !== page.steps[step].n) { page.steps[step].n = n; moved = true; roll.requestPaint() }
                    }
                    onReleased: {
                        if (step < 0) return
                        if (had && !moved) page.steps[step].on = false   // a tap on a block deletes it
                        else if (moved) page.preview(page.steps[step].n)
                        page.dragging = false
                        page.sendSteps()
                        page.stepsChanged()                              // `used` (range) may change
                        roll.requestPaint()
                        step = -1
                    }
                }
            }
        }

        Panel {
            title: "KEYBOARD"; Layout.margins: 12; subtitle: "C3 .. C5"
            Keyboard {
                Layout.fillWidth: true
                onPressedKey: n => {
                    if (page.selStep >= 0) {               // step entry
                        page.steps[page.selStep].n = n; page.steps[page.selStep].on = true
                        page.sendSteps(); page.preview(n)
                        page.selStep = (page.selStep + 1) % (page.pat.length || 8)
                        page.stepsChanged(); roll.requestPaint()
                    } else {
                        device.seqTrig(n)                  // play the sequence from this key
                    }
                }
            }
        }
    }
}
