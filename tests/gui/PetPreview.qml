import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import "../../ui"
Window {
    width: 920; height: 300; visible: true; color: "#282828"
    Column {
        id: preview
        anchors.fill: parent
        spacing: 20
        Text { x: 26; y: 15; height: 45; text: "Your tuxedo + tabby companions"; color: "#dfdfdf"; font.pixelSize: 23; font.bold: true }
        Row {
            x: 22
            spacing: 20
            Repeater {
                model: [{label: "Idle",working:false,listening:false},{label: "Working",working:true,listening:false},{label: "Music",working:false,listening:true},{label: "Music + working",working:true,listening:true}]
                delegate: Column {
                    required property var modelData
                    spacing: 16
                    Rectangle {
                        width: 200; height: 145; radius: 16; color: "#303030"
                        Row { anchors.centerIn: parent; spacing: 0
                            TuxedoCat { width: 90; height: 80; working: modelData.working; listening: modelData.listening; reducedMotion: true }
                            TabbyCat { width: 90; height: 80; working: modelData.working; listening: modelData.listening; reducedMotion: true }
                        }
                    }
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.label; color: "#dfdfdf"; font.pixelSize: 15 }
                }
            }
        }
    }
    Timer { interval: 700; running: true; onTriggered: preview.grabToImage(result => { result.saveToFile(Qt.resolvedUrl("../../docs/tuxedo-states.png").toString().replace("file://", "")); Qt.quit() }) }
}
