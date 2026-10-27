#include "services/core/Backend.h"
#include "services/core/Compositor.h"
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
        QVERIFY(object->property("displayText").toString().contains(":"));
        QVERIFY(object->property("displayText").toString().contains(" · "));
        object->setProperty("activity", QVariantMap{{"mode", "working"},
                                                    {"title", "Working"},
                                                    {"working", true},
                                                    {"listening", false}});
        QCOMPARE(pet->property("pose").toString(), QString("typing"));
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
        const int compactWidth = window->width();
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(80, 22));
        QTRY_VERIFY(window->property("expanded").toBool());
        QCOMPARE(window->width(), 400);
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(!window->property("expanded").toBool());
        QCOMPARE(window->width(), compactWidth);
        settings.set("theme", "light");
        QVERIFY(!settings.colors()["dark"].toBool());
        settings.set("theme", "auto");
        for (int tab = 0; tab < 4; ++tab) {
            window->setProperty("expanded", true);
            window->setProperty("tab", tab);
            QTest::qWait(15);
        }
        window->setProperty("showSettings", true);
        QTest::qWait(20);
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
