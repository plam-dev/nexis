import QtQuick
import QtQuick.Shapes

Item {
    id: notch
    property color color: "#000000"
    readonly property real shoulder: Math.min(24, width / 8, height / 3)
    readonly property real bottomRadius: Math.min(28, height / 2)
    Shape {
        anchors.fill: parent
        antialiasing: true
        ShapePath {
            strokeWidth: 0
            strokeColor: "transparent"
            fillColor: notch.color
            startX: 0; startY: 0
            PathLine { x: notch.width; y: 0 }
            PathCubic { x: notch.width - notch.shoulder; y: notch.shoulder; control1X: notch.width - notch.shoulder; control1Y: 0; control2X: notch.width - notch.shoulder; control2Y: notch.shoulder / 2 }
            PathLine { x: notch.width - notch.shoulder; y: notch.height - notch.bottomRadius }
            PathQuad { x: notch.width - notch.shoulder - notch.bottomRadius; y: notch.height; controlX: notch.width - notch.shoulder; controlY: notch.height }
            PathLine { x: notch.shoulder + notch.bottomRadius; y: notch.height }
            PathQuad { x: notch.shoulder; y: notch.height - notch.bottomRadius; controlX: notch.shoulder; controlY: notch.height }
            PathLine { x: notch.shoulder; y: notch.shoulder }
            PathCubic { x: 0; y: 0; control1X: notch.shoulder; control1Y: notch.shoulder / 2; control2X: notch.shoulder; control2Y: 0 }
        }
    }
}
