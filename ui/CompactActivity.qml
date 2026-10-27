import QtQuick
import QtQuick.Layouts

Item {
    id: compact
    objectName: "compactActivity"
    property var activity: ({mode: "idle", title: "", working: false, listening: false})
    property bool reducedMotion: false
    property color foreground: "#dfdfdf"
    property color muted: "#aaaaaa"
    property color accent: "#8bbcff"
    property color attention: "#f5c16c"
    property date currentTime: new Date()
    readonly property bool idle: !activity.mode || activity.mode === "idle"
    readonly property string clockText: Qt.formatTime(currentTime, "HH:mm")
    readonly property string dateText: Qt.formatDate(currentTime, "ddd, d MMM")
    readonly property string displayText: idle ? clockText + " · " + dateText : activity.title || "Working"
    implicitHeight: 36
    implicitWidth: Math.max(170, Math.min(310, textWidth.advanceWidth + 80))
    Accessible.role: Accessible.Button
    Accessible.name: displayText + ". " + pet.Accessible.name
    Timer { interval: 1000; running: compact.visible && compact.idle; repeat: true; onTriggered: compact.currentTime = new Date() }
    TextMetrics { id: textWidth; text: compact.displayText; font.pixelSize: 12 }
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 5
        anchors.rightMargin: 12
        spacing: 8
        TuxedoCat {
            id: pet
            Layout.preferredWidth: Math.min(66, compact.height * 1.55)
            Layout.fillHeight: true
            working: !!compact.activity.working
            listening: !!compact.activity.listening
            reducedMotion: compact.reducedMotion
            accent: compact.accent
        }
        Text {
            objectName: "activityText"
            Layout.fillWidth: true
            text: compact.displayText
            textFormat: Text.PlainText
            color: compact.activity.mode === "attention" ? compact.attention : compact.foreground
            font.pixelSize: 12
            font.weight: compact.idle ? Font.Medium : Font.DemiBold
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }
    }
}
