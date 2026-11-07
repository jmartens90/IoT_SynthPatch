// PatchPage.qml - patch memory: 4 banks (A..D) of 8 slots, like on the web page.
//
// Tap a slot to select it (its name goes into the text field), then LOAD, SAVE or DELETE.
// A patch holds the LFO, envelope AND the sequence, stored on the ESP (LittleFS).
//
// QML lesson: `device.patches` is a list from C++ ([{slot,label,name}]). A Repeater turns each
// entry into a button; `modelData` is that entry. When C++ emits patchesChanged after a save,
// the Repeater rebuilds the buttons by itself.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {   // no scrolling: everything fits the screen, so dragging a knob never moves the page
    id: page
    property int bank: 0
    property int slot: -1
    readonly property var current: slot >= 0 ? (device.patches[slot] || {}) : {}
    readonly property bool empty: !current.name

    ColumnLayout {
        anchors { fill: parent; margins: 10 }
        spacing: 10

        Panel {
            title: "PATCH MEMORY"
            subtitle: page.slot >= 0 ? (page.current.label || "") + (page.empty ? " (empty)" : "") : "choose a slot"

            RowLayout {
                Layout.fillWidth: true
                Repeater {
                    model: ["A", "B", "C", "D"]
                    Button {
                        text: "BANK " + modelData; checkable: true; checked: page.bank === index
                        Layout.fillWidth: true; font.pixelSize: 11
                        onClicked: { page.bank = index; page.slot = -1 }
                    }
                }
            }

            GridLayout {
                columns: 2; Layout.fillWidth: true; columnSpacing: 8; rowSpacing: 8
                Repeater {
                    model: device.patches.slice(page.bank * 8, page.bank * 8 + 8)
                    Rectangle {
                        Layout.fillWidth: true; Layout.preferredHeight: 52; radius: 6
                        color: page.slot === modelData.slot ? "#3a2d12" : "#1c1f23"
                        border.color: page.slot === modelData.slot ? "#f0a020" : "#3b4048"
                        Column {
                            anchors { left: parent.left; verticalCenter: parent.verticalCenter; leftMargin: 10 }
                            Text { text: modelData.label; color: "#f0a020"; font.bold: true; font.pixelSize: 12 }
                            Text {
                                text: modelData.name || "empty"; color: modelData.name ? "#e6e6e6" : "#5a616b"
                                font.pixelSize: 13; elide: Text.ElideRight; width: 140
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: { page.slot = modelData.slot; nameField.text = modelData.name || "" }
                            onDoubleClicked: if (modelData.name) device.loadPatch(modelData.slot)   // quick load
                        }
                    }
                }
            }

            TextField {
                id: nameField; Layout.fillWidth: true
                placeholderText: "patch name"; maximumLength: 24
                enabled: page.slot >= 0
            }
            RowLayout {
                Layout.fillWidth: true
                Button {
                    text: "LOAD"; Layout.fillWidth: true; enabled: page.slot >= 0 && !page.empty
                    onClicked: device.loadPatch(page.slot)
                }
                Button {
                    text: "SAVE"; Layout.fillWidth: true; highlighted: true
                    enabled: page.slot >= 0 && nameField.text.trim().length > 0
                    // saving over a patch replaces it: ask first
                    onClicked: page.empty ? device.savePatch(page.slot, nameField.text.trim()) : overwrite.open()
                }
                Button {
                    text: "DELETE"; Layout.fillWidth: true; enabled: page.slot >= 0 && !page.empty
                    onClicked: remove.open()
                }
            }
            Text {
                text: "Tap a slot to choose it, double-tap to load it right away.\nA patch stores the LFO, the envelope and the sequence."
                color: "#8a929c"; font.pixelSize: 11; wrapMode: Text.Wrap; Layout.fillWidth: true
            }
        }
        Item { Layout.fillHeight: true }   // pushes the panel to the top
    }

    Dialog {
        id: overwrite; anchors.centerIn: parent; modal: true
        title: "Replace " + (page.current.label || "") + "?"
        Label { text: "\"" + (page.current.name || "") + "\" will be overwritten by \"" + nameField.text.trim() + "\"." ; wrapMode: Text.Wrap }
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: device.savePatch(page.slot, nameField.text.trim())
    }
    Dialog {
        id: remove; anchors.centerIn: parent; modal: true
        title: "Delete " + (page.current.label || "") + "?"
        Label { text: "\"" + (page.current.name || "") + "\" is removed from the ESP." }
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: { device.deletePatch(page.slot); nameField.text = "" }
    }
}
