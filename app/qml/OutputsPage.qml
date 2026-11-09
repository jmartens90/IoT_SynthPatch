// OutputsPage.qml - what the three DACs send out: the last 10 s of LFO, envelope and V/oct
// (as note blocks, like a piano roll), and the log of every note sent to the V/oct DAC.
// The ESP samples the outputs every 20 ms (in its 1 ms engine task, so exactly) and pushes
// them about 10 at a time ("cv" messages, numbered, so a lost one shows as a gap).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {   // no scrolling: everything fits the screen, so dragging a knob never moves the page
    id: page
    readonly property bool shown: SwipeView.isCurrentItem   // only draw when you can see it

    ColumnLayout {
        anchors { fill: parent; margins: 10 }
        spacing: 10

        Panel {
            Layout.fillHeight: true
            title: "OUTPUTS"; subtitle: "last 10 s, every 20 ms"
            Canvas {
                id: cvs
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 150
                // Smooth scrolling: the samples come in packs of 10 (every ~200 ms, give or take
                // what Bluetooth does). Drawing each pack as it lands made the plot jump 200 ms at
                // a time, unevenly. Instead a play head moves on with the clock, ~1 pack behind the
                // newest sample, and the plot is redrawn at 30 fps. Positions are ESP sample
                // numbers (device.cv.end = after the newest), so every sample sits at its own time.
                property real head: -1      // sample number at the right edge
                property double lastMs: 0
                readonly property int lag: 20   // samples behind the newest (400 ms): room for late Bluetooth packs
                // device.cv is a C++ map: every read of it makes a fresh JS copy of all 1500 samples.
                // Read 30x per second, that garbage made the JavaScript engine pause to clean up,
                // at regular intervals (the stutter). So it is copied once per new pack, here,
                // together with everything the drawing needs that only changes with the data.
                property var cv: ({})
                property var notesN: []          // V/oct as MIDI note numbers (NaN = gap)
                property int nlo: 0; property int nhi: 0
                function takeCv() {
                    const c = device.cv, a = c.voct || []
                    const ns = a.map(v => isNaN(v) ? NaN : Math.round(v * 12) + 36)
                    let lo = 999, hi = -999
                    for (const n of ns) if (!isNaN(n)) { lo = Math.min(lo, n); hi = Math.max(hi, n) }
                    cv = c; notesN = ns; nlo = lo - 1; nhi = hi + 1
                }
                Connections { target: device; function onCvChanged() { if (page.shown) cvs.takeCv() } }
                Connections { target: page; function onShownChanged() { if (page.shown) cvs.takeCv() } }
                Timer {
                    interval: 33; repeat: true; running: page.shown && (cvs.cv.end || 0) > 0
                    onTriggered: {
                        const now = Date.now(), end = cvs.cv.end, dt = cvs.cv.dt || 20
                        if (cvs.head < 0 || cvs.head < end - 4 * cvs.lag || cvs.head > end) cvs.head = end - cvs.lag   // start / after a stall
                        else {
                            cvs.head += (now - cvs.lastMs) / dt
                            cvs.head += (end - cvs.lag - cvs.head) * 0.03   // stay ~lag behind, gently
                            cvs.head = Math.min(cvs.head, end)
                        }
                        cvs.lastMs = now
                        cvs.requestPaint()
                    }
                }
                onPaint: {
                    const g = getContext("2d"), W = width, Hh = height, N = 500, CV = cvs.cv
                    const hd = cvs.head >= 0 ? cvs.head : (CV.end || 0)
                    const first = (CV.end || 0) - (CV.lfo || []).length   // sample number of a[0]
                    const X = i => W - (hd - (first + i)) / (N - 1) * W   // i = index in the arrays
                    g.reset(); g.fillStyle = "#0d0e10"; g.fillRect(0, 0, W, Hh); g.font = "10px sans-serif"
                    const lanes = [["lfo", "#f0a020", "LFO 0..3.3 V", 0, Hh * .25], ["env", "#59ff7a", "ENV 0..3.3 V", Hh * .25, Hh * .25],
                                   ["voct", "#4ea1ff", "V/oct", Hh * .5, Hh * .5]]
                    const env = CV.env || []
                    for (const [k, col, label, top, lane] of lanes) {
                        const a = CV[k] || []
                        if (top) { g.strokeStyle = "#222"; g.beginPath(); g.moveTo(0, top); g.lineTo(W, top); g.stroke() }
                        g.save(); g.beginPath(); g.rect(0, top, W, lane); g.clip()
                        if (k === "voct") {
                            // piano roll: a block while the envelope is open (the note sounds), a faint
                            // line while V/oct only holds its last value. NaN (a gap) draws nothing.
                            const ns = cvs.notesN
                            if (cvs.nhi > cvs.nlo) {
                                const nlo = cvs.nlo, nhi = cvs.nhi, rh = (lane - 16) / (nhi - nlo + 1)
                                const YN = n => top + 14 + (nhi - n) * rh, on = i => (env[i] || 0) > .02
                                const same = (i, j) => (isNaN(ns[i]) && isNaN(ns[j])) || (ns[j] === ns[i] && on(j) === on(i))
                                for (let i = 0; i < ns.length;) {
                                    let j = i
                                    while (j < ns.length && same(i, j)) j++
                                    if (!isNaN(ns[i])) {
                                        const x0 = X(i), x1 = X(j), y = YN(ns[i]), h = Math.max(3, rh - 2)
                                        if (on(i)) {
                                            g.fillStyle = col; g.fillRect(x0 + 1, y, Math.max(2, x1 - x0 - 2), h)
                                            if (x1 - x0 > 22 && h > 8) { g.fillStyle = "#001"; g.fillText(device.noteName(ns[i]), x0 + 3, y + h - 2) }
                                        } else { g.fillStyle = "rgba(78,161,255,0.25)"; g.fillRect(x0, y + h / 2 - 1, x1 - x0, 2) }
                                    }
                                    i = j
                                }
                            }
                        } else {
                            const Y = v => top + lane - 4 - v / 3.3 * (lane - 8)
                            g.strokeStyle = col; g.lineWidth = 1.5; g.beginPath()
                            let pen = false   // a NaN lifts the pen: a gap, not a line across it
                            a.forEach((v, i) => {
                                if (isNaN(v)) { pen = false; return }
                                if (pen) g.lineTo(X(i), Y(v)); else g.moveTo(X(i), Y(v))
                                pen = true
                            })
                            g.stroke()
                        }
                        g.restore()
                        g.fillStyle = "#667"; g.fillText(label, 4, top + 12)
                    }
                }
            }
            Text { text: "NOTE LOG (V/oct DAC)"; color: "#8a929c"; font.pixelSize: 11; font.letterSpacing: 1 }
            ListView {   // newest first; shows as many lines as fit, no scrolling
                Layout.fillWidth: true; Layout.preferredHeight: 150
                interactive: false; clip: true
                model: device.notes.slice(-12).reverse()   // only what fits: fewer lines to rebuild per note
                delegate: Text {
                    font.family: "monospace"; font.pixelSize: 11; color: "#c8ccd2"
                    text: (modelData.ms / 1000).toFixed(2).padStart(8) + " s  " + device.noteName(modelData.note).padEnd(4)
                          + modelData.v.toFixed(3) + " V  code " + String(modelData.code).padStart(5) + "  " + modelData.src
                }
            }
            Text { visible: device.notes.length === 0; text: "no notes yet"; color: "#5a616b"; font.pixelSize: 11 }
        }
    }
}
