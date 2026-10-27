import QtQuick
import QtQuick.Controls as C
import QtQuick.Layouts

Rectangle {
    id: fold
    objectName: "foldSurface"
    property var snapshot: ({})
    property int section: 0
    property bool preferencesAvailable: false
    property bool embeddedNotch: false
    property real maximumHeight: 600
    readonly property bool lightTheme: options.theme === "light" || (options.theme !== "dark" && snapshot.darkTheme === false)
    property color foreground: lightTheme ? "#243248" : "#edf2fb"
    property color muted: lightTheme ? "#5b6c83" : "#9daabe"
    property color accent: "#8bbcff"
    property color surface: "#242d3d"
    readonly property var track: snapshot.media || ({})
    readonly property var state: snapshot.presentation || ({})
    readonly property var sessions: snapshot.sessions || []
    readonly property var notices: snapshot.notifications || []
    readonly property var options: snapshot.options || ({})
    readonly property bool reducedMotion: !!options.reducedMotion
    signal action(string name, var value)
    signal closeRequested()
    implicitWidth: 376
    implicitHeight: Math.min(maximumHeight, layout.implicitHeight + 32)
    radius: 21
    color: embeddedNotch ? "transparent" : lightTheme ? "#f1f4f9" : "#171b23"
    border.color: "#414b5b"
    border.width: embeddedNotch ? 0 : 1
    function time(us) { const s = Math.max(0, Math.floor((us || 0) / 1000000)); return Math.floor(s / 60) + ":" + (s % 60).toString().padStart(2, "0") }
    function provider(s) { return s.provider === "claude_code" ? "Claude Code" : "Qoder" }
    function status(s) { return ({running:"Processing",tool_running:"Using a tool",waiting_permission:"Permission needed",turn_complete:"Response complete",session_ended:"Session ended",stale:"No recent event",started:"Session started"})[s] || "Unknown" }
    component Caption: C.Label { textFormat: Text.PlainText; color: fold.muted; font.pixelSize: 11; elide: Text.ElideRight }
    component SmallButton: C.Button {
        id: small
        property bool selected: false
        property bool round: false
        implicitHeight: round ? 38 : 28
        implicitWidth: round ? 38 : Math.max(40, implicitContentWidth + 18)
        padding: 7
        hoverEnabled: true
        contentItem: C.Label { text: small.text; textFormat: Text.PlainText; color: small.enabled ? small.selected && small.round ? "#1e2c42" : small.selected || small.hovered ? "#d3e4ff" : fold.foreground : "#637086"; font.pixelSize: small.round ? 17 : 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { radius: small.round ? width / 2 : 6; color: small.selected && small.round ? "#e3ecf9" : small.selected || small.hovered ? "#303f55" : "transparent"; border.width: small.activeFocus ? 2 : 0; border.color: fold.accent }
        Accessible.name: text
    }
    C.ScrollView {
        id: viewport
        anchors.fill: parent
        anchors.leftMargin: 18
        anchors.rightMargin: 18
        anchors.topMargin: 16
        anchors.bottomMargin: 16
        contentWidth: availableWidth
        clip: true
        ColumnLayout {
        id: layout
        width: viewport.availableWidth
        spacing: 12
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            TrackArtwork { visible: !!fold.track.service; Layout.preferredWidth: 48; Layout.preferredHeight: 48; source: fold.track.art || ""; fallbackColor: fold.accent }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 4
                C.Label { Layout.fillWidth: true; text: fold.track.title || fold.state.title || "All quiet"; textFormat: Text.PlainText; color: fold.foreground; font.pixelSize: 17; font.weight: Font.DemiBold; elide: Text.ElideRight }
                Caption { Layout.fillWidth: true; text: fold.track.artist || (fold.snapshot.demo ? "Preview data" : fold.track.identity || "Your desktop, together") }
            }
            TuxedoCat { Layout.preferredWidth: 42; Layout.preferredHeight: 34; working: !!fold.state.working; listening: !!fold.state.listening; reducedMotion: fold.reducedMotion; accent: fold.accent }
            PlaybackPulse { visible: !!fold.track.service; playing: !!fold.track.playing; reducedMotion: fold.reducedMotion; color: fold.accent }
            SmallButton { text: "×"; implicitWidth: 24; Accessible.name: "Close island"; onClicked: fold.closeRequested() }
        }
        ColumnLayout {
            visible: fold.section === 0
            Layout.fillWidth: true
            spacing: 4
            C.ComboBox {
                id: players
                visible: (fold.snapshot.players || []).length > 1
                Layout.fillWidth: true
                model: fold.snapshot.players || []
                textRole: "identity"
                valueRole: "service"
                currentIndex: (fold.snapshot.players || []).findIndex(p => p.service === fold.track.service)
                onActivated: fold.action("selectPlayer", currentValue)
                Accessible.name: "Music player"
                palette.text: fold.foreground
                palette.buttonText: fold.foreground
                delegate: C.ItemDelegate { required property var modelData; width: players.width; contentItem: Caption { text: modelData.identity || modelData.service } }
            }
            C.Slider {
                id: seek
                visible: !!fold.track.service
                Layout.fillWidth: true
                implicitHeight: 16
                padding: 0
                from: 0; to: 1
                value: fold.track.duration ? fold.track.position / fold.track.duration : 0
                enabled: !!fold.track.canSeek
                onMoved: fold.action("seek", value)
                Accessible.name: "Playback position"
                background: Rectangle { x: seek.leftPadding; y: seek.topPadding + seek.availableHeight / 2 - height / 2; width: seek.availableWidth; height: 3; radius: 2; color: "#424d60"; Rectangle { width: seek.visualPosition * parent.width; height: parent.height; radius: 2; color: fold.accent } }
                handle: Rectangle { x: seek.leftPadding + seek.visualPosition * (seek.availableWidth - width); y: seek.topPadding + seek.availableHeight / 2 - height / 2; width: seek.hovered || seek.pressed || seek.activeFocus ? 9 : 0; height: width; radius: width / 2; color: fold.accent }
            }
            RowLayout {
                visible: !!fold.track.service
                Layout.fillWidth: true
                Caption { text: fold.time(fold.track.position); font.pixelSize: 10 }
                Item { Layout.fillWidth: true }
                Caption { text: fold.time(fold.track.duration); font.pixelSize: 10 }
            }
            RowLayout {
                visible: !!fold.track.service
                Layout.alignment: Qt.AlignHCenter
                spacing: 15
                SmallButton { round: true; text: "⏮"; enabled: !!fold.track.canPrevious; Accessible.name: "Previous track"; onClicked: fold.action("previous", null) }
                SmallButton { objectName: "playPauseButton"; round: true; text: fold.track.playing ? "Ⅱ" : "▶"; selected: true; enabled: !!fold.track.canPlayPause; Accessible.name: fold.track.playing ? "Pause music" : "Play music"; onClicked: fold.action("playPause", null) }
                SmallButton { round: true; text: "⏭"; enabled: !!fold.track.canNext; Accessible.name: "Next track"; onClicked: fold.action("next", null) }
            }
            Caption { visible: !fold.track.service; Layout.fillWidth: true; text: "Play music in your usual player."; wrapMode: Text.WordWrap; elide: Text.ElideNone }
        }
        ColumnLayout {
            visible: fold.notices.length > 0 && fold.section === 0
            Layout.fillWidth: true
            spacing: 8
            Repeater {
                model: fold.notices.slice(0, 3)
                delegate: ColumnLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 5
                    Rectangle { Layout.fillWidth: true; height: 1; color: "#343d4e" }
                    Caption { Layout.fillWidth: true; text: modelData.appName || "Notification"; Layout.topMargin: 4 }
                    C.Label { Layout.fillWidth: true; text: modelData.title || "Notification"; textFormat: Text.PlainText; color: fold.foreground; font.pixelSize: 12; font.weight: Font.DemiBold; elide: Text.ElideRight }
                    Caption { Layout.fillWidth: true; text: modelData.body || ""; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight }
                    RowLayout {
                        SmallButton { visible: !!modelData.hasDefaultAction; text: "Open"; selected: true; onClicked: fold.action("openNotification", modelData.id) }
                        SmallButton { text: "Dismiss"; onClicked: fold.action("dismissNotification", modelData.id) }
                    }
                }
            }
            Caption { visible: fold.notices.length > 3; text: "+" + (fold.notices.length - 3) + " more in Plasma notifications" }
        }
        ColumnLayout {
            visible: fold.section === 1
            Layout.fillWidth: true
            spacing: 8
            Caption { visible: fold.sessions.length === 0; Layout.fillWidth: true; wrapMode: Text.WordWrap; text: "Connect Claude Code or Qoder with NEXIS hooks." }
            ListView {
                visible: fold.sessions.length > 0
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(contentHeight, 260)
                clip: true
                spacing: 8
                model: fold.sessions
                delegate: ColumnLayout {
                    required property var modelData
                    width: ListView.view.width
                    height: 74
                    spacing: 3
                    C.Label { Layout.fillWidth: true; text: fold.provider(modelData) + " · " + modelData.project; textFormat: Text.PlainText; font.pixelSize: 12; color: fold.foreground; elide: Text.ElideRight }
                    Caption { Layout.fillWidth: true; text: fold.status(modelData.status); color: modelData.status === "waiting_permission" ? "#f5c16c" : fold.muted }
                    Caption { Layout.fillWidth: true; text: modelData.status === "waiting_permission" ? "Respond in the originating app" : "Last observed: " + modelData.lastEvent }
                }
                C.ScrollBar.vertical: C.ScrollBar {}
            }
            ListView {
                objectName: "jobsList"
                visible: count > 0
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(contentHeight, 140)
                clip: true
                model: fold.snapshot.jobs || []
                delegate: ColumnLayout {
                    required property var modelData
                    width: ListView.view.width
                    height: 54
                    spacing: 4
                    Caption { Layout.fillWidth: true; text: modelData.title + " · " + modelData.status }
                    C.ProgressBar { Layout.fillWidth: true; from: 0; to: 100; value: modelData.progress || 0; Accessible.name: modelData.title + " progress" }
                }
                C.ScrollBar.vertical: C.ScrollBar {}
            }
        }
        C.ScrollView {
            visible: fold.section === 2
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(systemLayout.implicitHeight, 300)
            contentWidth: availableWidth
            clip: true
            ColumnLayout {
                id: systemLayout
                width: parent.width
                spacing: 10
                Caption { text: "Network: " + ((fold.snapshot.system || {}).network || "Unavailable") }
                Caption { text: (fold.snapshot.system || {}).batteryAvailable ? "Battery: " + Math.round(fold.snapshot.system.battery) + "%" : "No battery" }
                RowLayout {
                    Layout.fillWidth: true
                    Caption { text: "Output volume"; Layout.fillWidth: true }
                    SmallButton { text: (fold.snapshot.system || {}).muted ? "Unmute" : "Mute"; enabled: !!(fold.snapshot.system || {}).volumeAvailable; onClicked: fold.action("mute", null) }
                }
                C.Slider { Layout.fillWidth: true; from: 0; to: 1; value: (fold.snapshot.system || {}).volume || 0; enabled: !!(fold.snapshot.system || {}).volumeAvailable; onMoved: fold.action("volume", value); Accessible.name: "System output volume" }
                C.Switch { text: "Reduced motion"; checked: fold.reducedMotion; onToggled: fold.action("reducedMotion", checked); contentItem: Caption { text: parent.text; leftPadding: 52; verticalAlignment: Text.AlignVCenter } }
                Repeater {
                    model: fold.snapshot.health || []
                    delegate: Caption { required property var modelData; Layout.fillWidth: true; text: modelData.name + ": " + modelData.status; wrapMode: Text.WordWrap; elide: Text.ElideNone }
                }
                SmallButton { visible: fold.preferencesAvailable; text: "Preferences"; onClicked: fold.action("preferences", null) }
            }
        }
        RowLayout {
            visible: fold.section === 0 && fold.sessions.length > 0
            Layout.fillWidth: true
            Caption { Layout.fillWidth: true; text: fold.state.workingCount ? "●  " + fold.state.workingCount + " agent" + (fold.state.workingCount === 1 ? " working" : "s working") : fold.sessions.length + " agent sessions" }
            SmallButton { text: "View agents"; onClicked: fold.section = 1 }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: "#343d4e" }
        RowLayout {
            Layout.fillWidth: true
            spacing: 2
            Repeater {
                model: ["Music", "Agents", "System"]
                delegate: SmallButton { required property string modelData; required property int index; text: modelData; selected: fold.section === index; onClicked: fold.section = index }
            }
            Item { Layout.fillWidth: true }
            Caption { visible: !!fold.snapshot.demo; text: "Demo"; font.pixelSize: 9 }
        }
    }
    }
}
