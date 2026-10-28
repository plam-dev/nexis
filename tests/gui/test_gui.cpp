#include "services/core/Backend.h"
#include "services/core/Compositor.h"
#include "services/core/NotchGeometry.h"
#include <QHostAddress>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTcpServer>
#include <QtTest>
using namespace Nexis;
class HostileMedia : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap current READ current CONSTANT)
    Q_PROPERTY(QVariantList players READ players CONSTANT)
    Q_PROPERTY(QString error READ error CONSTANT)
  public:
    QString title;
    QVariantMap current() const {
        return {{"service", "org.mpris.MediaPlayer2.hostile"},
                {"title", title},
                {"artist", title},
                {"identity", title},
                {"playing", false},
                {"duration", 0},
                {"position", 0},
                {"art", ""},
                {"canPlayPause", false},
                {"canSeek", false},
                {"canNext", false},
                {"canPrevious", false}};
    }
    QVariantList players() const { return {current()}; }
    QString error() const { return {}; }
};
class GuiTest : public QObject {
    Q_OBJECT
  private slots:
    void notchHasShouldersAndOnlyRoundedBottomCorners() {
        const auto path = notchPath(QSizeF(440, 72));
        QVERIFY(path.contains(QPointF(12, 1)));
        QVERIFY(path.contains(QPointF(220, 1)));
        QVERIFY(!path.contains(QPointF(2, 35)));
        QVERIFY(path.contains(QPointF(26, 35)));
        QVERIFY(path.contains(QPointF(220, 70)));
        QVERIFY(!path.contains(QPointF(25, 71)));
    }
    void notchBackgroundMatchesItsInputShape() {
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl("qrc:/nexis/NotchBackground.qml"));
        QScopedPointer<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *item = qobject_cast<QQuickItem *>(object.data());
        QQuickWindow window;
        window.setColor(Qt::transparent);
        window.resize(440, 72);
        item->setParentItem(window.contentItem());
        item->setSize(QSizeF(440, 72));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTest::qWait(30);
        const auto image = window.grabWindow();
        QVERIFY(!image.isNull());
        const auto ratio = window.devicePixelRatio();
        auto pixel = [&](int x, int y) { return image.pixelColor(qRound(x * ratio), qRound(y * ratio)); };
        QVERIFY(pixel(220, 30).alpha() > 240);
        QVERIFY(pixel(2, 35).alpha() < 10);
        QVERIFY(pixel(25, 71).alpha() < 10);
        QVERIFY(pixel(220, 70).alpha() > 240);
    }
    void foldCapsuleFitsContentAndCentersTheWholeGroup() {
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl("qrc:/nexis/CompactActivity.qml"));
        QScopedPointer<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *item = qobject_cast<QQuickItem *>(object.data());
        object->setProperty("reducedMotion", true);
        object->setProperty("activity", QVariantMap{{"mode", "music"}, {"title", "Hi"}, {"listening", true}});
        object->setProperty("track", QVariantMap{{"title", "Hi"}, {"artist", "Artist"}, {"playing", true}});
        QCoreApplication::processEvents();
        auto shortWidth = item->implicitWidth();
        object->setProperty("activity", QVariantMap{{"mode", "music"}, {"title", QString(200, 'W')}, {"listening", true}});
        QTRY_VERIFY(item->implicitWidth() > shortWidth);
        QVERIFY(item->implicitWidth() > 420);
        QVERIFY(item->implicitWidth() <= 640);
        item->setSize(QSizeF(500, 44));
        auto *content = item->findChild<QQuickItem *>("capsuleContent");
        QVERIFY(content);
        QTRY_VERIFY(qAbs(content->x() + content->width() / 2 - item->width() / 2) < 1);
        object->setProperty("activity", QVariantMap{{"mode", "idle"}});
        QVERIFY(item->implicitWidth() < 300);
    }
    void longestLineFitsUntilTheAvailableWidthIsExhausted() {
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl("qrc:/nexis/CompactActivity.qml"));
        QScopedPointer<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *item = qobject_cast<QQuickItem *>(object.data());
        object->setProperty("maximumWidth", 1000);
        object->setProperty("reducedMotion", true);
        object->setProperty("activity", QVariantMap{{"mode", "music"}, {"title", "Short"}});
        object->setProperty("track", QVariantMap{{"artist", QString(60, 'M')}});
        QQuickWindow window;
        item->setParentItem(window.contentItem());
        item->setSize(QSizeF(item->implicitWidth(), 58));
        window.resize(qCeil(item->implicitWidth()), 58);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *subtitle = item->findChild<QQuickItem *>("subtitleText");
        QVERIFY(subtitle);
        QTest::qWait(30);
        QVERIFY(!subtitle->property("truncated").toBool());
        QVERIFY(item->implicitWidth() < 1000);
        object->setProperty("maximumWidth", 320);
        item->setWidth(item->implicitWidth());
        window.resize(qCeil(item->implicitWidth()), 58);
        QTest::qWait(30);
        QCOMPARE(item->implicitWidth(), 320);
        QVERIFY(subtitle->property("truncated").toBool());
    }
    void pulseStopsWhenPausedHiddenOrReduced() {
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl("qrc:/nexis/PlaybackPulse.qml"));
        QScopedPointer<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        object->setProperty("playing", true);
        QTRY_VERIFY(object->property("animating").toBool());
        object->setProperty("playing", false);
        QVERIFY(!object->property("animating").toBool());
        object->setProperty("playing", true);
        object->setProperty("reducedMotion", true);
        QVERIFY(!object->property("animating").toBool());
        object->setProperty("reducedMotion", false);
        object->setProperty("visible", false);
        QVERIFY(!object->property("animating").toBool());
        QQuickWindow window;
        auto *item = qobject_cast<QQuickItem *>(object.data());
        item->setParentItem(window.contentItem());
        object->setProperty("visible", true);
        window.resize(80, 40);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTRY_VERIFY(object->property("animating").toBool());
        window.hide();
        QVERIFY(!object->property("animating").toBool());
    }
    void artworkFallsBackAndFoldHeightFollowsContent() {
        QQmlEngine engine;
        QQmlComponent artwork(&engine, QUrl("qrc:/nexis/TrackArtwork.qml"));
        QScopedPointer<QObject> art(artwork.create());
        QVERIFY2(art, qPrintable(artwork.errorString()));
        QVERIFY(art->property("fallbackVisible").toBool());
        art->setProperty("source", QUrl("file:///nonexistent/nexis-cover.png"));
        QTRY_VERIFY(art->property("fallbackVisible").toBool());
        QQmlComponent surface(&engine, QUrl("qrc:/nexis/FoldSurface.qml"));
        QScopedPointer<QObject> object(surface.create());
        QVERIFY2(object, qPrintable(surface.errorString()));
        auto *item = qobject_cast<QQuickItem *>(object.data());
        item->setWidth(376);
        QQuickWindow window;
        item->setParentItem(window.contentItem());
        item->setHeight(500);
        window.resize(376, 500);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        object->setProperty("snapshot", QVariantMap{{"media", QVariantMap{{"service", "test"}, {"title", "Track"}, {"artist", "Artist"}, {"playing", true}}}, {"presentation", QVariantMap{{"mode", "music"}, {"listening", true}}}});
        QCoreApplication::processEvents();
        auto musicHeight = item->implicitHeight();
        QVERIFY(musicHeight > 120 && musicHeight < 330);
        object->setProperty("snapshot", QVariantMap{{"notifications", QVariantList{QVariantMap{{"id", 1}, {"title", "Message"}, {"body", "A short message"}, {"appName", "Test"}}}}});
        QCoreApplication::processEvents();
        QVERIFY(item->implicitHeight() > 80);
        QVERIFY(item->implicitHeight() < 500);
        QVariantList notices;
        for (int i = 0; i < 3; ++i)
            notices << QVariantMap{{"id", i + 1}, {"title", "Message"}, {"body", QString(300, 'W')}, {"appName", "Test"}};
        object->setProperty("snapshot", QVariantMap{{"notifications", notices}});
        object->setProperty("section", 0);
        object->setProperty("maximumHeight", 250);
        QTest::qWait(30);
        QVERIFY(item->implicitHeight() <= 250);
    }
    void foldHonorsLightAppearance() {
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl("qrc:/nexis/FoldSurface.qml"));
        QScopedPointer<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        object->setProperty("snapshot", QVariantMap{{"options", QVariantMap{{"theme", "light"}}}});
        QVERIFY(object->property("color").value<QColor>().lightnessF() > 0.8);
        QVERIFY(object->property("foreground").value<QColor>().lightnessF() < 0.4);
    }
    void foldKeepsJobProgressAndForwardsPlaybackActions() {
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl("qrc:/nexis/FoldSurface.qml"));
        QScopedPointer<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        object->setProperty("snapshot", QVariantMap{{"media", QVariantMap{{"service", "test"}, {"playing", true}, {"canPlayPause", true}}}, {"jobs", QVariantList{QVariantMap{{"title", "Build"}, {"status", "progress"}, {"progress", 45}}}}});
        object->setProperty("section", 1);
        auto *jobs = object->findChild<QObject *>("jobsList");
        QVERIFY(jobs);
        QTRY_COMPARE(jobs->property("count").toInt(), 1);
        object->setProperty("section", 0);
        QSignalSpy actions(object.data(), SIGNAL(action(QString,QVariant)));
        auto *play = object->findChild<QObject *>("playPauseButton");
        QVERIFY(play);
        QVERIFY(play->property("enabled").toBool());
        QVERIFY(QMetaObject::invokeMethod(play, "clicked"));
        QCOMPARE(actions.size(), 1);
        QCOMPARE(actions[0][0].toString(), QString("playPause"));
    }
    void persistedFalseStillAnimatesRenderedMusicPet() {
        Settings settings;
        settings.set("reducedMotion", QString("false"));
        Media media;
        System system;
        Backend backend(&settings, &media, &system, false);
        auto snapshot =
            QJsonDocument::fromJson(backend.Snapshot().toUtf8()).object().toVariantMap();
        snapshot["presentation"] =
            presentation({}, {{"playing", true}, {"title", "Bambina"}}, {}, 10000);
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData(R"(import QtQuick
CompactActivity {
    property var snapshot: ({})
    activity: snapshot.presentation || ({})
    reducedMotion: !!(snapshot.options || {}).reducedMotion
})",
                          QUrl("qrc:/nexis/SnapshotPetTest.qml"));
        QScopedPointer<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        object->setProperty("snapshot", snapshot);
        auto *pet = object->findChild<QObject *>("tuxedoCat");
        QVERIFY(pet);
        QVERIFY(!pet->property("reducedMotion").toBool());
        QCOMPARE(object->property("displayText").toString(), QString("Bambina"));
        auto *item = qobject_cast<QQuickItem *>(object.data());
        QQuickWindow window;
        item->setParentItem(window.contentItem());
        item->setSize(QSizeF(250, 44));
        window.resize(250, 44);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        const auto first = window.grabWindow();
        QVERIFY(!first.isNull());
        QTRY_VERIFY(window.grabWindow() != first);
    }
    void idlePetBreathesAndStopsWhenHiddenOrReduced() {
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl("qrc:/nexis/TuxedoCat.qml"));
        QScopedPointer<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *item = qobject_cast<QQuickItem *>(object.data());
        QVERIFY(item);
        QQuickWindow window;
        item->setParentItem(window.contentItem());
        item->setSize(QSizeF(116, 72));
        window.resize(116, 72);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTest::qWait(50);
        QCOMPARE(object->property("pose").toString(), QString("idle"));
        const auto resting = window.grabWindow();
        QVERIFY(!resting.isNull());
        // A breath must be visible well before the occasional 5.2-second blink.
        QTRY_VERIFY_WITH_TIMEOUT(window.grabWindow() != resting, 1800);
        QVERIFY(!object->property("blinking").toBool());
        window.hide();
        const auto hiddenFrame = object->property("frame").toInt();
        QTest::qWait(900);
        QCOMPARE(object->property("frame").toInt(), hiddenFrame);
        window.show();
        QTRY_VERIFY(object->property("frame").toInt() != hiddenFrame);
        object->setProperty("reducedMotion", true);
        QTest::qWait(50);
        const auto reduced = window.grabWindow();
        QTest::qWait(900);
        QCOMPARE(window.grabWindow(), reduced);
        object->setProperty("reducedMotion", false);
        QTRY_VERIFY_WITH_TIMEOUT(window.grabWindow() != reduced, 1800);
    }
    void idlePetOccasionallyMovesItsEarsAndTail() {
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl("qrc:/nexis/TuxedoCat.qml"));
        QScopedPointer<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *item = qobject_cast<QQuickItem *>(object.data());
        QQuickWindow window;
        item->setParentItem(window.contentItem());
        item->setSize(QSizeF(160, 112));
        window.resize(160, 112);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto capture = [&](int cycle) {
            object->setProperty("idleCycle", cycle);
            object->setProperty("frame", 3);
            QMetaObject::invokeMethod(item->childItems().first(), "requestPaint");
            QTest::qWait(30);
            return window.grabWindow();
        };
        const auto resting = capture(0);
        QVERIFY(!resting.isNull());
        QVERIFY(capture(1) != resting);
        QVERIFY(capture(3) != resting);
        object->setProperty("reducedMotion", true);
        QCOMPARE(capture(1), capture(3));
    }
    void persistentPetCombinesStatesAndHonorsReducedMotion() {
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl("qrc:/nexis/CompactActivity.qml"));
        QScopedPointer<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *item = qobject_cast<QQuickItem *>(object.data());
        QVERIFY(item);
        QQuickWindow window;
        item->setParentItem(window.contentItem());
        item->setWidth(250);
        item->setHeight(44);
        window.resize(250, 44);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *pet = object->findChild<QObject *>("tuxedoCat");
        QVERIFY(pet);
        QVERIFY(pet->property("visible").toBool());
        QVERIFY(object->property("displayText").toString().isEmpty());
        QVERIFY(item->implicitWidth() < 120);
        QVERIFY(!object->findChild<QQuickItem *>("activityText")->isVisible());
        const auto idleWidth = item->implicitWidth();
        auto *content = object->findChild<QQuickItem *>("capsuleContent");
        QVERIFY(content);
        QTRY_VERIFY(qAbs(content->x() + content->width() / 2 - item->width() / 2) < 1);
        object->setProperty("activity", presentation(
            {QVariantMap{{"status", "running"}}}, {}, {}, 10000));
        QVERIFY(object->property("displayText").toString().isEmpty());
        QCOMPARE(pet->property("pose").toString(), QString("typing"));
        const auto typingFrame = pet->property("frame").toInt();
        QTRY_VERIFY(pet->property("frame").toInt() != typingFrame);
        object->setProperty("activity", QVariantMap{{"mode", "music"},
                                                    {"title", "Music"},
                                                    {"working", false},
                                                    {"listening", true}});
        QCOMPARE(pet->property("pose").toString(), QString("listening"));
        object->setProperty("activity", QVariantMap{{"mode", "music"},
                                                    {"title", "Music"},
                                                    {"working", true},
                                                    {"listening", true}});
        QCOMPARE(pet->property("pose").toString(), QString("typingListening"));
        QCOMPARE(object->property("displayText").toString(), QString("Music"));
        const auto frame = pet->property("frame").toInt();
        QTRY_VERIFY(pet->property("frame").toInt() != frame);
        object->setProperty("reducedMotion", true);
        QCOMPARE(pet->property("frame").toInt(), 0);
        QTest::qWait(220);
        QCOMPARE(pet->property("frame").toInt(), 0);
        QVERIFY(pet->property("visible").toBool());
        object->setProperty("activity", QVariantMap{{"mode", "feedback"},
                                                    {"title", "Brightness · 40%"},
                                                    {"progress", 0.4}});
        QCOMPARE(object->property("displayText").toString(), QString("Brightness · 40%"));
        auto *progress = object->findChild<QQuickItem *>("feedbackProgress");
        QVERIFY(progress);
        QTRY_VERIFY(progress->isVisible());
        QCOMPARE(progress->property("value").toDouble(), 0.4);
        QVERIFY(item->implicitWidth() > idleWidth);
        object->setProperty("activity", QVariantMap{{"mode", "feedback"}, {"title", "Audio Muted"}});
        QVERIFY(!progress->isVisible());
        object->setProperty("activity", QVariantMap{{"mode", "idle"}});
        QCOMPARE(item->implicitWidth(), idleWidth);
        QVERIFY(!progress->isVisible());
        QVERIFY(!object->findChild<QQuickItem *>("activityText")->isVisible());
        object->setProperty("showIdleClock", true);
        object->setProperty("currentTime", QDateTime(QDate(2026, 10, 28), QTime(9, 7)));
        QVERIFY(object->property("displayText").toString().startsWith("09:07 · "));
        QVERIFY(object->findChild<QQuickItem *>("activityText")->isVisible());
        QVERIFY(object->findChild<QQuickItem *>("subtitleText")->isVisible());
        QVERIFY(item->implicitWidth() > idleWidth);
        object->setProperty("activity", QVariantMap{{"mode", "feedback"},
                                                    {"title", "Volume · 60%"}, {"progress", 0.6}});
        QCOMPARE(object->property("displayText").toString(), QString("Volume · 60%"));
        object->setProperty("activity", QVariantMap{{"mode", "idle"}});
        QVERIFY(!object->property("displayText").toString().isEmpty());
        object->setProperty("showIdleClock", false);
        QVERIFY(object->property("displayText").toString().isEmpty());
        QCOMPARE(item->implicitWidth(), idleWidth);
    }
    void mediaTextCannotMakeNetworkRequests() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        HostileMedia hostile;
        hostile.title =
            "<img src=\"http://127.0.0.1:" + QString::number(server.serverPort()) + "/track.png\">";
        Settings settings;
        settings.set("reducedMotion", true);
        Media media;
        System system;
        Backend backend(&settings, &media, &system, false);
        Compositor compositor;
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("settings", &settings);
        engine.rootContext()->setContextProperty("media", &hostile);
        engine.rootContext()->setContextProperty("systemInfo", &system);
        engine.rootContext()->setContextProperty("backend", &backend);
        engine.rootContext()->setContextProperty("compositor", &compositor);
        engine.load(QUrl("qrc:/nexis/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        window->setProperty("expanded", true);
        window->setProperty("tab", 2);
        window->setProperty("ready", true);
        window->show();
        QTest::qWait(500);
        QVERIFY(!server.hasPendingConnections());
    }
    void hoverExpansionDoesNotTakeFocus() {
        Settings settings;
        settings.set("reducedMotion", true);
        settings.set("hoverExpand", true);
        Media media;
        System system;
        Backend backend(&settings, &media, &system, false);
        Compositor compositor;
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("settings", &settings);
        engine.rootContext()->setContextProperty("media", &media);
        engine.rootContext()->setContextProperty("systemInfo", &system);
        engine.rootContext()->setContextProperty("backend", &backend);
        engine.rootContext()->setContextProperty("compositor", &compositor);
        engine.load(QUrl("qrc:/nexis/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        window->setProperty("ready", true);
        window->show();
        QQuickWindow other;
        other.show();
        other.requestActivate();
        QTRY_VERIFY(other.isActive());
        QTest::mouseMove(window, QPoint(80, 22));
        QTest::qWait(450);
        QVERIFY(window->property("expanded").toBool());
        QVERIFY(other.isActive());
        settings.set("hoverExpand", false);
    }
    void capsuleRespondsToClickAndEscapeWithoutAnimation() {
        Settings settings;
        settings.set("idleClock", false);
        settings.set("reducedMotion", true);
        settings.set("hoverExpand", false);
        Media media;
        System system;
        Backend backend(&settings, &media, &system, false);
        Compositor compositor;
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("settings", &settings);
        engine.rootContext()->setContextProperty("media", &media);
        engine.rootContext()->setContextProperty("systemInfo", &system);
        engine.rootContext()->setContextProperty("backend", &backend);
        engine.rootContext()->setContextProperty("compositor", &compositor);
        QSignalSpy warnings(&engine, &QQmlApplicationEngine::warnings);
        engine.load(QUrl("qrc:/nexis/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        window->setProperty("ready", true);
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QVERIFY(!window->property("expanded").toBool());
        auto *compact = window->findChild<QObject *>("compactActivity");
        QVERIFY(compact);
        QVERIFY(compact->property("displayText").toString().isEmpty());
        auto *clockSwitch = window->findChild<QObject *>("idleClockSwitch");
        QVERIFY(clockSwitch);
        clockSwitch->setProperty("checked", true);
        QVERIFY(QMetaObject::invokeMethod(clockSwitch, "toggled"));
        QTRY_VERIFY(!compact->property("displayText").toString().isEmpty());
        Settings reloaded;
        QVERIFY(reloaded.options()["idleClock"].toBool());
        clockSwitch->setProperty("checked", false);
        QVERIFY(QMetaObject::invokeMethod(clockSwitch, "toggled"));
        QTRY_VERIFY(compact->property("displayText").toString().isEmpty());
        settings.set("notchHeight", 96);
        QCOMPARE(window->height(), 96);
        settings.set("notchHeight", 48);
        QCOMPARE(window->height(), 48);
        settings.set("notchHeight", 72);
        QCOMPARE(window->height(), 72);
        const int compactWidth = window->width();
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(80, 22));
        QTRY_VERIFY(window->property("expanded").toBool());
        QCOMPARE(window->width(), qMin(560, qFloor(window->screen()->geometry().width() * 0.6)));
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(!window->property("expanded").toBool());
        QCOMPARE(window->width(), compactWidth);
        settings.set("theme", "light");
        QVERIFY(!settings.colors()["dark"].toBool());
        settings.set("theme", "auto");
        for (int tab = 0; tab < 3; ++tab) {
            window->setProperty("expanded", true);
            window->setProperty("tab", tab);
            QTest::qWait(15);
        }
        window->setProperty("expanded", false);
        backend.ingest(normalize({{"hook_event_name", "PermissionRequest"}, {"session_id", "default-menu"}, {"cwd", "/work/project"}}, "claude_code"));
        window->setProperty("expanded", true);
        QCOMPARE(window->property("tab").toInt(), 0);
        auto *surface = window->findChild<QObject *>("foldSurface");
        QVERIFY(surface);
        for (auto *child : surface->findChildren<QObject *>())
            QVERIFY(child->property("text").toString() != "Activity");
        window->setProperty("showSettings", true);
        QTest::qWait(20);
        auto *heightSlider = window->findChild<QObject *>("notchHeightSlider");
        QVERIFY(heightSlider);
        heightSlider->setProperty("value", 88);
        QVERIFY(QMetaObject::invokeMethod(heightSlider, "moved"));
        QCOMPARE(settings.options()["notchHeight"].toInt(), 88);
        window->setProperty("expanded", false);
        QCOMPARE(window->height(), 88);
        settings.set("notchHeight", 72);
        QCOMPARE(warnings.size(), 0);
    }
};
int main(int argc, char **argv) {
    QQuickStyle::setStyle("Fusion");
    QGuiApplication app(argc, argv);
    GuiTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "test_gui.moc"
