import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
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
    toolTipSubText: "Clock, music and activity · Everything stays on this device"
    readonly property var state: client.data.presentation || ({mode: "idle", working: false, listening: false, title: ""})
    readonly property var track: client.data.media || ({})
    readonly property var sessions: client.data.sessions || []
    readonly property var options: client.data.options || ({})
    readonly property color secondary: Kirigami.Theme.disabledTextColor
    Native.DesktopClient { id: client }
    onExpandedChanged: () => client.action("expanded", root.expanded)
    compactRepresentation: Item {
        id: compact
        Layout.minimumWidth: face.implicitWidth
        Layout.preferredWidth: face.implicitWidth
        Layout.maximumWidth: face.implicitWidth
        Layout.fillHeight: true
        CompactActivity {
            id: face
            anchors.fill: parent
            activity: root.state
            foreground: Kirigami.Theme.textColor
            muted: root.secondary
            accent: Kirigami.Theme.highlightColor
            reducedMotion: !!root.options.reducedMotion
        }
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.MiddleButton
            onClicked: mouse => {
                if (mouse.button === Qt.MiddleButton) client.action("playPause")
                else root.expanded = !root.expanded
            }
        }
        Keys.onSpacePressed: root.expanded = !root.expanded
        Keys.onReturnPressed: root.expanded = !root.expanded
        Accessible.role: Accessible.Button
        Accessible.name: face.displayText + ". Tuxedo companion. Open activities."
    }
    fullRepresentation: ColumnLayout {
        id: popup
        Layout.minimumWidth: Kirigami.Units.gridUnit * 20
        Layout.preferredWidth: Kirigami.Units.gridUnit * 22
        Layout.minimumHeight: Kirigami.Units.gridUnit * 21
        Layout.preferredHeight: Kirigami.Units.gridUnit * 26
        spacing: Kirigami.Units.smallSpacing
        RowLayout {
            Layout.fillWidth: true
            TuxedoCat { Layout.preferredWidth: 76; Layout.preferredHeight: 48; working: !!root.state.working; listening: !!root.state.listening; reducedMotion: !!root.options.reducedMotion; accent: Kirigami.Theme.highlightColor }
            ColumnLayout {
                Layout.fillWidth: true
                Controls.Label { text: "Your companion"; textFormat: Text.PlainText; font.pixelSize: 18; font.weight: Font.DemiBold }
                Controls.Label { text: client.connected ? "On this device · Connected" : "NEXIS service is not running"; textFormat: Text.PlainText; color: root.secondary; font.pixelSize: 11 }
            }
            Controls.ToolButton { text: "×"; Accessible.name: "Close"; onClicked: root.expanded = false }
        }
        Controls.TabBar {
            id: tabs
            Layout.fillWidth: true
            Controls.TabButton { text: "Music" }
            Controls.TabButton { text: "Agents" }
            Controls.TabButton { text: "Activity" }
            Controls.TabButton { text: "System" }
        }
        StackLayout {
            currentIndex: tabs.currentIndex
            Layout.fillWidth: true
            Layout.fillHeight: true
            ColumnLayout {
                spacing: 12
                Controls.ComboBox {
                    id: playerSelector
                    visible: (client.data.players || []).length > 1
                    Layout.fillWidth: true
                    model: client.data.players || []
                    textRole: "identity"
                    valueRole: "service"
                    currentIndex: (client.data.players || []).findIndex(p => p.service === root.track.service)
                    contentItem: Controls.Label { text: playerSelector.displayText; textFormat: Text.PlainText; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; leftPadding: 8; rightPadding: 30 }
                    delegate: Controls.ItemDelegate { required property var modelData; width: playerSelector.width; contentItem: Controls.Label { text: modelData.identity || modelData.service; textFormat: Text.PlainText; elide: Text.ElideRight } }
                    onActivated: client.action("selectPlayer", currentValue)
                    Accessible.name: "Music player"
                }
                Item { Layout.fillHeight: true }
                TuxedoCat { Layout.alignment: Qt.AlignHCenter; Layout.preferredWidth: 140; Layout.preferredHeight: 98; working: !!root.state.working; listening: !!root.state.listening; reducedMotion: !!root.options.reducedMotion; accent: Kirigami.Theme.highlightColor }
                Controls.Label { Layout.fillWidth: true; text: root.track.title || "A little quiet"; textFormat: Text.PlainText; font.pixelSize: 19; font.weight: Font.DemiBold; horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight }
                Controls.Label { Layout.fillWidth: true; text: root.track.artist || (root.track.service ? root.track.identity : "Play music in an MPRIS-compatible app"); textFormat: Text.PlainText; color: root.secondary; horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight }
                Controls.Slider { Layout.fillWidth: true; from: 0; to: 1; value: root.track.duration ? root.track.position / root.track.duration : 0; enabled: !!root.track.canSeek; onMoved: client.action("seek", value); Accessible.name: "Playback position" }
                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    Controls.Button { text: "Previous"; icon.name: "media-skip-backward"; enabled: !!root.track.canPrevious; onClicked: client.action("previous") }
                    Controls.Button { text: root.track.playing ? "Pause" : "Play"; icon.name: root.track.playing ? "media-playback-pause" : "media-playback-start"; enabled: !!root.track.canPlayPause; onClicked: client.action("playPause") }
                    Controls.Button { text: "Next"; icon.name: "media-skip-forward"; enabled: !!root.track.canNext; onClicked: client.action("next") }
                }
                Item { Layout.fillHeight: true }
            }
            ColumnLayout {
                Controls.Label { text: "Your agents"; textFormat: Text.PlainText; font.pixelSize: 16; font.weight: Font.DemiBold }
                Controls.Label { visible: root.sessions.length === 0; Layout.fillWidth: true; text: "Connect Claude Code or the new Qoder app with NEXIS hooks."; textFormat: Text.PlainText; wrapMode: Text.WordWrap; color: root.secondary }
                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: root.sessions
                    clip: true
                    spacing: 8
                    delegate: Controls.ItemDelegate {
                        required property var modelData
                        width: ListView.view.width
                        height: 86
                        contentItem: ColumnLayout {
                            Controls.Label { text: (modelData.provider === "claude_code" ? "Claude Code" : "Qoder") + " · " + modelData.project; textFormat: Text.PlainText; Layout.fillWidth: true; elide: Text.ElideRight }
                            Controls.Label { text: popup.status(modelData.status); textFormat: Text.PlainText; color: root.secondary }
                            Controls.Label { text: modelData.status === "waiting_permission" ? "Respond in the originating app" : "Last observed: " + modelData.lastEvent; textFormat: Text.PlainText; font.pixelSize: 11; color: root.secondary }
                        }
                    }
                    Controls.ScrollBar.vertical: Controls.ScrollBar {}
                }
            }
            ColumnLayout {
                RowLayout {
                    Controls.Label { text: "Recent activity"; textFormat: Text.PlainText; Layout.fillWidth: true }
                    Controls.Button { text: "Clear"; enabled: !!(client.data.history || []).length; onClicked: client.action("clearHistory") }
                }
                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: client.data.history || []
                    clip: true
                    delegate: Controls.ItemDelegate {
                        required property var modelData
                        width: ListView.view.width
                        contentItem: Controls.Label { text: modelData.time + "  " + modelData.project + " · " + modelData.event; textFormat: Text.PlainText; elide: Text.ElideRight }
                    }
                    Controls.ScrollBar.vertical: Controls.ScrollBar {}
                }
            }
            Controls.ScrollView {
                clip: true
                contentWidth: availableWidth
                ColumnLayout {
                    width: parent.width
                    spacing: 12
                    Controls.Label { text: "Network: " + ((client.data.system || {}).network || "Unavailable"); textFormat: Text.PlainText }
                    Controls.Label { text: (client.data.system || {}).batteryAvailable ? "Battery: " + Math.round(client.data.system.battery) + "%" : "No battery"; textFormat: Text.PlainText }
                    Controls.Label { text: "Output volume"; textFormat: Text.PlainText }
                    Controls.Slider { Layout.fillWidth: true; from: 0; to: 1; value: (client.data.system || {}).volume || 0; enabled: !!(client.data.system || {}).volumeAvailable; onMoved: client.action("volume", value); Accessible.name: "System output volume" }
                    Controls.Button { text: (client.data.system || {}).muted ? "Unmute" : "Mute"; enabled: !!(client.data.system || {}).volumeAvailable; onClicked: client.action("mute") }
                    Controls.Switch { text: "Reduced motion"; checked: !!root.options.reducedMotion; onToggled: client.action("reducedMotion", checked) }
                    Repeater {
                        model: client.data.health || []
                        delegate: Controls.Label { required property var modelData; text: modelData.name + ": " + modelData.status; textFormat: Text.PlainText; wrapMode: Text.WordWrap; Layout.fillWidth: true; color: root.secondary }
                    }
                }
            }
        }
        function status(s) {
            const labels = {started: "Session started", running: "Processing", tool_running: "Using a tool", waiting_permission: "Permission needed", turn_complete: "Response complete", session_ended: "Session ended", stale: "Stale · no recent event", unknown: "Unknown"}
            return labels[s] || "Unknown"
        }
    }
}
