// Panel.qml - a module panel with a title, like the boxes on the web page.
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: panel
    property string title
    property string subtitle
    default property alias content: inner.data   // children go into the column below the title
    color: "#24272c"; radius: 10; border.color: "#3b4048"
    implicitHeight: col.implicitHeight + 24
    Layout.fillWidth: true

    ColumnLayout {
        id: col
        anchors { fill: parent; margins: 12 }   // fills the panel: a canvas inside can take the spare height
        spacing: 8
        RowLayout {
            Layout.fillWidth: true
            Text { text: panel.title; color: "#e6e6e6"; font.bold: true; font.letterSpacing: 2; font.pixelSize: 13 }
            Item { Layout.fillWidth: true }
            Text { text: panel.subtitle; color: "#8a929c"; font.pixelSize: 11 }
        }
        ColumnLayout { id: inner; Layout.fillWidth: true; Layout.fillHeight: true; spacing: 8 }
    }
}
