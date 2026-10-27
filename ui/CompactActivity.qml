import QtQuick
import QtQuick.Layouts

Item {
    id: compact
    objectName: "compactActivity"
    property var activity: ({mode: "idle", title: "", working: false, listening: false})
    property var track: ({})
    property bool reducedMotion: false
    property color foreground: "#dfdfdf"
    property color muted: "#aaaaaa"
    property color accent: "#8bbcff"
    property color attention: "#f5c16c"
    property date currentTime: new Date()
    readonly property bool idle: !activity.mode || activity.mode === "idle"
    readonly property bool music: activity.mode === "music"
    readonly property string clockText: Qt.formatTime(currentTime, "HH:mm")
    readonly property string dateText: Qt.formatDate(currentTime, "ddd, d MMM")
    readonly property string displayText: idle ? clockText + " · " + dateText : activity.title || "Working"
    readonly property string subtitle: idle ? dateText : music ? track.artist || track.identity || "" : activity.subtitle || ""
    property int titleFontSize: 15
    property int subtitleFontSize: 12
    property real maximumWidth: 640
    implicitHeight: 52
    readonly property real petWidth: Math.min(48, (height || implicitHeight) * 1.1)
    readonly property real artSize: Math.max(16, Math.min(44, (height || implicitHeight) - 10))
    readonly property real naturalTextWidth: Math.ceil(Math.max(titleMetrics.advanceWidth, titleMetrics.boundingRect.width, subtitleMetrics.advanceWidth, subtitleMetrics.boundingRect.width)) + 2
    readonly property real chromeWidth: 24 + petWidth + 8 + (music ? artSize + 17 + 16 : 0)
    readonly property real textWidth: Math.max(0, Math.min(naturalTextWidth, maximumWidth - chromeWidth))
    implicitWidth: Math.min(maximumWidth, chromeWidth + naturalTextWidth)
    Accessible.role: Accessible.Button
    Accessible.name: displayText + ". " + pet.Accessible.name
    Timer { interval: 1000; running: compact.visible && compact.idle; repeat: true; onTriggered: compact.currentTime = new Date() }
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
            Layout.preferredWidth: compact.textWidth
            Layout.minimumWidth: 0
            Layout.fillWidth: true
            spacing: 1
            Text {
                id: titleLabel
                objectName: "activityText"
                Layout.fillWidth: true
                text: compact.idle ? compact.clockText : compact.displayText
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
