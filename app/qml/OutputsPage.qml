// OutputsPage.qml - what the three DACs send out: the last 10 s of LFO, envelope and V/oct
// (as note blocks, like a piano roll), and the log of every note sent to the V/oct DAC.
// The ESP samples the outputs every 20 ms (in its 1 ms engine task, so exactly) and pushes
// them about 10 at a time ("cv" messages, numbered, so a lost one shows as a gap). The plot is
// ScopeView (src/ScopeView.cpp): a QML Canvas repainting the whole area 30-60x per second was
// too slow for a phone.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SynthPatch   // ScopeView (C++)

Item {   // no scrolling: everything fits the screen, so dragging a knob never moves the page
    id: page
    readonly property bool shown: SwipeView.isCurrentItem   // only draw when you can see it

    ColumnLayout {
        anchors { fill: parent; margins: 10 }
        spacing: 10

        Panel {
            Layout.fillHeight: true
            title: "OUTPUTS"; subtitle: "last 10 s, every 20 ms"
            Item {   // the plot: ScopeView (C++, drawn by the GPU) + lane titles + note names
                id: plot
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 150
                clip: true
                Rectangle { anchors.fill: parent; color: "#0d0e10" }
                ScopeView {
                    id: scope
                    anchors.fill: parent
                    source: page.shown ? device : null   // hidden page: not even listening
                    span: 500                            // 10 s at 20 ms
                }
                // Smooth scrolling: the samples come in packs of 10 (every ~200 ms, give or take
                // what the radio does). A play head moves on with the phone's clock at every
                // screen frame (FrameAnimation = in step with the display), 20 samples (400 ms)
                // behind the newest, so a pack that comes late does not make the plot stop.
                FrameAnimation {
                    running: page.shown && device.cvEnd > 0
                    onTriggered: {
                        const end = device.cvEnd, lag = 20, s = frameTime * 1000 / device.cvDt   // this frame in samples
                        if (scope.head <= 0 || scope.head < end - 4 * lag || scope.head > end) scope.head = end - lag
                        else scope.head = Math.min(end, scope.head + s + (end - lag - scope.head) * Math.min(1, 0.3 * frameTime))
                    }
                }
                Repeater {   // the note names on the piano-roll blocks; they slide with the plot
                    model: scope.labels
                    Text {
                        // same as ScopeView::xOf, written out so the binding follows head and width
                        readonly property real pps: scope.width / (scope.span - 1)
                        readonly property real x0: scope.width - (scope.head - modelData.k0) * pps
                        readonly property real x1: scope.width - (scope.head - modelData.k1) * pps
                        visible: x1 - x0 > 22 && x1 > 0
                        x: x0 + 3; y: modelData.y + modelData.h - height - 1
                        text: modelData.text; color: "#001"; font.pixelSize: Math.min(11, modelData.h - 1)
                    }
                }
                Rectangle { y: plot.height * .25; width: plot.width; height: 1; color: "#222" }
                Rectangle { y: plot.height * .5; width: plot.width; height: 1; color: "#222" }
                Text { x: 4; y: 2; text: "LFO 0..3.3 V"; color: "#667"; font.pixelSize: 10 }
                Text { x: 4; y: plot.height * .25 + 2; text: "ENV 0..3.3 V"; color: "#667"; font.pixelSize: 10 }
                Text { x: 4; y: plot.height * .5 + 2; text: "V/oct"; color: "#667"; font.pixelSize: 10 }
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
