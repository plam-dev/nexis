import QtQuick
import QtQuick.Window

Item {
    id: pulse
    property bool playing: false
    property bool reducedMotion: false
    property color color: "#8bbcff"
    readonly property bool animating: visible && playing && !reducedMotion && (!Window.window || (Window.window.visible && Window.window.visibility !== Window.Minimized))
    implicitWidth: 17
    implicitHeight: 20
    Accessible.ignored: true
    Row {
        anchors.centerIn: parent
        spacing: 2
        Repeater {
            model: [10, 18, 8, 14]
            delegate: Rectangle {
                required property int index
                required property int modelData
                width: 2
                height: pulse.playing ? modelData : 3
                y: (20 - height) / 2
                radius: 1
                color: pulse.color
                transform: Scale { id: amplitude; origin.y: parent.height / 2; yScale: 1 }
                SequentialAnimation {
                    running: pulse.animating
                    loops: Animation.Infinite
                    NumberAnimation { target: amplitude; property: "yScale"; from: 0.35; to: 1; duration: 260 + index * 65; easing.type: Easing.InOutSine }
                    NumberAnimation { target: amplitude; property: "yScale"; from: 1; to: 0.35; duration: 310 + index * 45; easing.type: Easing.InOutSine }
                    onRunningChanged: if (!running) amplitude.yScale = 1
                }
            }
        }
    }
}
