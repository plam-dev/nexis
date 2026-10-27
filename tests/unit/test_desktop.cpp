#include "services/core/Backend.h"
#include "services/core/Compositor.h"
#include "services/core/Jobs.h"
#include <QColor>
#include <QDBusConnection>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>
#include <limits>
using namespace Nexis;
class FakeScripting : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kwin.Scripting")
  public:
    bool loaded = true;
    int unloads = 0;
  public slots:
    bool unloadScript(const QString &) {
        loaded = false;
        ++unloads;
        return true;
    }
    int loadScript(const QString &, const QString &) {
        if (loaded)
            return -1;
        loaded = true;
        return 1;
    }
};
class FakeScript : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kwin.Script")
  public:
    int runs = 0;
  public slots:
    void run() { ++runs; }
};
class DesktopTest : public QObject {
    Q_OBJECT
  private slots:
    void notchHeightIsPersistedAndBounded() {
        QSettings store("nexis", "Desktop");
        store.remove("notchHeight");
        store.sync();
        Settings settings;
        QCOMPARE(settings.options()["notchHeight"].toInt(), 72);
        settings.set("notchHeight", 96);
        Settings reloaded;
        QCOMPARE(reloaded.options()["notchHeight"].toInt(), 96);
        settings.set("notchHeight", 0);
        QCOMPARE(settings.options()["notchHeight"].toInt(), 48);
        settings.set("notchHeight", 1000);
        QCOMPARE(settings.options()["notchHeight"].toInt(), 120);
        store.setValue("notchHeight", "999");
        store.sync();
        QCOMPARE(settings.options()["notchHeight"].toInt(), 120);
        settings.set("notchHeight", 72);
    }
    void persistedOptionsKeepTheirTypesInSnapshots() {
        Settings settings;
        settings.set("reducedMotion", QString("false"));
        settings.set("hoverExpand", QString("false"));
        settings.set("offsetX", QString("6"));
        Media media;
        System system;
        Backend backend(&settings, &media, &system, false);
        const auto options =
            QJsonDocument::fromJson(backend.Snapshot().toUtf8()).object()["options"].toObject();
        QVERIFY(options["reducedMotion"].isBool());
        QVERIFY(!options["reducedMotion"].toBool());
        QVERIFY(options["hoverExpand"].isBool());
        QVERIFY(!options["hoverExpand"].toBool());
        QVERIFY(options["offsetX"].isDouble());
        QCOMPARE(options["offsetX"].toInt(), 6);
        settings.set("reducedMotion", true);
        QVERIFY(settings.options()["reducedMotion"].toBool());
        settings.set("reducedMotion", false);
        settings.set("offsetX", 0);
    }
    void panelActionsRejectNonNumericValues() {
        Settings settings;
        Media media;
        System system;
        Backend backend(&settings, &media, &system, false);
        for (const auto &action : {QString("volume"), QString("seek")}) {
            for (const auto &value :
                 {QVariant("not-a-number"), QVariant("0.5"), QVariant(true), QVariant(-0.1),
                  QVariant(1.1), QVariant(std::numeric_limits<double>::infinity())})
                QVERIFY(!backend.PanelAction(action, QDBusVariant(value)));
            QVERIFY(backend.PanelAction("seek", QDBusVariant(0.5)));
        }
    }
    void jobsAreBoundedValidatedAndReplaySafe() {
        Jobs jobs;
        auto event = QJsonObject{
            {"schema_version", 1},
            {"provider", "local_job"},
            {"event_id", QUuid::createUuid().toString(QUuid::WithoutBraces)},
            {"job_id", "build"},
            {"label", "Project build"},
            {"event", "started"},
            {"progress", 0},
            {"occurred_at", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
        QVERIFY(jobs.accept(event));
        QVERIFY(!jobs.accept(event));
        QCOMPARE(jobs.list().size(), 1);
        event["event_id"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
        event["event"] = "progress";
        event["progress"] = 45;
        QVERIFY(jobs.accept(event));
        QCOMPARE(jobs.list()[0].toMap()["progress"].toInt(), 45);
        event["event_id"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
        event["progress"] = 101;
        QVERIFY(!jobs.accept(event));
        event["progress"] = 50;
        event["command"] = "secret";
        QVERIFY(!jobs.accept(event));
        event.remove("command");
        event["label"] = "/private/file";
        QVERIFY(!jobs.accept(event));
    }
    void changingAppearancePreservesHistory() {
        Settings settings;
        settings.set("history", true);
        Media media;
        System system;
        Backend backend(&settings, &media, &system, false);
        backend.ingest(
            {{"schema_version", 1},
             {"provider", "local_job"},
             {"event_id", QUuid::createUuid().toString(QUuid::WithoutBraces)},
             {"job_id", "build"},
             {"label", "Project build"},
             {"event", "started"},
             {"progress", 0},
             {"occurred_at", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}});
        QCOMPARE(backend.history().size(), 1);
        settings.set("theme", "light");
        QCOMPARE(backend.history().size(), 1);
        settings.set("history", false);
        QCOMPARE(backend.history().size(), 0);
        settings.set("theme", "auto");
        settings.set("history", true);
    }
    void desktopRgbListsActuallyAffectTheTheme() {
        const auto directory =
            QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
        if (!directory.contains("test-config"))
            QSKIP("Only write isolated test configuration");
        QDir().mkpath(directory);
        {
            QSettings kde(directory + "/kdeglobals", QSettings::IniFormat);
            kde.setValue("Colors:Window/BackgroundNormal", QStringList{"18", "30", "42"});
            kde.sync();
        }
        Settings settings;
        settings.set("theme", "auto");
        QCOMPARE(settings.colors()["background"].value<QColor>(), QColor("#121e2a"));
        QFile::remove(directory + "/kdeglobals");
    }
    void orphanKwinHelperIsReplaced() {
        auto bus = QDBusConnection::sessionBus();
        FakeScripting scripting;
        FakeScript script;
        QVERIFY(bus.registerService("org.kde.KWin"));
        QVERIFY(bus.registerObject("/Scripting", &scripting, QDBusConnection::ExportAllSlots));
        QVERIFY(bus.registerObject("/Scripting/Script1", &script, QDBusConnection::ExportAllSlots));
        QTemporaryDir directory;
        const auto source = directory.path() + "/observer.js";
        QFile file(source);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("// __NEXIS_SERVICE__");
        file.close();
        {
            Compositor c;
            c.start(source, directory.path(), "org.nexis.Test");
            QTRY_COMPARE(script.runs, 1);
            QVERIFY(scripting.unloads >= 1);
        }
        bus.unregisterObject("/Scripting");
        bus.unregisterObject("/Scripting/Script1");
        bus.unregisterService("org.kde.KWin");
    }
    void expandingIdleKeepsContextDuringOrdinaryWork() {
        Settings settings;
        settings.set("pinned", "");
        Media media;
        System system;
        Backend backend(&settings, &media, &system, false);
        backend.expanded(true);
        backend.ingest(normalize({{"hook_event_name", "UserPromptSubmit"},
                                  {"session_id", "one"},
                                  {"cwd", "/work/project"}},
                                 "claude_code"));
        QCOMPARE(backend.primary()["kind"].toString(), QString("idle"));
        backend.expanded(false);
        QCOMPARE(backend.primary()["kind"].toString(), QString("agents"));
    }
    void fullscreenIsPerDisplayAndRejectsMalformedReports() {
        Compositor c;
        c.setScreen("HDMI-A-1");
        c.Report("[\"eDP-1\"]");
        QVERIFY(!c.fullscreen());
        c.Report("[\"HDMI-A-1\"]");
        QVERIFY(c.fullscreen());
        c.Report("{\"secret\":true}");
        QVERIFY(c.fullscreen());
        c.Report("[]");
        QVERIFY(!c.fullscreen());
    }
    void priorityPinningAndPermissionPersistence() {
        Settings settings;
        settings.set("pinned", "");
        settings.set("attentionOverride", true);
        Media media;
        System system;
        Backend backend(&settings, &media, &system, false);
        auto raw = QJsonObject{{"hook_event_name", "PermissionRequest"},
                               {"session_id", "one"},
                               {"cwd", "/demo/project"}};
        backend.ingest(normalize(raw, "qoder_app"));
        QCOMPARE(backend.primary()["status"].toString(), QString("waiting_permission"));
        raw["hook_event_name"] = "Notification";
        backend.ingest(normalize(raw, "qoder_app"));
        QCOMPARE(backend.primary()["status"].toString(), QString("waiting_permission"));
        raw["hook_event_name"] = "Stop";
        backend.ingest(normalize(raw, "qoder_app"));
        QCOMPARE(backend.primary()["status"].toString(), QString("turn_complete"));
    }
};
QTEST_GUILESS_MAIN(DesktopTest)
#include "test_desktop.moc"
