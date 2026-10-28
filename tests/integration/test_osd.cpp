#include "services/core/Backend.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QJsonDocument>
#include <QScopeGuard>
#include <QtTest>
using namespace Nexis;

static const QString service = "org.kde.plasmashell";
static const QString path = "/org/kde/osdService";
static const QString iface = "org.kde.osdService";

class OsdTest : public QObject {
    Q_OBJECT
    QDBusConnection plasma = QDBusConnection::connectToBus(QDBusConnection::SessionBus, "plasma");
    Settings settings;
    Media media;
    System system;
    static QVariantMap state(const Backend &backend) {
        return QJsonDocument::fromJson(backend.Snapshot().toUtf8())
            .object()["presentation"]
            .toObject()
            .toVariantMap();
    }
    static void progress(const QDBusConnection &bus, const QString &icon, int value,
                         int maximum = 100, const QString &text = {}) {
        auto signal = QDBusMessage::createSignal(path, iface, "osdProgress");
        signal << icon << value << maximum << text;
        QVERIFY(bus.send(signal));
    }
    static void text(const QDBusConnection &bus, const QString &icon, const QString &value) {
        auto signal = QDBusMessage::createSignal(path, iface, "osdText");
        signal << icon << value;
        QVERIFY(bus.send(signal));
    }
  private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void init() {
        QVERIFY(plasma.registerService(service));
        QTest::qWait(20);
    }
    void cleanup() {
        plasma.unregisterService(service);
        QTest::qWait(20);
    }
    void nativeChangesKeepTheCatAndLatestAdjustmentRestartsExpiry() {
        Backend backend(&settings, &media, &system, false);
        backend.ingest(normalize(
            {{"hook_event_name", "UserPromptSubmit"}, {"session_id", "feedback"}, {"cwd", "/work"}},
            "claude_code"));
        QTest::qWait(30);
        QCOMPARE(state(backend)["mode"].toString(), QString("idle"));
        progress(plasma, "audio-volume-medium", 63);
        QTRY_COMPARE(state(backend)["mode"].toString(), QString("feedback"));
        QCOMPARE(state(backend)["title"].toString(), QString("Volume · 63%"));
        QCOMPARE(state(backend)["progress"].toDouble(), 0.63);
        QVERIFY(state(backend)["working"].toBool());
        QTest::qWait(1900);
        progress(plasma, "video-display-brightness", 40, 100, "Laptop screen");
        QTRY_COMPARE(state(backend)["title"].toString(), QString("Brightness · 40%"));
        QCOMPARE(state(backend)["subtitle"].toString(), QString("Laptop screen"));
        QTest::qWait(1300);
        QCOMPARE(state(backend)["mode"].toString(), QString("feedback"));
        QSignalSpy changed(&backend, &Backend::changed);
        QTRY_COMPARE_WITH_TIMEOUT(state(backend)["mode"].toString(), QString("idle"), 2500);
        QVERIFY(!changed.isEmpty());
        QVERIFY(state(backend)["working"].toBool());
        QVERIFY(!state(backend).contains("progress"));
    }
    void textFeedbackAndProgressAreBounded() {
        Backend backend(&settings, &media, &system, false);
        QTest::qWait(30);
        text(plasma, "audio-volume-muted", "Audio Muted");
        QTRY_COMPARE(state(backend)["title"].toString(), QString("Audio Muted"));
        QVERIFY(!state(backend).contains("progress"));
        progress(plasma, "audio-volume-high", 125, 150);
        QTRY_COMPARE(state(backend)["title"].toString(), QString("Volume · 125%"));
        QCOMPARE(state(backend)["progress"].toDouble(), 125.0 / 150);
        progress(plasma, "input-keyboard-brightness", 20);
        QTRY_COMPARE(state(backend)["title"].toString(), QString("Keyboard brightness · 20%"));
        progress(plasma, "microphone-sensitivity-high", 70);
        QTRY_COMPARE(state(backend)["title"].toString(), QString("Microphone · 70%"));
        progress(plasma, "audio-volume-high", -1);
        progress(plasma, "audio-volume-high", 101);
        progress(plasma, "audio-volume-high", 0, 0);
        QTest::qWait(40);
        QCOMPARE(state(backend)["title"].toString(), QString("Microphone · 70%"));
        text(plasma, "network-wireless-off", QString(10000, 'W'));
        QTRY_COMPARE(state(backend)["title"].toString().size(), 200);
        QVERIFY(!state(backend).contains("progress"));
    }
    void rejectsOtherSendersAndReconnectsAfterPlasmaRestarts() {
        Backend backend(&settings, &media, &system, false);
        QTest::qWait(30);
        auto rogue = QDBusConnection::connectToBus(QDBusConnection::SessionBus, "rogue-osd");
        progress(rogue, "audio-volume-high", 75);
        QTest::qWait(50);
        QCOMPARE(state(backend)["mode"].toString(), QString("idle"));
        text(plasma, "preferences-system-bluetooth", "Bluetooth On");
        QTRY_COMPARE(state(backend)["title"].toString(), QString("Bluetooth On"));
        QVERIFY(plasma.unregisterService(service));
        QTRY_COMPARE(state(backend)["mode"].toString(), QString("idle"));
        QVERIFY(rogue.registerService(service));
        QTest::qWait(30);
        progress(plasma, "audio-volume-high", 90);
        QTest::qWait(40);
        QCOMPARE(state(backend)["mode"].toString(), QString("idle"));
        progress(rogue, "video-display-brightness", 50);
        QTRY_COMPARE(state(backend)["title"].toString(), QString("Brightness · 50%"));
        rogue.unregisterService(service);
    }
    void demoDoesNotConsumeLiveDesktopChanges() {
        Backend backend(&settings, &media, &system, true);
        const auto original = state(backend);
        QTest::qWait(30);
        progress(plasma, "audio-volume-high", 75);
        text(plasma, "input-touchpad-off", "Touchpad Off");
        QTest::qWait(50);
        QCOMPARE(state(backend), original);
    }
    void permissionOutranksFeedbackWithoutLosingTheUnderlyingCatState() {
        Backend backend(&settings, &media, &system, false);
        QTest::qWait(30);
        progress(plasma, "audio-volume-medium", 60);
        QTRY_COMPARE(state(backend)["mode"].toString(), QString("feedback"));
        backend.ingest(normalize({{"hook_event_name", "PermissionRequest"},
                                  {"session_id", "permission"},
                                  {"cwd", "/work"}},
                                 "claude_code"));
        QCOMPARE(state(backend)["status"].toString(), QString("waiting_permission"));
        progress(plasma, "video-display-brightness", 80);
        QTest::qWait(40);
        QCOMPARE(state(backend)["status"].toString(), QString("waiting_permission"));
        QVERIFY(!state(backend).contains("progress"));
    }
    void localVolumeControlsReportOnlySuccessfulChanges() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QFile stateFile(directory.filePath("volume"));
        QVERIFY(stateFile.open(QIODevice::WriteOnly));
        stateFile.write("0.20");
        stateFile.close();
        QFile executable(directory.filePath("wpctl"));
        QVERIFY(executable.open(QIODevice::WriteOnly));
        executable.write(R"(#!/bin/sh
case "$1" in
get-volume) printf 'Volume: '; cat "$NEXIS_TEST_AUDIO_STATE" ;;
set-volume)
    [ "$3" = "0.13" ] && exit 1
    printf '%s' "$3" > "$NEXIS_TEST_AUDIO_STATE" ;;
