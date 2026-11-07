// Main.qml - the window: a status bar on top, four pages you swipe between.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 420; height: 860           // phone-like on a PC; full screen on Android
    visible: true
    title: "SynthPatch"
    color: "#16181b"
    Material.theme: Material.Dark
    Material.accent: "#f0a020"

    header: ToolBar {
        Material.background: "#0f1012"
        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 8
            Text { text: "SYNTHPATCH"; color: "#e6e6e6"; font.bold: true; font.letterSpacing: 4 }
            Rectangle { width: 10; height: 10; radius: 5; color: device.connected ? "#59ff7a" : "#ff4d4d" }
            Text { text: device.status; color: "#8a929c"; font.pixelSize: 12; elide: Text.ElideRight; Layout.fillWidth: true }
            ToolButton { text: "↻"; onClicked: device.connected ? device.refreshAll() : device.start() }
        }
    }

    footer: TabBar {
        id: tabs
        currentIndex: pages.currentIndex
        TabButton { text: "Synth" }
        TabButton { text: "Seq" }
        TabButton { text: "Patches" }
        TabButton { text: "Monitor" }
    }

    SwipeView {
        id: pages
        anchors.fill: parent
        currentIndex: tabs.currentIndex
        interactive: false            // swiping would fight with the knobs and the piano roll
        // "--page 2" on the command line opens that page (0..3) first
        Component.onCompleted: {
            const a = Qt.application.arguments, i = a.indexOf("--page")
            if (i >= 0 && i + 1 < a.length) tabs.currentIndex = Number(a[i + 1])
        }
        SynthPage {}
        SeqPage {}
        PatchPage {}
        MonitorPage {}
    }

    // Short messages from the device ("saved", "loaded A3") at the bottom for 2 s.
    ToolTip { id: toast; timeout: 2000; y: win.height - 160; x: (win.width - width) / 2 }
    Connections { target: device; function onInfo(text) { toast.show(text) } }
}
