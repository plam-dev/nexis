import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls as C

Item {
    id: compact
    objectName: "compactActivity"
    property var activity: ({mode: "idle", title: "", working: false, listening: false})
    property var track: ({})
    property bool reducedMotion: false
    property bool showIdleClock: false
    property date currentTime: new Date()
    property color foreground: "#dfdfdf"
    property color muted: "#aaaaaa"
    property color accent: "#8bbcff"
    property color attention: "#f5c16c"
    readonly property bool activeSurface: visible && (!Window.window || (Window.window.visible && Window.window.visibility !== Window.Minimized))
    readonly property bool idle: !activity.mode || activity.mode === "idle"
    readonly property bool clockVisible: idle && showIdleClock
    readonly property bool hasText: !idle || showIdleClock
    readonly property string clockText: Qt.formatTime(currentTime, "HH:mm")
    readonly property string dateText: Qt.formatDate(currentTime, "ddd, d MMM")
    readonly property bool music: activity.mode === "music"
    readonly property bool hasProgress: activity.mode === "feedback" && typeof activity.progress === "number"
    readonly property string displayText: idle ? (clockVisible ? clockText + " · " + dateText : "") : activity.title || "Working"
    readonly property string subtitle: clockVisible ? dateText : idle ? "" : music ? track.artist || track.identity || "" : activity.subtitle || ""
    property int titleFontSize: 15
    property int subtitleFontSize: 12
    property real maximumWidth: 640
    implicitHeight: 52
    readonly property real petWidth: Math.min(48, (height || implicitHeight) * 1.1)
    readonly property real artSize: Math.max(16, Math.min(44, (height || implicitHeight) - 10))
    readonly property real naturalTextWidth: hasText ? Math.max(hasProgress ? 140 : 0, Math.ceil(Math.max(titleMetrics.advanceWidth, titleMetrics.boundingRect.width, subtitleMetrics.advanceWidth, subtitleMetrics.boundingRect.width)) + 2) : 0
    readonly property real chromeWidth: 24 + petWidth + (hasText ? 8 : 0) + (music ? artSize + 17 + 16 : 0)
    readonly property real textWidth: Math.max(0, Math.min(naturalTextWidth, maximumWidth - chromeWidth))
    implicitWidth: Math.min(maximumWidth, chromeWidth + naturalTextWidth)
    Accessible.role: Accessible.Button
    Accessible.name: (displayText ? displayText + ". " : "") + pet.Accessible.name
    onClockVisibleChanged: if (clockVisible) currentTime = new Date()
    Timer {
        interval: 1000
        repeat: true
        running: compact.activeSurface && compact.clockVisible
        onTriggered: compact.currentTime = new Date()
    }
    TextMetrics { id: titleMetrics; text: titleLabel.text; font: titleLabel.font }
    TextMetrics { id: subtitleMetrics; text: compact.subtitle; font: subtitleLabel.font }
    RowLayout {
        id: content
        objectName: "capsuleContent"
        anchors.centerIn: parent
        width: Math.max(0, Math.min(compact.width - 24, compact.implicitWidth - 24))
        spacing: 8
        TrackArtwork {
            visible: compact.music
            Layout.preferredWidth: compact.artSize
            Layout.minimumWidth: compact.artSize
            Layout.maximumWidth: compact.artSize
            Layout.preferredHeight: Layout.preferredWidth
            source: compact.track.art || ""
            fallbackColor: compact.accent
        }
        ColumnLayout {
            visible: compact.hasText
            Layout.preferredWidth: compact.textWidth
            Layout.minimumWidth: 0
            Layout.fillWidth: true
            spacing: 1
            Text {
                id: titleLabel
                objectName: "activityText"
                Layout.fillWidth: true
                text: compact.clockVisible ? compact.clockText : compact.displayText
                textFormat: Text.PlainText
                color: compact.activity.mode === "attention" ? compact.attention : compact.foreground
                font.pixelSize: compact.titleFontSize
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            Text {
                id: subtitleLabel
                objectName: "subtitleText"
                visible: text.length > 0
                Layout.fillWidth: true
                text: compact.subtitle
                textFormat: Text.PlainText
                color: compact.muted
                font.pixelSize: compact.subtitleFontSize
                elide: Text.ElideRight
            }
            C.ProgressBar {
                id: feedbackProgress
                objectName: "feedbackProgress"
                visible: compact.hasProgress
                Layout.fillWidth: true
                Layout.topMargin: 4
                from: 0; to: 1
                value: compact.hasProgress ? Math.max(0, Math.min(1, compact.activity.progress)) : 0
                Accessible.name: compact.displayText
                background: Rectangle { implicitHeight: 3; radius: 2; color: compact.muted; opacity: 0.25 }
                contentItem: Item { implicitHeight: 3; Rectangle { width: feedbackProgress.visualPosition * parent.width; height: parent.height; radius: 2; color: compact.accent } }
            }
        }
        TuxedoCat {
            id: pet
            Layout.preferredWidth: compact.petWidth
            Layout.minimumWidth: compact.petWidth
            Layout.maximumWidth: compact.petWidth
            Layout.preferredHeight: Math.min(38, compact.height || compact.implicitHeight)
            working: !!compact.activity.working
            listening: !!compact.activity.listening
            reducedMotion: compact.reducedMotion
            accent: compact.accent
        }
        PlaybackPulse {
            objectName: "playbackPulse"
            Layout.preferredWidth: 17
            Layout.minimumWidth: 17
            Layout.maximumWidth: 17
            visible: compact.music
            playing: !!compact.track.playing || !!compact.activity.listening
            reducedMotion: compact.reducedMotion
            color: compact.accent
        }
    }
}
