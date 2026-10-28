import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid
import "Nexis/Panel" as Native

PlasmoidItem {
    id: root
    Plasmoid.backgroundHints: PlasmaCore.Types.NoBackground
    preferredRepresentation: compactRepresentation
    switchWidth: 10000
    switchHeight: 10000
    toolTipMainText: "Tuxedo · " + (client.connected ? (state.title || "Your desktop companion") : "Start NEXIS to connect your apps")
    toolTipSubText: "Music, notifications and agent activity"
    readonly property var state: client.data.presentation || ({mode: "idle", working: false, listening: false, title: ""})
    readonly property var track: client.data.media || ({})
    readonly property var options: client.data.options || ({})
    readonly property bool lightTheme: client.data.darkTheme === false
    Native.DesktopClient { id: client }
    onExpandedChanged: client.action("expanded", root.expanded)
    compactRepresentation: Item {
        id: compact
        property real capsuleWidth: face.implicitWidth
        Layout.minimumWidth: capsuleWidth
        Layout.preferredWidth: capsuleWidth
        Layout.maximumWidth: capsuleWidth
        Layout.fillHeight: true
        Behavior on capsuleWidth { NumberAnimation { duration: root.options.reducedMotion ? 0 : 250; easing.type: Easing.OutCubic } }
        Rectangle { anchors.fill: parent; anchors.topMargin: 2; anchors.bottomMargin: 2; radius: height / 2; color: root.lightTheme ? "#f1f4f9" : "#171b23"; border.color: "#414b5b" }
        CompactActivity { id: face; anchors.fill: parent; activity: root.state; track: root.track; showIdleClock: !!root.options.idleClock; petKind: root.options.pet || "tuxedo"; foreground: root.lightTheme ? "#243248" : "#edf2fb"; muted: root.lightTheme ? "#5b6c83" : "#9daabe"; accent: "#8bbcff"; reducedMotion: !!root.options.reducedMotion }
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.MiddleButton
            onClicked: mouse => { if (mouse.button === Qt.MiddleButton) client.action("playPause"); else root.expanded = !root.expanded }
        }
        activeFocusOnTab: true
        Keys.onSpacePressed: root.expanded = !root.expanded
        Keys.onReturnPressed: root.expanded = !root.expanded
        Accessible.role: Accessible.Button
        Accessible.name: face.displayText + ". Open island."
    }
    fullRepresentation: Item {
        Layout.minimumWidth: 376
        Layout.preferredWidth: 376
        Layout.maximumWidth: 376
        Layout.minimumHeight: surface.implicitHeight
        Layout.preferredHeight: surface.implicitHeight
        Layout.maximumHeight: surface.implicitHeight
        implicitWidth: 376
        implicitHeight: surface.implicitHeight
        FoldSurface {
            id: surface
            anchors.fill: parent
            snapshot: client.data
            maximumHeight: Math.max(180, Screen.height - 90)
            onAction: (name, value) => client.action(name, value)
            onCloseRequested: root.expanded = false
        }
    }
}
