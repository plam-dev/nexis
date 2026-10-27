import QtQuick

Rectangle {
    id: art
    property url source: ""
    property color fallbackColor: "#9fbce4"
    readonly property bool fallbackVisible: cover.status !== Image.Ready
    implicitWidth: 30
    implicitHeight: 30
    radius: Math.min(9, width / 5)
    color: "#2e3b50"
    clip: true
    Image {
        id: cover
        anchors.fill: parent
        source: art.source
        asynchronous: true
        sourceSize: Qt.size(128, 128)
        fillMode: Image.PreserveAspectCrop
        visible: status === Image.Ready
    }
    Text {
        anchors.centerIn: parent
        text: "♪"
        textFormat: Text.PlainText
        color: art.fallbackColor
        font.pixelSize: Math.round(art.width * 0.55)
        visible: art.fallbackVisible
    }
    Accessible.role: Accessible.Graphic
    Accessible.name: art.fallbackVisible ? "Artwork unavailable" : "Album artwork"
}