set-mute) printf '0.42 [MUTED]' > "$NEXIS_TEST_AUDIO_STATE" ;;
esac
)");
        executable.close();
        QVERIFY(executable.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        const auto previousPath = qgetenv("PATH"),
                   previousState = qgetenv("NEXIS_TEST_AUDIO_STATE");
        const auto restore = qScopeGuard([&] {
            qputenv("PATH", previousPath);
            if (previousState.isNull())
                qunsetenv("NEXIS_TEST_AUDIO_STATE");
            else
                qputenv("NEXIS_TEST_AUDIO_STATE", previousState);
        });
        qputenv("PATH", directory.path().toUtf8() + ":" + previousPath);
        qputenv("NEXIS_TEST_AUDIO_STATE", stateFile.fileName().toUtf8());
        System localSystem;
        Backend backend(&settings, &media, &localSystem, false);
        QTRY_VERIFY(localSystem.state()["volumeAvailable"].toBool());
        QCOMPARE(state(backend)["mode"].toString(), QString("idle"));
        localSystem.setVolume(0.13);
        QTRY_COMPARE(localSystem.state()["audioError"].toString(), QString("Output change failed"));
        QCOMPARE(state(backend)["mode"].toString(), QString("idle"));
        QTest::qWait(50);
        localSystem.setVolume(0.42);
        QTRY_COMPARE(localSystem.state()["volume"].toDouble(), 0.42);
        QTRY_COMPARE_WITH_TIMEOUT(state(backend)["title"].toString(), QString("Volume · 42%"),
                                  1000);
        localSystem.toggleMute();
        QTRY_COMPARE(state(backend)["title"].toString(), QString("Audio Muted"));
    }
};
QTEST_GUILESS_MAIN(OsdTest)
#include "test_osd.moc"
