import QtQuick
import QtQuick.Window

Item {
    id: cat
    objectName: "tabbyCat"
    property bool working: false
    property bool listening: false
    property bool reducedMotion: false
    property color accent: "#8bbcff"
    property int frame: 0
    property int idleCycle: 0
    property bool blinking: false
    readonly property bool activeSurface: visible && (!Window.window || (Window.window.visible && Window.window.visibility !== Window.Minimized))
    readonly property string pose: working ? (listening ? "typingListening" : "typing") : (listening ? "listening" : "idle")
    implicitWidth: 58
    implicitHeight: 36
    Accessible.role: Accessible.Graphic
    Accessible.name: "Tabby cat: " + (working ? (listening ? "listening to music while typing" : "typing") : (listening ? "listening to music" : "resting"))
    onWorkingChanged: sprite.requestPaint()
    onListeningChanged: sprite.requestPaint()
    onAccentChanged: sprite.requestPaint()
    onReducedMotionChanged: { frame = 0; idleCycle = 0; blinking = false; sprite.requestPaint() }
    Timer {
        interval: cat.working || cat.listening ? 190 : 440
        repeat: true
        running: cat.activeSurface && !cat.reducedMotion
        onTriggered: {
            cat.frame = (cat.frame + 1) % 8
            if (!cat.working && !cat.listening && cat.frame === 0)
                cat.idleCycle = (cat.idleCycle + 1) % 4
            sprite.requestPaint()
        }
    }
    Timer {
        interval: 6100
        repeat: true
        running: cat.activeSurface && !cat.reducedMotion && !cat.working && !cat.listening
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
            const idle = !cat.working && !cat.listening && !cat.reducedMotion
            const breath = idle && cat.frame >= 2 && cat.frame < 6 ? 1 : 0
            const ear = idle && cat.idleCycle === 1 && (cat.frame === 2 || cat.frame === 3) ? 1 : 0
            const tail = idle && cat.idleCycle === 3 && cat.frame >= 2 && cat.frame < 6 ? 1 : 0
            const bob = cat.listening && !cat.reducedMotion && cat.frame % 8 >= 4 ? 1 : -breath
            function px(x, y, w, h, color) { ctx.fillStyle = color; ctx.fillRect(x, y, w, h) }
            // Grey-brown tabby with dark stripes, an orange patch and a cream muzzle.
            const out = "#2b231d", fur = "#756450", stripe = "#3b3029", paw = "#9a8468", cream = "#eadfca", orange = "#c98a4b", innerEar = "#d8a0a0"
            px(7, 19, 5, 5, out); px(5, 17 - tail, 3, 5 + tail, fur); px(4 + tail, 16 - tail, 2, 3, fur)
            px(5, 19 - tail, 3, 1, stripe); px(5, 21 - tail, 3, 1, stripe); px(4 + tail, 17 - tail, 2, 1, stripe)
            px(12, 15 - breath, 15, 10 + breath, out); px(13, 14 - breath, 13, 11 + breath, fur)
            px(15, 15 - breath, 1, 6, stripe); px(23, 15 - breath, 1, 6, stripe); px(13, 18, 2, 1, stripe); px(24, 18, 2, 1, stripe)
            px(22, 20, 4, 3, orange)
            px(17, 16 - breath, 5, 6 + breath, cream); px(18, 21, 3, 2, "#cfc3ad")
            px(12, 24, 6, 2, paw); px(22, 24, 6, 2, paw)
            // Angular ears with pink insides, an "M" forehead mark and cheek stripes.
            px(11, 5 + bob + ear, 4, 6 - ear, out); px(12, 4 + bob + ear, 2, 7 - ear, fur); px(12, 6 + bob + ear, 1, 3 - ear, innerEar)
            px(24, 5 + bob, 4, 6, out); px(25, 4 + bob, 2, 7, fur); px(26, 6 + bob, 1, 3, innerEar)
            px(12, 9 + bob, 15, 8, out); px(13, 8 + bob, 13, 9, fur)
            px(14, 7 + bob, 11, 9, fur)
            px(16, 8 + bob, 1, 2, stripe); px(19, 7 + bob, 2, 3, stripe); px(23, 8 + bob, 1, 2, stripe)
            px(13, 13 + bob, 2, 1, stripe); px(25, 13 + bob, 2, 1, stripe); px(13, 15 + bob, 2, 1, stripe); px(25, 15 + bob, 2, 1, stripe)
            px(23, 9 + bob, 2, 2, orange)
            px(17, 12 + bob, 6, 5, cream); px(16, 14 + bob, 8, 2, cream)
            if (!cat.blinking) {
                px(15, 11 + bob, 2, 2, "#d8e08a"); px(23, 11 + bob, 2, 2, "#d8e08a")
                px(16, 11 + bob, 1, 2, out); px(23, 11 + bob, 1, 2, out)
            } else { px(15, 12 + bob, 2, 1, stripe); px(23, 12 + bob, 2, 1, stripe) }
            px(19, 14 + bob, 2, 1, "#d98f8f"); px(20, 15 + bob, 1, 1, out)
            if (cat.listening) {
                px(11, 7 + bob, 2, 7, out); px(26, 7 + bob, 2, 7, out)
                px(13, 5 + bob, 13, 2, cat.accent)
                px(10, 10 + bob, 3, 5, cat.accent); px(26, 10 + bob, 3, 5, cat.accent)
                px(11, 11 + bob, 1, 3, out); px(27, 11 + bob, 1, 3, out)
                const note = cat.frame % 8 < 4 ? 0 : 1
                px(33, 7 + note, 1, 5, cat.accent); px(33, 7 + note, 4, 1, cat.accent)
                px(31, 11 + note, 3, 2, cat.accent)
            }
            if (cat.working) {
                const tap = !cat.reducedMotion && cat.frame % 2 === 1 ? 1 : 0
                px(13, 18 + tap, 5, 3, paw); px(23, 19 - tap, 5, 3, paw)
                px(11, 21, 18, 5, out); px(12, 21, 16, 4, "#303847")
                px(13, 22, 14, 1, cat.accent); px(14, 24, 5, 1, "#7c879a")
                px(9, 26, 22, 1, "#c9cbd0"); px(16, 26, 8, 1, cream)
            }
        }
        Component.onCompleted: requestPaint()
    }
}
