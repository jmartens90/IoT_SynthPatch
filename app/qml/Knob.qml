// Knob.qml - a synth knob: drag up/down (or turn the mouse wheel), double-tap = default value.
//
// QML lesson: a reusable COMPONENT. This file defines a new type "Knob" that the pages use like
// a built-in item:  Knob { label: "RATE"; from: 0.01; to: 50; logScale: true; value: ... }
// `signal moved(real v)` tells the page when the user turned it; the page sends it to the device.
import QtQuick

Item {
    id: knob
    property string label
    property real from: 0
    property real to: 1
    property real value: 0
    property real defaultValue: from
    property bool logScale: false        // rate and times: small values get as much room as big ones
    property bool bipolar: false         // arc drawn from the middle (offset around 1.65 V)
    property var format: v => v.toFixed(2)
    property color accent: "#f0a020"
    signal moved(real v)

    implicitWidth: 68
    implicitHeight: 96

    // value <-> knob position 0..1
    function toPos(v) { return logScale ? Math.log(v / from) / Math.log(to / from) : (v - from) / (to - from) }
    function fromPos(x) { x = Math.max(0, Math.min(1, x)); return logScale ? from * Math.pow(to / from, x) : from + x * (to - from) }
    // While turning we show our own value at once and send at most 20 changes per second (each
    // one is a Bluetooth message). `value` stays bound to the device and follows by itself.
    property real shown: value
    property bool busy: false
    function turnTo(pos) { shown = fromPos(pos); busy = true; if (!send.running) send.start() }
    Timer { id: send; interval: 50; onTriggered: { knob.moved(knob.shown); knob.busy = false } }
    onValueChanged: if (!busy) shown = value
    onShownChanged: dial.requestPaint()

    Canvas {
        id: dial
        width: 64; height: 64
        anchors.horizontalCenter: parent.horizontalCenter
        onPaint: {
            const g = getContext("2d"), c = 32, r = 26
            const a0 = 0.75 * Math.PI, a1 = 2.25 * Math.PI          // 270 degrees, gap at the bottom
            const pos = Math.max(0, Math.min(1, knob.toPos(knob.shown)))
            const a = a0 + pos * (a1 - a0)
            g.reset()
            g.lineWidth = 4; g.lineCap = "round"
            g.strokeStyle = "#3b4048"; g.beginPath(); g.arc(c, c, r, a0, a1); g.stroke()
            g.strokeStyle = knob.accent; g.beginPath()
            const start = knob.bipolar ? 1.5 * Math.PI : a0
            g.arc(c, c, r, Math.min(start, a), Math.max(start, a)); g.stroke()
            g.fillStyle = "#15171a"; g.beginPath(); g.arc(c, c, 20, 0, 2 * Math.PI); g.fill()
            g.strokeStyle = "#e6e6e6"; g.lineWidth = 3; g.beginPath()
            g.moveTo(c + 6 * Math.cos(a), c + 6 * Math.sin(a)); g.lineTo(c + 18 * Math.cos(a), c + 18 * Math.sin(a)); g.stroke()
        }
    }
    Text {
        id: valueText
        anchors { top: dial.bottom; horizontalCenter: parent.horizontalCenter }
        text: knob.format(knob.shown)
        color: knob.accent; font.family: "monospace"; font.pixelSize: 12
    }
    Text {
        anchors { top: valueText.bottom; horizontalCenter: parent.horizontalCenter }
        text: knob.label; color: "#8a929c"; font.pixelSize: 11; font.letterSpacing: 1.5
    }

    MouseArea {
        anchors.fill: parent
        property real y0
        property real p0
        onPressed: m => { y0 = m.y; p0 = knob.toPos(knob.shown) }
        onPositionChanged: m => knob.turnTo(p0 + (y0 - m.y) / 180)       // 180 px = the whole range
        onDoubleClicked: knob.turnTo(knob.toPos(knob.defaultValue))
        onWheel: w => knob.turnTo(knob.toPos(knob.shown) + (w.angleDelta.y > 0 ? 0.03 : -0.03))
    }
}
