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
    onTabChanged: if (fold && fold.section !== tab) fold.section = tab
    property bool showSettings: false
    readonly property var colors: settings.colors
    readonly property var opts: settings.options
    readonly property var activity: backend.primary
    readonly property var compactState: backend.presentation
    readonly property var track: media.current
    readonly property bool motion: !opts.reducedMotion
    readonly property var snapshot: {
        const state = backend.presentation
        const data = JSON.parse(backend.Snapshot())
        data.presentation = state
        data.media = track
        return data
    }
    title: "NEXIS"
    readonly property real maximumNotchWidth: Math.floor(Math.max(0, Math.min(Screen.width - 32, Screen.width * 0.6)))
    width: Math.min(maximumNotchWidth, expanded ? Math.max(560, compactFace.implicitWidth + 48) : compactFace.implicitWidth + 48)
    height: expanded ? showSettings ? Math.min(530, Screen.height * 0.75) : fold.implicitHeight + 8 : opts.notchHeight
    color: "transparent"
    visible: ready && (!compositor.fullscreen || opts.fullscreenOverlay)
    flags: Qt.FramelessWindowHint | Qt.Tool | Qt.WindowStaysOnTopHint
    palette.window: colors.dark ? "#000000" : "#f1f4f9"
    palette.base: colors.dark ? "#242d3d" : "#e2e9f2"
    palette.button: colors.dark ? "#242d3d" : "#e2e9f2"
    palette.text: colors.dark ? "#edf2fb" : "#243248"
    palette.windowText: colors.dark ? "#edf2fb" : "#243248"
    palette.buttonText: colors.dark ? "#edf2fb" : "#243248"
    palette.highlight: "#8bbcff"
    font.family: Qt.application.font.family
    font.pixelSize: 13
    onExpandedChanged: {
        backend.expanded(expanded)
        if (expanded) tab = 0
        else showSettings = false
    }
    onActiveChanged: if (!active && expanded) expanded = false
    Behavior on width { NumberAnimation { duration: island.motion ? 280 : 0; easing.type: Easing.OutCubic } }
    Behavior on height { NumberAnimation { duration: island.motion ? 300 : 0; easing.type: Easing.OutCubic } }
    Shortcut { sequence: "Escape"; enabled: island.expanded; onActivated: island.expanded = false }
    Timer { id: dwell; interval: 350; onTriggered: if (hover.hovered && opts.hoverExpand) island.expanded = true }
    // Moving the pointer away from an open island folds it back into the notch after a short grace
    // period; returning, an open settings page or a held mouse button (slider drag) cancels it.
    Timer {
        id: shrink
        interval: 700
        onTriggered: {
            if (!island.expanded || hover.hovered || island.showSettings || !opts.autoShrink) return
            if (press.active) restart(); else island.expanded = false
        }
    }
    PointHandler { id: press; acceptedButtons: Qt.AllButtons; grabPermissions: PointerHandler.ApprovesTakeOverByAnything }
    HoverHandler { id: hover; onHoveredChanged: { if (hovered && opts.hoverExpand && !island.expanded) dwell.start(); else dwell.stop(); if (hovered) shrink.stop(); else if (island.expanded && opts.autoShrink) shrink.start(); if (!hovered && opts.hoverExpand && !island.active) island.expanded = false } }
    TapHandler { acceptedButtons: Qt.MiddleButton; onTapped: if (opts.middleClick === "media" && track.service) media.control("PlayPause"); else if (opts.middleClick === "toggle") island.expanded = !island.expanded }
    background: NotchBackground { color: island.palette.window }
    Button {
        id: capsule
        objectName: "capsuleButton"
        anchors.fill: parent
        anchors.leftMargin: 24
        anchors.rightMargin: 24
        anchors.topMargin: 6
        anchors.bottomMargin: 8
        visible: !island.expanded
        padding: 0
        background: Item {}
        Accessible.name: "Expand: " + (compactFace.displayText || "Tuxedo cat")
        onClicked: { island.expanded = true; island.requestActivate() }
        contentItem: CompactActivity { id: compactFace; maximumWidth: Math.max(0, island.maximumNotchWidth - 48); activity: island.compactState; track: island.track; showIdleClock: !!island.opts.idleClock; petKind: island.opts.pet || "tuxedo"; reducedMotion: !island.motion; foreground: island.palette.text; muted: island.colors.dark ? "#9daabe" : "#5b6c83"; accent: "#8bbcff" }
    }
    FoldSurface {
        id: fold
        anchors.fill: parent
        anchors.leftMargin: 24
        anchors.rightMargin: 24
        anchors.topMargin: 8
        visible: island.expanded && !island.showSettings
        snapshot: island.snapshot
        section: island.tab
        preferencesAvailable: true
        embeddedNotch: true
        maximumHeight: Math.max(180, Screen.height * 0.75)
        onSectionChanged: island.tab = section
        onCloseRequested: island.expanded = false
        onAction: (name, value) => { if (name === "preferences") island.showSettings = true; else backend.action(name, value) }
    }
    component QuietButton: Button {
        implicitHeight: 32
        font.pixelSize: 12
        palette.buttonText: "#edf2fb"
        background: Rectangle { radius: 7; color: parent.hovered ? "#35445c" : "#242d3d" }
    }
    ColumnLayout {
        visible: island.expanded && island.showSettings
        anchors.fill: parent
        anchors.leftMargin: 42
        anchors.rightMargin: 42
        anchors.topMargin: 24
        anchors.bottomMargin: 24
        RowLayout {
            Layout.fillWidth: true
            Label { text: "Preferences"; color: "#edf2fb"; font.pixelSize: 17; Layout.fillWidth: true }
            QuietButton { text: "Back"; onClicked: island.showSettings = false }
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
                    Label { text: "Notch height · " + opts.notchHeight + " px"; color: island.colors.text }
                    Slider {
                        objectName: "notchHeightSlider"
                        Layout.fillWidth: true
                        from: 48; to: 120; stepSize: 1
                        value: opts.notchHeight
                        onMoved: settings.set("notchHeight", Math.round(value))
                        Accessible.name: "Notch height"
                    }
                    Label { text: "Height when collapsed. Default: 72 px."; color: island.colors.muted; font.pixelSize: 11 }
                    Switch { text: "Reduced motion"; checked: opts.reducedMotion; onToggled: settings.set("reducedMotion", checked) }
                    Switch { text: "Shrink when the pointer leaves"; checked: opts.autoShrink; onToggled: settings.set("autoShrink", checked) }
                    ComboBox {
                        Layout.fillWidth: true
                        model: ["Companion: tuxedo", "Companion: tabby"]
                        currentIndex: opts.pet === "tabby" ? 1 : 0
                        onActivated: settings.set("pet", currentIndex === 1 ? "tabby" : "tuxedo")
                        Accessible.name: "Companion cat"
                    }
                    Switch { text: "Expand after hovering for 350 ms"; checked: opts.hoverExpand; onToggled: settings.set("hoverExpand", checked) }
                    Switch { text: "Show over fullscreen apps"; checked: opts.fullscreenOverlay; onToggled: settings.set("fullscreenOverlay", checked) }
                    ComboBox {
                        Layout.fillWidth: true
                        model: ["Middle-click: play / pause", "Middle-click: expand / collapse", "Middle-click: do nothing"]
                        currentIndex: ["media", "toggle", "none"].indexOf(opts.middleClick)
                        onActivated: settings.set("middleClick", ["media", "toggle", "none"][currentIndex])
                        Accessible.name: "Middle-click action"
                    }
                    Switch { objectName: "idleClockSwitch"; text: "Show clock and date when idle"; checked: opts.idleClock; onToggled: settings.set("idleClock", checked) }
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
                    QuietButton { text: "Reset position"; onClicked: settings.resetPosition() }
                    QuietButton { text: "Quit NEXIS"; onClicked: backend.quit() }
                }
            }

    }
}
