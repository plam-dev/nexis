import QtQuick

Item {
    id: cat
    objectName: "tuxedoCat"
    property bool working: false
    property bool listening: false
    property bool reducedMotion: false
    property color accent: "#8bbcff"
    property int frame: 0
    property bool blinking: false
    readonly property string pose: working ? (listening ? "typingListening" : "typing") : (listening ? "listening" : "idle")
    implicitWidth: 58
    implicitHeight: 36
    Accessible.role: Accessible.Graphic
    Accessible.name: "Tuxedo cat: " + (working ? (listening ? "listening to music while typing" : "typing") : (listening ? "listening to music" : "resting"))
    onWorkingChanged: sprite.requestPaint()
    onListeningChanged: sprite.requestPaint()
    onAccentChanged: sprite.requestPaint()
    onReducedMotionChanged: { frame = 0; blinking = false; sprite.requestPaint() }
    Timer {
        interval: 170
        repeat: true
        running: cat.visible && !cat.reducedMotion && (cat.working || cat.listening)
        onTriggered: { cat.frame = (cat.frame + 1) % 8; sprite.requestPaint() }
    }
    Timer {
        interval: 5200
        repeat: true
        running: cat.visible && !cat.reducedMotion && !cat.working && !cat.listening
        onTriggered: { cat.blinking = true; unblink.restart(); sprite.requestPaint() }
    }
    Timer { id: unblink; interval: 140; onTriggered: { cat.blinking = false; sprite.requestPaint() } }
    Canvas {
        id: sprite
        anchors.centerIn: parent
        width: 40
        height: 28
        scale: Math.min(cat.width / width, cat.height / height)
        smooth: false
        antialiasing: false
        renderStrategy: Canvas.Immediate
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.imageSmoothingEnabled = false
            const black = "#191c25", edge = "#59606e", white = "#f3f0e9", shade = "#c9cbd0"
            const bob = cat.listening && !cat.reducedMotion && cat.frame % 8 >= 4 ? 1 : 0
            function px(x, y, w, h, color) { ctx.fillStyle = color; ctx.fillRect(x, y, w, h) }
            // Tail, haunches and white socks stay grounded while the head bobs.
            px(7, 19, 5, 5, edge); px(5, 17, 3, 5, black); px(4, 16, 2, 3, black)
            px(12, 15, 15, 10, edge); px(13, 14, 13, 11, black)
            px(17, 16, 5, 6, white); px(18, 21, 3, 2, shade)
            px(12, 24, 6, 2, white); px(22, 24, 6, 2, white)
            // Angular ears and a tuxedo face with white muzzle and a tiny pink nose.
            px(11, 5 + bob, 4, 6, edge); px(12, 4 + bob, 2, 7, black)
            px(24, 5 + bob, 4, 6, edge); px(25, 4 + bob, 2, 7, black)
            px(12, 9 + bob, 15, 8, edge); px(13, 8 + bob, 13, 9, black)
            px(14, 7 + bob, 11, 9, black)
            px(17, 12 + bob, 6, 5, white); px(16, 14 + bob, 8, 2, white)
            px(19, 10 + bob, 2, 3, white)
            if (!cat.blinking) {
                px(15, 11 + bob, 2, 2, "#c6dfab"); px(23, 11 + bob, 2, 2, "#c6dfab")
                px(16, 11 + bob, 1, 2, black); px(23, 11 + bob, 1, 2, black)
            } else { px(15, 12, 2, 1, shade); px(23, 12, 2, 1, shade) }
            px(19, 14 + bob, 2, 1, "#dba5a5"); px(20, 15 + bob, 1, 1, black)
            if (cat.listening) {
                px(11, 7 + bob, 2, 7, edge); px(26, 7 + bob, 2, 7, edge)
                px(13, 5 + bob, 13, 2, cat.accent)
                px(10, 10 + bob, 3, 5, cat.accent); px(26, 10 + bob, 3, 5, cat.accent)
                px(11, 11 + bob, 1, 3, black); px(27, 11 + bob, 1, 3, black)
                const note = cat.frame % 8 < 4 ? 0 : 1
                px(33, 7 + note, 1, 5, cat.accent); px(33, 7 + note, 4, 1, cat.accent)
                px(31, 11 + note, 3, 2, cat.accent)
            }
            if (cat.working) {
                const tap = !cat.reducedMotion && cat.frame % 2 === 0 ? 1 : 0
                px(13, 18 + tap, 5, 3, white); px(23, 19 - tap, 5, 3, white)
                px(11, 21, 18, 5, edge); px(12, 21, 16, 4, "#303847")
                px(13, 22, 14, 1, cat.accent); px(14, 24, 5, 1, "#7c879a")
                px(9, 26, 22, 1, shade); px(16, 26, 8, 1, white)
            }
        }
        Component.onCompleted: requestPaint()
    }
}
