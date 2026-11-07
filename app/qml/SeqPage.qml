// SeqPage.qml - the step sequencer: transport, gate mode, BPM/GATE/STEPS and a piano roll.
//
// Piano roll (same as the web page):
//   tap an empty cell     -> that step plays this note
//   drag up/down          -> move the note
//   tap a block           -> delete it (the step becomes a rest)
//   tap a step number     -> select it; the keyboard below then writes notes into that step
// Everything fits one screen (no scrolling). The roll shows 8 steps x 1 octave at a time:
// 1-8 / 9-16 and the octave buttons move that window.
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

Item {   // no scrolling: everything fits the screen, so dragging a knob never moves the page
    id: page
    readonly property var pat: device.pattern
    readonly property bool running: device.seqState.run === true
    property int selStep: -1          // step chosen for keyboard entry, -1 = keys transpose
    property int playPos: -1          // step lit by the running light
    property int stepPage: 0          // piano roll shows steps 1-8 (0) or 9-16 (1)
    property int octLo: 48            // piano roll shows octLo..octLo+12 (C3..C4 at first)
    property bool octSet: false       // octave picked once from the pattern's notes

    // Our own copy of the steps: the roll edits it while you drag, then sends it in one go.
    // JSON round trip = a deep copy (the property from C++ is read-only).
    property var steps: []
    property bool dragging: false
    function copySteps() {
        if (dragging) return
        steps = JSON.parse(JSON.stringify(pat.steps || []))
        if (!octSet && steps.length) {   // first load: show the octave where most notes are
            const ns = steps.slice(0, pat.length || 8).filter(s => s.on).map(s => s.n).sort((a, b) => a - b)
            if (ns.length) octLo = 12 * Math.floor(ns[Math.floor(ns.length / 2)] / 12)
            octSet = true
        }
    }
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
        anchors { fill: parent; margins: 10 }
        spacing: 10

        Panel {
            title: "SEQUENCER"
            // the step time the ESP really uses: lets you check the tempo (120 BPM = 500 ms)
            subtitle: (device.seqState.step_ms ? Math.round(device.seqState.step_ms) + " ms/step · " : "")
                      + (device.seqState.tr ? "transpose " + (device.seqState.tr > 0 ? "+" : "") + device.seqState.tr
                                           + " (C3 = " + device.noteName(48 + device.seqState.tr) + ")" : "transpose 0")
            RowLayout {
                Layout.fillWidth: true
                Button {
                    leftPadding: 4; rightPadding: 4; font.pixelSize: 13   // small padding: on a phone the full word must fit
                    text: page.running ? "STOP" : "PLAY"; Layout.fillWidth: true; Layout.preferredWidth: 1
                    highlighted: page.running
                    onClicked: device.seqRun(!page.running)
                }
                Button {
                    leftPadding: 4; rightPadding: 4; font.pixelSize: 13; text: "TRIG"; Layout.fillWidth: true; Layout.preferredWidth: 1; onClicked: device.seqTrig() }
                // RETRIG = a new gate every step, LEGATO = one gate per loop (GATE knob unused)
                Button {
                    leftPadding: 4; rightPadding: 4; font.pixelSize: 13
                    text: "RETRIG"; checkable: true; checked: page.pat.mode !== "legato"; Layout.fillWidth: true; Layout.preferredWidth: 1
                    onClicked: device.seqSet({ mode: "retrig" })
                }
                Button {
                    leftPadding: 4; rightPadding: 4; font.pixelSize: 13
                    text: "LEGATO"; checkable: true; checked: page.pat.mode === "legato"; Layout.fillWidth: true; Layout.preferredWidth: 1
                    onClicked: device.seqSet({ mode: "legato" })
                }
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
            title: "PIANO ROLL"; Layout.fillHeight: true
            subtitle: page.selStep >= 0 ? "step " + (page.selStep + 1) + " selected: keys write notes" : "keys play transposed"

            // A phone is too small for 16 steps x 2 octaves, so the roll is a WINDOW onto the
            // pattern: 8 steps (1-8 or 9-16) and one octave (C..C, 13 rows) at a time.
            RowLayout {
                Layout.fillWidth: true; spacing: 4
                Button {
                    leftPadding: 4; rightPadding: 4; font.pixelSize: 13
                    text: "1-8"; checkable: true; checked: page.stepPage === 0; Layout.fillWidth: true
                    visible: (page.pat.length || 8) > 8
                    font.bold: page.playPos >= 0 && page.playPos < 8     // bold = the running light is here
                    onClicked: page.stepPage = 0
                }
                Button {
                    leftPadding: 4; rightPadding: 4; font.pixelSize: 13
                    text: "9-16"; checkable: true; checked: page.stepPage === 1; Layout.fillWidth: true
                    visible: (page.pat.length || 8) > 8
                    font.bold: page.playPos >= 8
                    onClicked: page.stepPage = 1
                }
                Item { Layout.fillWidth: true; visible: (page.pat.length || 8) <= 8 }
                Button {
                    leftPadding: 4; rightPadding: 4; font.pixelSize: 13; text: "OCT -"; onClicked: page.octLo = Math.max(0, page.octLo - 12); Layout.preferredWidth: 72 }
                Label {
                    text: device.noteName(page.octLo) + "-" + device.noteName(page.octLo + 12)
                    horizontalAlignment: Text.AlignHCenter; Layout.preferredWidth: 64; color: "#e6e6e6"
                }
                Button {
                    leftPadding: 4; rightPadding: 4; font.pixelSize: 13; text: "OCT +"; onClicked: page.octLo = Math.min(108, page.octLo + 12); Layout.preferredWidth: 72 }
            }

            Canvas {
                id: roll
                Layout.fillWidth: true
                Layout.fillHeight: true
                readonly property int lo: page.octLo
                readonly property int hi: page.octLo + 12
                readonly property int first: (page.pat.length || 8) > 8 ? page.stepPage * 8 : 0   // first step shown
                readonly property real labelW: 34
                readonly property real headH: 26
                readonly property real colW: (width - labelW) / 8
                readonly property real rowH: (height - headH) / 13
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
                onFirstChanged: requestPaint()
                onLoChanged: requestPaint()
                Connections { target: page; function onSelStepChanged() { roll.requestPaint() } }

                function rowOf(n) { return hi - n }   // top row = highest note
                function black(n) { return [1, 3, 6, 8, 10].includes(n % 12) }

                onPaint: {
                    const g = getContext("2d"), len = page.pat.length || 8, leg = page.pat.mode === "legato"
                    g.reset()
                    g.fillStyle = "#0d0e10"; g.fillRect(0, 0, width, height)
                    g.font = "11px sans-serif"; g.textBaseline = "middle"
                    // note rows, with a piano key strip on the left
                    for (let n = hi; n >= lo; n--) {
                        const y = headH + rowOf(n) * rowH
                        g.fillStyle = black(n) ? "#15171a" : "#1c1f23"; g.fillRect(labelW, y, width - labelW, rowH - 1)
                        g.fillStyle = black(n) ? "#2a2a2a" : "#d8d8d8"; g.fillRect(0, y, labelW - 2, rowH - 1)
                        g.fillStyle = black(n) ? "#aaa" : "#333"; g.fillText(device.noteName(n), 3, y + rowH / 2)
                    }
                    for (let c = 0; c < 8; c++) {
                        const i = first + c, x = labelW + c * colW
                        // step header (tap to select)
                        g.fillStyle = i === page.selStep ? "#f0a020" : i === page.playPos ? "#59ff7a" : "#24272c"
                        g.fillRect(x + 1, 0, colW - 2, headH - 3)
                        g.fillStyle = i === page.selStep || i === page.playPos ? "#000" : "#8a929c"
                        g.textAlign = "center"; g.fillText(i + 1, x + colW / 2, headH / 2); g.textAlign = "start"
                        if (c % 4 === 0) { g.fillStyle = "#3b4048"; g.fillRect(x, headH, 1, height - headH) }   // beat groups
                        if (i === page.playPos) { g.fillStyle = "rgba(89,255,122,0.15)"; g.fillRect(x, headH, colW, height - headH) }
                        if (i >= len) { g.fillStyle = "rgba(0,0,0,0.6)"; g.fillRect(x, headH, colW, height - headH) }   // greyed out
                        const s = page.steps[i]
                        if (!s || !s.on) continue
                        const col = i < len ? (leg ? "#4ea1ff" : "#f0a020") : "#555"
                        if (s.n >= lo && s.n <= hi) {   // the block of this step
                            const w = leg ? colW : colW * (page.pat.gate || 0.5)
                            g.fillStyle = col
                            g.fillRect(x + 2, headH + rowOf(s.n) * rowH + 2, Math.max(6, w - 4), rowH - 5)
                            if (w > 30) { g.fillStyle = "#111"; g.fillText(device.noteName(s.n), x + 5, headH + rowOf(s.n) * rowH + rowH / 2) }
                        } else {                        // outside this octave: an arrow at the edge
                            const up = s.n > hi, y = up ? headH + 2 : height - 12
                            g.fillStyle = col; g.textAlign = "center"
                            g.fillText((up ? "^ " : "v ") + device.noteName(s.n), x + colW / 2, y + 5); g.textAlign = "start"
                        }
                    }
                }

                // Touch: which step and note is under the finger?
                MouseArea {
                    anchors.fill: parent
                    preventStealing: true          // keep the whole drag, even when the finger moves fast
                    property int step: -1
                    property int note0: 0
                    property real y0: 0
                    property bool had: false
                    property bool moved: false
                    onPressed: mouse => {
                        const c = Math.floor((mouse.x - roll.labelW) / roll.colW)
                        if (c < 0 || c > 7) { step = -1; return }
                        step = roll.first + c
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
                        roll.requestPaint()
                        step = -1
                    }
                }
            }
            Keyboard {
                Layout.fillWidth: true; Layout.preferredHeight: 80
                onPressedKey: n => {
                    if (page.selStep >= 0) {               // step entry
                        page.steps[page.selStep].n = n; page.steps[page.selStep].on = true
                        page.sendSteps(); page.preview(n)
                        page.selStep = (page.selStep + 1) % (page.pat.length || 8)
                        page.stepPage = page.selStep >= 8 ? 1 : 0     // follow the selected step
                        roll.requestPaint()
                    } else {
                        device.seqTrig(n)                  // play the sequence from this key
                    }
                }
            }
        }
    }
}
