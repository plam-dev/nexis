import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id: island
    objectName: "island"
    property bool ready: false
    property bool expanded: false
    property int tab: 0
    property bool showSettings: false
    readonly property var colors: settings.colors
    readonly property var opts: settings.options
    readonly property var activity: backend.primary
    readonly property var compactState: backend.presentation
    readonly property var track: media.current
    readonly property bool motion: !opts.reducedMotion
    readonly property color attention: colors.dark ? "#f5c16c" : "#855700"
    readonly property color claude: colors.dark ? "#e7a580" : "#955431"
    readonly property color qoder: colors.dark ? "#8bbcff" : "#275db2"
    readonly property int liveCount: backend.sessions.filter(s => ["started", "running", "tool_running", "waiting_permission"].includes(s.status)).length
    property string clock: Qt.formatTime(new Date(), "HH:mm")
    title: "NEXIS"
    width: expanded ? 400 : compactFace.implicitWidth
    height: expanded ? Math.min(570, Screen.height * 0.65) : 44
    color: "transparent"
    visible: ready && (!compositor.fullscreen || opts.fullscreenOverlay)
    flags: Qt.FramelessWindowHint | Qt.Tool | Qt.WindowStaysOnTopHint
    palette.window: colors.background
    palette.base: colors.surface
    palette.button: colors.surface
    palette.text: colors.text
    palette.windowText: colors.text
    palette.buttonText: colors.text
    palette.highlight: colors.accent
    palette.highlightedText: "white"
    font.family: Qt.application.font.family
    font.pixelSize: 13
    onExpandedChanged: {
        backend.expanded(expanded)
        if (expanded) {
            if (activity.kind === "media") tab = 2
            else if (activity.kind === "agents") tab = 1
            else if (activity.kind === "system") tab = 3
        } else showSettings = false
    }
    onActiveChanged: if (!active && expanded) expanded = false
    Behavior on width { NumberAnimation { duration: island.motion ? 330 : 0; easing.type: Easing.OutCubic } }
    Behavior on height { NumberAnimation { duration: island.motion ? 380 : 0; easing.type: Easing.OutCubic } }
    Shortcut { sequence: "Escape"; enabled: island.expanded; onActivated: island.expanded = false }
    Timer { interval: 30000; running: opts.idleClock; repeat: true; onTriggered: island.clock = Qt.formatTime(new Date(), "HH:mm") }
    Timer { id: dwell; interval: 350; onTriggered: if (hover.hovered && opts.hoverExpand) island.expanded = true }
    HoverHandler { id: hover; onHoveredChanged: { if (hovered && opts.hoverExpand && !island.expanded) dwell.start(); else dwell.stop(); if (!hovered && opts.hoverExpand && !island.active) island.expanded = false } }
    TapHandler { acceptedButtons: Qt.MiddleButton; onTapped: if (opts.middleClick === "media" && track.service) media.control("PlayPause"); else if (opts.middleClick === "toggle") island.expanded = !island.expanded }
    background: Rectangle {
        color: island.colors.background
        radius: island.expanded ? 24 : height / 2
        border.width: 1
        border.color: hover.hovered ? Qt.lighter(island.colors.border, 1.25) : island.colors.border
        Behavior on radius { NumberAnimation { duration: island.motion ? 280 : 0 } }
    }
    function statusLabel(s) {
        const labels = {started: "Session started", running: "Processing", tool_running: "Using a tool", waiting_permission: "Permission needed", turn_complete: "Turn complete", session_ended: "Session ended", stale: "Stale · no recent event", unknown: "Unknown"}
        return labels[s] || s || "Unknown"
    }
    function sourceName(s) { return s === "claude_code" ? "Claude Code" : s === "local_job" ? "Local job" : "Qoder" }
    function accentFor(s) { return s.status === "waiting_permission" ? attention : s.provider === "claude_code" ? claude : qoder }
    function duration(us) { const s = Math.max(0, Math.floor((us || 0) / 1000000)); return Math.floor(s / 60) + ":" + (s % 60).toString().padStart(2, "0") }

    component QuietButton: Button {
        id: button
        property bool emphasis: false
        font.pixelSize: 12
        hoverEnabled: true
        implicitHeight: 32
        padding: 9
        palette.buttonText: emphasis ? "white" : island.colors.text
        background: Rectangle {
            radius: 12
            color: button.emphasis ? island.colors.accent : button.down ? Qt.lighter(island.colors.surface, 1.35) : button.hovered ? Qt.lighter(island.colors.surface, 1.2) : island.colors.surface
            border.width: button.activeFocus ? 2 : 0
            border.color: island.colors.accent
        }
        Accessible.name: text
        ToolTip.visible: hovered && text.length === 0
        ToolTip.text: Accessible.name
    }
    component EmptyState: ColumnLayout {
        property string icon: "dialog-information"
        property string heading: ""
        property string detail: ""
        spacing: 12
        Item { Layout.fillHeight: true; Layout.minimumHeight: 18 }
        Label { textFormat: Text.PlainText; text: heading; font.pixelSize: 17; font.weight: Font.DemiBold; color: island.colors.text; Layout.alignment: Qt.AlignHCenter }
        Label { textFormat: Text.PlainText; text: detail; color: island.colors.muted; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter; Layout.fillWidth: true; Layout.leftMargin: 14; Layout.rightMargin: 14 }
        Item { Layout.fillHeight: true; Layout.minimumHeight: 18 }
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        Button {
            id: capsule
            objectName: "capsuleButton"
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            hoverEnabled: true
            padding: 0
            background: Item {}
            Accessible.name: island.expanded ? "Collapse NEXIS" : "Expand: " + compactFace.displayText
            DragHandler { target: null; onActiveChanged: backend.drag(active); onActiveTranslationChanged: backend.moveDrag() }
            onClicked: { island.expanded = !island.expanded; if (island.expanded) island.requestActivate() }
            contentItem: CompactActivity {
                id: compactFace
                activity: island.compactState
                foreground: island.colors.text
                muted: island.colors.muted
                accent: island.qoder
                attention: island.attention
                reducedMotion: !island.motion
            }
        }

        Rectangle { visible: island.expanded; Layout.fillWidth: true; Layout.leftMargin: 16; Layout.rightMargin: 16; height: 1; color: island.colors.border }
        ColumnLayout {
            visible: island.expanded
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 16
            spacing: 14
            RowLayout {
                Layout.fillWidth: true
                Label { textFormat: Text.PlainText; text: showSettings ? "Preferences" : "Your activity"; font.pixelSize: 20; font.weight: Font.DemiBold; color: island.colors.text; Layout.fillWidth: true }
                Label { textFormat: Text.PlainText; visible: backend.demo; text: "Demo"; color: island.attention; font.pixelSize: 11; Accessible.name: "Demo data, not live sessions" }
                QuietButton { text: showSettings ? "Back" : "Settings"; onClicked: showSettings = !showSettings }
            }
            RowLayout {
                visible: !showSettings
                spacing: 4
                Repeater {
                    model: ["Activities", "Agents", "Media", "System"]
                    delegate: QuietButton {
                        required property string modelData
                        required property int index
                        text: modelData
                        Layout.fillWidth: true
                        emphasis: island.tab === index
                        onClicked: island.tab = index
                    }
                }
            }
            StackLayout {
                visible: !showSettings
                currentIndex: island.tab
                Layout.fillWidth: true
                Layout.fillHeight: true
                // Activities
                ColumnLayout {
                    spacing: 10
                    RowLayout {
                        Label { textFormat: Text.PlainText; text: opts.history ? "Recent events" : "History is off"; color: island.colors.muted; Layout.fillWidth: true }
                        QuietButton { text: "Clear"; enabled: backend.history.length > 0; onClicked: backend.clearHistory() }
                    }
                    Repeater {
                        model: backend.jobs.filter(j => j.status === "started" || j.status === "progress" || j.status === "failed")
                        delegate: ColumnLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            Label { textFormat: Text.PlainText; text: modelData.title + " · " + modelData.status; color: island.colors.text; Layout.fillWidth: true; elide: Text.ElideRight }
                            ProgressBar { Layout.fillWidth: true; from: 0; to: 100; value: modelData.progress }
                        }
                    }
                    EmptyState { visible: backend.history.length === 0 && backend.jobs.length === 0; Layout.fillWidth: true; Layout.fillHeight: true; heading: "A quiet moment"; detail: opts.history ? "Music and agent activity will appear here. Connect your apps from System." : "Enable local history in Settings to keep recent activity." }
                    ListView {
                        visible: backend.history.length > 0
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        clip: true
                        spacing: 2
                        model: backend.history
                        delegate: ItemDelegate {
                            required property var modelData
                            width: ListView.view.width
                            height: 64
                            background: Rectangle { radius: 12; color: parent.hovered ? island.colors.surface : "transparent" }
                            contentItem: ColumnLayout {
                                RowLayout {
                                    Label { textFormat: Text.PlainText; text: modelData.project; color: island.colors.text; elide: Text.ElideRight; Layout.fillWidth: true }
                                    Label { textFormat: Text.PlainText; text: modelData.time; color: island.colors.muted; font.pixelSize: 11 }
                                }
                                Label { textFormat: Text.PlainText; text: sourceName(modelData.provider) + " · " + modelData.event; color: island.colors.muted; font.pixelSize: 11; elide: Text.ElideRight; Layout.fillWidth: true }
                            }
                            onClicked: island.tab = 1
                        }
                        ScrollBar.vertical: ScrollBar {}
                    }
                }
                // Agents
                ColumnLayout {
                    Label { textFormat: Text.PlainText; text: liveCount + (liveCount === 1 ? " active session" : " active sessions"); color: island.colors.muted }
                    EmptyState { visible: backend.sessions.length === 0; Layout.fillWidth: true; Layout.fillHeight: true; heading: "Space for your agents"; detail: "Connect Claude Code or Qoder hooks. Only observed activity appears here." }
                    ListView {
                        visible: backend.sessions.length > 0
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: backend.sessions
                        clip: true
                        spacing: 10
                        delegate: Rectangle {
                            required property var modelData
                            property bool details: false
                            width: ListView.view.width
                            height: details ? 160 : 112
                            radius: 16
                            color: island.colors.surface
                            border.width: modelData.status === "waiting_permission" ? 1 : 0
                            border.color: island.attention
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 6
                                RowLayout {
                                    Label { textFormat: Text.PlainText; text: sourceName(modelData.provider); color: accentFor(modelData); font.pixelSize: 11; Layout.fillWidth: true }
                                    Label { textFormat: Text.PlainText; text: modelData.time; color: island.colors.muted; font.pixelSize: 11 }
                                }
                                Label { textFormat: Text.PlainText; text: modelData.project; font.pixelSize: 16; font.weight: Font.Medium; color: island.colors.text; elide: Text.ElideRight; Layout.fillWidth: true }
                                RowLayout {
                                    Label { textFormat: Text.PlainText; text: statusLabel(modelData.status); color: modelData.status === "waiting_permission" ? island.attention : island.colors.text; font.pixelSize: 12; Layout.fillWidth: true }
                                    QuietButton { text: details ? "Less" : "Details"; implicitHeight: 26; onClicked: details = !details }
                                }
                                Label { textFormat: Text.PlainText; visible: details; text: modelData.status === "waiting_permission" ? "Respond in " + sourceName(modelData.provider) + "." : "Last observed: " + modelData.lastEvent; color: island.colors.muted; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                                Label { textFormat: Text.PlainText; visible: details; text: modelData.subagents + " subagents" + (modelData.unknownSubagent ? " + unidentified" : "") + (modelData.tool ? " · " + modelData.tool : "") + (modelData.confidence === "limited" ? " · Limited confidence" : ""); color: island.colors.muted; font.pixelSize: 11 }
                            }
                            Accessible.name: sourceName(modelData.provider) + ", " + modelData.project + ", " + statusLabel(modelData.status)
                        }
                        ScrollBar.vertical: ScrollBar {}
                    }
                }
                // Media
                ColumnLayout {
                    spacing: 12
                    EmptyState { visible: !track.service; Layout.fillWidth: true; Layout.fillHeight: true; heading: "Make room for music"; detail: "Play something in an MPRIS-compatible app. NEXIS will pick it up automatically." }
                    ColumnLayout {
                        visible: !!track.service
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 12
                        ComboBox {
                            id: playerSelector
                            Layout.fillWidth: true
                            contentItem: Label { textFormat: Text.PlainText; text: playerSelector.displayText; color: island.colors.text; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight; leftPadding: 10; rightPadding: 30 }
                            delegate: ItemDelegate {
                                required property var modelData
                                width: playerSelector.width
                                contentItem: Label { textFormat: Text.PlainText; text: modelData.identity || modelData.service; color: island.colors.text; elide: Text.ElideRight }
                            }
                            model: media.players
                            textRole: "identity"
                            valueRole: "service"
                            currentIndex: media.players.findIndex(p => p.service === track.service)
                            onActivated: media.select(currentValue)
                            Accessible.name: "Media player"
                        }
                        Rectangle {
                            Layout.alignment: Qt.AlignHCenter
                            Layout.preferredHeight: 120
                            Layout.preferredWidth: 120
                            color: island.colors.surface
                            radius: 18
                            Image { anchors.fill: parent; anchors.margins: 1; source: track.art || ""; asynchronous: true; sourceSize: Qt.size(240, 240); fillMode: Image.PreserveAspectFit }
                            Label { textFormat: Text.PlainText; anchors.centerIn: parent; visible: !track.art; text: "♪"; font.pixelSize: 50; color: island.colors.muted }
                        }
                        Label { textFormat: Text.PlainText; Layout.fillWidth: true; text: track.title || "Unknown track"; font.pixelSize: 18; font.weight: Font.DemiBold; color: island.colors.text; horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight }
                        Label { textFormat: Text.PlainText; Layout.fillWidth: true; text: track.artist || track.identity || ""; font.pixelSize: 12; color: island.colors.muted; horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight }
                        Slider { Layout.fillWidth: true; from: 0; to: 1; value: track.duration ? track.position / track.duration : 0; enabled: !!track.canSeek; onMoved: media.seek(value); Accessible.name: "Playback position" }
                        RowLayout {
                            Label { textFormat: Text.PlainText; text: duration(track.position); color: island.colors.muted; font.pixelSize: 11 }
                            Item { Layout.fillWidth: true }
                            Label { textFormat: Text.PlainText; text: duration(track.duration); color: island.colors.muted; font.pixelSize: 11 }
                        }
                        RowLayout {
                            Layout.alignment: Qt.AlignHCenter
                            spacing: 14
                            QuietButton { text: "Previous"; icon.name: "media-skip-backward"; enabled: !!track.canPrevious; onClicked: media.control("Previous") }
                            QuietButton { text: track.playing ? "Pause" : "Play"; emphasis: true; enabled: !!track.canPlayPause; onClicked: media.control("PlayPause") }
                            QuietButton { text: "Next"; icon.name: "media-skip-forward"; enabled: !!track.canNext; onClicked: media.control("Next") }
                        }
                        Label { textFormat: Text.PlainText; visible: media.error.length > 0; text: media.error; color: island.attention; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                        Item { Layout.fillHeight: true }
                    }
                }
                // System
                ScrollView {
                    clip: true
                    contentWidth: availableWidth
                    ColumnLayout {
                        width: parent.width
                        spacing: 12
                        Label { textFormat: Text.PlainText; text: "Desktop"; font.pixelSize: 15; font.weight: Font.DemiBold; color: island.colors.text }
                        RowLayout {
                            Layout.fillWidth: true
                            Label { textFormat: Text.PlainText; text: "Network"; color: island.colors.muted; Layout.fillWidth: true }
                            Label { textFormat: Text.PlainText; text: systemInfo.state.network; color: island.colors.text }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Label { textFormat: Text.PlainText; text: "Battery"; color: island.colors.muted; Layout.fillWidth: true }
                            Label { textFormat: Text.PlainText; text: systemInfo.state.batteryAvailable ? Math.round(systemInfo.state.battery) + "%" + (systemInfo.state.charging ? " · Charging" : "") : "No battery"; color: island.colors.text }
                        }
                        RowLayout {
                            Label { textFormat: Text.PlainText; text: "Output volume"; color: island.colors.muted; Layout.fillWidth: true }
                            QuietButton { text: systemInfo.state.muted ? "Unmute" : "Mute"; enabled: !!systemInfo.state.volumeAvailable; onClicked: systemInfo.toggleMute() }
                        }
                        Slider { Layout.fillWidth: true; from: 0; to: 1; value: systemInfo.state.volume || 0; enabled: !!systemInfo.state.volumeAvailable; onMoved: systemInfo.setVolume(value); Accessible.name: "System output volume" }
                        Label { textFormat: Text.PlainText; visible: !systemInfo.state.volumeAvailable; text: "Output volume unavailable"; color: island.colors.muted; font.pixelSize: 11 }
                        Label { textFormat: Text.PlainText; text: "Fullscreen detection: " + compositor.health; color: island.colors.muted; font.pixelSize: 11 }
                        Label { textFormat: Text.PlainText; text: "Connections"; font.pixelSize: 15; font.weight: Font.DemiBold; color: island.colors.text; Layout.topMargin: 4 }
                        Repeater {
                            model: backend.health
                            delegate: ColumnLayout {
                                required property var modelData
                                Layout.fillWidth: true
                                spacing: 4
                                RowLayout {
                                    Layout.fillWidth: true
                                    Label { textFormat: Text.PlainText; text: modelData.name; color: island.colors.text; Layout.fillWidth: true }
                                    Label { textFormat: Text.PlainText; text: modelData.status; color: island.colors.muted; font.pixelSize: 11 }
                                }
                                Label { textFormat: Text.PlainText; text: modelData.detail; font.pixelSize: 11; color: island.colors.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                                Rectangle { Layout.fillWidth: true; height: 1; color: island.colors.border; Layout.topMargin: 6 }
                            }
                        }
                        Label { textFormat: Text.PlainText; text: "Connect hooks with nexis hooks install.\nNEXIS never responds to permission requests."; color: island.colors.muted; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    }
                }
            }
            ScrollView {
                visible: showSettings
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                clip: true
                ColumnLayout {
                    width: parent.width
                    spacing: 10
                    Label { textFormat: Text.PlainText; text: "Appearance"; font.pixelSize: 15; color: island.colors.text }
                    ComboBox {
                        Layout.fillWidth: true
                        model: ["Desktop theme", "Dark", "Light"]
                        currentIndex: ["auto", "dark", "light"].indexOf(opts.theme)
                        onActivated: settings.set("theme", ["auto", "dark", "light"][currentIndex])
                        Accessible.name: "Theme"
                    }
                    Label { textFormat: Text.PlainText; text: "Following " + colors.name.split(".").pop(); color: island.colors.muted; font.pixelSize: 11; Layout.fillWidth: true; elide: Text.ElideRight }
                    Switch { text: "Reduced motion"; checked: opts.reducedMotion; onToggled: settings.set("reducedMotion", checked) }
                    Switch { text: "Expand after hovering for 350 ms"; checked: opts.hoverExpand; onToggled: settings.set("hoverExpand", checked) }
                    Switch { text: "Show over fullscreen apps"; checked: opts.fullscreenOverlay; onToggled: settings.set("fullscreenOverlay", checked) }
                    ComboBox {
                        Layout.fillWidth: true
                        model: ["Middle-click: play / pause", "Middle-click: expand / collapse", "Middle-click: do nothing"]
                        currentIndex: ["media", "toggle", "none"].indexOf(opts.middleClick)
                        onActivated: settings.set("middleClick", ["media", "toggle", "none"][currentIndex])
                        Accessible.name: "Middle-click action"
                    }
                    Switch { text: "Show clock when idle"; checked: opts.idleClock; onToggled: settings.set("idleClock", checked) }
                    ComboBox {
                        Layout.fillWidth: true
                        model: ["Show highest priority", "Pin music", "Pin agents"]
                        currentIndex: ["", "media", "agents"].indexOf(opts.pinned)
                        onActivated: settings.set("pinned", ["", "media", "agents"][currentIndex])
                        Accessible.name: "Pinned activity"
                    }
                    Switch { text: "Permission alerts can override pinned activity"; checked: opts.attentionOverride; onToggled: settings.set("attentionOverride", checked) }
                    Label { textFormat: Text.PlainText; text: "Position"; font.pixelSize: 15; color: island.colors.text; Layout.topMargin: 8 }
                    ComboBox {
                        Layout.fillWidth: true
                        model: ["Active display"].concat(backend.screens)
                        currentIndex: opts.screen ? backend.screens.indexOf(opts.screen) + 1 : 0
                        onActivated: settings.set("screen", currentIndex ? backend.screens[currentIndex - 1] : "")
                        Accessible.name: "Display"
                    }
                    Label { textFormat: Text.PlainText; text: "Horizontal offset"; color: island.colors.muted }
                    Slider { Layout.fillWidth: true; from: -600; to: 600; value: opts.offsetX; stepSize: 1; onMoved: settings.set("offsetX", value); Accessible.name: "Horizontal offset" }
                    Label { textFormat: Text.PlainText; text: "Distance from top"; color: island.colors.muted }
                    Slider { Layout.fillWidth: true; from: 0; to: 300; value: opts.offsetY; stepSize: 1; onMoved: settings.set("offsetY", value); Accessible.name: "Distance from top" }
                    QuietButton { text: "Reset position"; onClicked: settings.resetPosition() }
                    Switch { text: "Keep recent events in memory"; checked: opts.history; onToggled: settings.set("history", checked) }
                    Label { textFormat: Text.PlainText; text: "History clears on restart. No prompts, commands, or tool output are stored."; color: island.colors.muted; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    QuietButton { text: "Quit NEXIS"; onClicked: backend.quit() }
                }
            }
            Label { textFormat: Text.PlainText;
                text: backend.demo ? "Preview data · isolated from your live sessions" : "On this device · " + (liveCount ? liveCount + " sessions" : "All quiet")
                color: island.colors.muted
                font.pixelSize: 10
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
