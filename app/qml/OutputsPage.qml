// OutputsPage.qml - what the three DACs send out: the last 10 s of LFO, envelope and V/oct
// (as note blocks, like a piano roll), and the log of every note sent to the V/oct DAC.
// The ESP samples the outputs every 20 ms and pushes them 10 at a time ("cv" messages).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: page
    contentWidth: availableWidth
    readonly property bool shown: SwipeView.isCurrentItem   // only draw when you can see it

    ColumnLayout {
        width: page.availableWidth
        spacing: 12

        Panel {
            title: "OUTPUTS"; Layout.margins: 12; subtitle: "last 10 s, every 20 ms"
            Canvas {
                id: cvs
                Layout.fillWidth: true; Layout.preferredHeight: 320
                Connections { target: device; function onCvChanged() { if (page.shown) cvs.requestPaint() } }
                onPaint: {
                    const g = getContext("2d"), W = width, Hh = height, N = 500, CV = device.cv
                    const X = i => i / (N - 1) * W
                    g.reset(); g.fillStyle = "#0d0e10"; g.fillRect(0, 0, W, Hh); g.font = "10px sans-serif"
                    const lanes = [["lfo", "#f0a020", "LFO 0..3.3 V", 0, Hh * .25], ["env", "#59ff7a", "ENV 0..3.3 V", Hh * .25, Hh * .25],
                                   ["voct", "#4ea1ff", "V/oct", Hh * .5, Hh * .5]]
                    const env = CV.env || []
                    for (const [k, col, label, top, lane] of lanes) {
                        const a = CV[k] || [], off = N - a.length
                        if (top) { g.strokeStyle = "#222"; g.beginPath(); g.moveTo(0, top); g.lineTo(W, top); g.stroke() }
                        if (k === "voct") {
                            // piano roll: a block while the envelope is open (the note sounds), a faint
                            // line while V/oct only holds its last value
                            const ns = a.map(v => Math.round(v * 12) + 36)
                            if (ns.length) {
                                const nlo = Math.min(...ns) - 1, nhi = Math.max(...ns) + 1, rh = (lane - 16) / (nhi - nlo + 1)
                                const YN = n => top + 14 + (nhi - n) * rh, on = i => (env[i] || 0) > .02
                                for (let i = 0; i < ns.length;) {
                                    let j = i
                                    while (j < ns.length && ns[j] === ns[i] && on(j) === on(i)) j++
                                    const x0 = X(i + off), x1 = X(j + off), y = YN(ns[i]), h = Math.max(3, rh - 2)
                                    if (on(i)) {
                                        g.fillStyle = col; g.fillRect(x0 + 1, y, Math.max(2, x1 - x0 - 2), h)
                                        if (x1 - x0 > 22 && h > 8) { g.fillStyle = "#001"; g.fillText(device.noteName(ns[i]), x0 + 3, y + h - 2) }
                                    } else { g.fillStyle = "rgba(78,161,255,0.25)"; g.fillRect(x0, y + h / 2 - 1, x1 - x0, 2) }
                                    i = j
                                }
                            }
                        } else {
                            const Y = v => top + lane - 4 - v / 3.3 * (lane - 8)
                            g.strokeStyle = col; g.lineWidth = 1.5; g.beginPath()
                            a.forEach((v, i) => i ? g.lineTo(X(i + off), Y(v)) : g.moveTo(X(i + off), Y(v)))
                            g.stroke()
                        }
                        g.fillStyle = "#667"; g.fillText(label, 4, top + 12)
                    }
                }
            }
            Text { text: "NOTE LOG (V/oct DAC)"; color: "#8a929c"; font.pixelSize: 11; font.letterSpacing: 1 }
            Repeater {   // newest first
                model: device.notes.slice().reverse().slice(0, 32)
                Text {
                    font.family: "monospace"; font.pixelSize: 11; color: "#c8ccd2"
                    text: (modelData.ms / 1000).toFixed(2).padStart(8) + " s  " + device.noteName(modelData.note).padEnd(4)
                          + modelData.v.toFixed(3) + " V  code " + String(modelData.code).padStart(5) + "  " + modelData.src
                }
            }
            Text { visible: device.notes.length === 0; text: "no notes yet"; color: "#5a616b"; font.pixelSize: 11 }
        }
    }
}
