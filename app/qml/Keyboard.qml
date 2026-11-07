// Keyboard.qml - two octaves C3..C5 (MIDI 48..72), white keys with black keys on top.
//
// QML lesson: the black keys are a second Repeater drawn AFTER the white keys, so they sit on top
// (later siblings are painted over earlier ones). x is computed from the white key to its left.
import QtQuick

Item {
    id: kb
    property int low: 48
    property int high: 72
    signal pressedKey(int note)
    implicitHeight: 80

    readonly property var whites: { const w = []; for (let n = low; n <= high; n++) if (![1, 3, 6, 8, 10].includes(n % 12)) w.push(n); return w }
    readonly property var blacks: { const b = []; for (let n = low; n <= high; n++) if ([1, 3, 6, 8, 10].includes(n % 12)) b.push(n); return b }
    readonly property real keyW: width / whites.length

    Repeater {
        model: kb.whites
        Rectangle {
            x: index * kb.keyW; width: kb.keyW - 2; height: kb.height
            radius: 3; color: ma.pressed ? "#f0a020" : "#e6e6e6"
            Text { text: modelData % 12 === 0 ? device.noteName(modelData) : ""; anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter; bottomMargin: 4 } font.pixelSize: 9; color: "#555" }
            MouseArea { id: ma; anchors.fill: parent; onPressed: kb.pressedKey(modelData) }
        }
    }
    Repeater {
        model: kb.blacks
        Rectangle {
            // a black key sits on the line between white key (number of whites below it) - 1 and the next
            readonly property int whitesBelow: kb.whites.filter(w => w < modelData).length
            x: whitesBelow * kb.keyW - kb.keyW * 0.3 - 1; width: kb.keyW * 0.6; height: kb.height * 0.6
            radius: 2; color: mb.pressed ? "#f0a020" : "#111"
            MouseArea { id: mb; anchors.fill: parent; onPressed: kb.pressedKey(modelData) }
        }
    }
}
