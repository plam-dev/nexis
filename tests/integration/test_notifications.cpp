#include "services/core/Backend.h"
#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusError>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QJsonArray>
#include <QJsonDocument>
#include <QtTest>
using namespace Nexis;

static const QString service = "org.freedesktop.Notifications";
static const QString path = "/org/freedesktop/Notifications";
class Daemon : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.NotificationManager")
  public:
    explicit Daemon(const QString &connection = "daemon")
        : bus(QDBusConnection::connectToBus(QDBusConnection::SessionBus, connection)) {}
    QDBusConnection bus;
    QString watcher;
    int registrations = 0, unregistrations = 0, invoked = 0;
    uint actionId = 0;
    QString action;
    bool inhibited = false, rejectRegistration = false, registrationNotReady = false;
    void notify(uint id, QString title = "Build finished", QString body = "<b>Ready</b>",
                QStringList actions = {}, uint replaces = 0, QString app = "Builder") {
        auto msg = QDBusMessage::createMethodCall(watcher, "/NotificationWatcher",
                                                  "org.kde.NotificationWatcher", "Notify");
        msg.setArguments({id, app, replaces, QString(), title, body, actions, QVariantMap(), -1});
        bus.asyncCall(msg, 1000);
    }
    void close(uint id) {
        auto msg = QDBusMessage::createMethodCall(
            watcher, "/NotificationWatcher", "org.kde.NotificationWatcher", "CloseNotification");
        msg << id;
        bus.asyncCall(msg, 1000);
    }
    void setInhibited(bool value) {
        inhibited = value;
        auto msg = QDBusMessage::createSignal(path, "org.freedesktop.DBus.Properties",
                                              "PropertiesChanged");
        msg << service << QVariantMap{{"Inhibited", value}} << QStringList();
        bus.send(msg);
    }
  public slots:
    void RegisterWatcher() {
        watcher = message().service();
        ++registrations;
        if (rejectRegistration)
            sendErrorReply(QDBusError::AccessDenied, "Watcher denied");
        else if (registrationNotReady)
            sendErrorReply(QDBusError::UnknownObject, "Notification manager is starting");
    }
    void UnRegisterWatcher() { ++unregistrations; }
    void InvokeAction(uint id, const QString &key) {
        ++invoked;
        actionId = id;
        action = key;
    }
};
class NotificationProperties : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")
    Q_PROPERTY(bool Inhibited READ inhibited)
  public:
    explicit NotificationProperties(Daemon *daemon)
        : QDBusAbstractAdaptor(daemon), m_daemon(daemon) {}
    bool inhibited() const { return m_daemon->inhibited; }

  private:
    Daemon *m_daemon;
};

class NotificationTest : public QObject {
    Q_OBJECT
    Daemon daemon;
    NotificationProperties properties{&daemon};
    Settings settings;
    Media media;
    System system;
    QJsonObject snapshot(const Backend &backend) {
        return QJsonDocument::fromJson(backend.Snapshot().toUtf8()).object();
    }
    QJsonArray notifications(const Backend &backend) {
        return snapshot(backend)["notifications"].toArray();
    }
    QString notificationHealth(const Backend &backend) {
        for (const auto &value : snapshot(backend)["health"].toArray())
            if (value.toObject()["name"] == "Notifications")
                return value.toObject()["status"].toString();
        return {};
    }
  private slots:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(daemon.bus.registerObject(
            path, &daemon, QDBusConnection::ExportAllSlots | QDBusConnection::ExportAdaptors));
    }
    void init() {
        daemon.registrations = daemon.unregistrations = daemon.invoked = 0;
        daemon.watcher.clear();
        daemon.inhibited = false;
        daemon.rejectRegistration = false;
        daemon.registrationNotReady = false;
        QVERIFY(daemon.bus.registerService(service));
    }
    void cleanup() {
        daemon.bus.unregisterService(service);
        QTest::qWait(20);
    }
    void watcherExportsPlainTextUpdatesClosesAndActions() {
        Backend backend(&settings, &media, &system, false);
        QTRY_COMPARE(daemon.registrations, 1);
        QTRY_COMPARE(notificationHealth(backend), QString("Connected"));
        daemon.notify(7, "<b>Build finished</b>",
                      "<b>Ready</b> &amp; safe<img src='https://example.org/a'>",
                      {"default", "Open"});
        QTRY_COMPARE(notifications(backend).size(), 1);
        auto notice = notifications(backend).first().toObject();
        QCOMPARE(notice["id"].toInt(), 7);
        QCOMPARE(notice["title"].toString(), QString("Build finished"));
        QVERIFY(notice["body"].toString().startsWith("Ready & safe"));
        QVERIFY(!notice["body"].toString().contains("https://"));
        QCOMPARE(notice["appName"].toString(), QString("Builder"));
        QVERIFY(notice["hasDefaultAction"].toBool());
        QTRY_COMPARE(snapshot(backend)["presentation"].toObject()["mode"].toString(),
                     QString("notification"));
        QVERIFY(backend.PanelAction("openNotification", QDBusVariant(7)));
        QTRY_COMPARE(daemon.invoked, 1);
        QCOMPARE(daemon.actionId, uint(7));
        QCOMPARE(daemon.action, QString("default"));
        daemon.notify(7, "Updated", "Progress", {}, 7);
        QTRY_COMPARE(notifications(backend).first().toObject()["title"].toString(),
                     QString("Updated"));
        QCOMPARE(notifications(backend).size(), 1);
        QVERIFY(backend.PanelAction("openNotification", QDBusVariant(7)));
        QTest::qWait(30);
        QCOMPARE(daemon.invoked, 1);
        daemon.close(7);
        QTRY_VERIFY(notifications(backend).isEmpty());
        daemon.notify(8);
        QTRY_COMPARE(notifications(backend).size(), 1);
        auto closed = QDBusMessage::createSignal(path, service, "NotificationClosed");
        closed << uint(8) << uint(1);
        daemon.bus.send(closed);
        QTRY_VERIFY(notifications(backend).isEmpty());
    }
    void rejectsForgedCallsAndBoundsMemory() {
        Backend backend(&settings, &media, &system, false);
        QTRY_COMPARE(daemon.registrations, 1);
        QTRY_COMPARE(notificationHealth(backend), QString("Connected"));
        Daemon rogue("rogue");
        rogue.watcher = daemon.watcher;
        rogue.notify(1, "Forged");
        QTest::qWait(40);
        QVERIFY(notifications(backend).isEmpty());
        daemon.notify(1, QString(100000, 't'), QString(100000, 'b'), {}, 0, QString(100000, 'a'));
        QTRY_COMPARE(notifications(backend).size(), 1);
        auto notice = notifications(backend).first().toObject();
        QVERIFY(notice["title"].toString().size() <= 200);
        QVERIFY(notice["body"].toString().size() <= 2000);
        QVERIFY(notice["appName"].toString().size() <= 100);
        rogue.close(1);
        QTest::qWait(40);
        QCOMPARE(notifications(backend).size(), 1);
        for (uint id = 2; id <= 25; ++id)
            daemon.notify(id);
        QTRY_COMPARE(notifications(backend).first().toObject()["id"].toInt(), 25);
        QCOMPARE(notifications(backend).size(), 20);
        QVERIFY(backend.PanelAction("dismissNotification", QDBusVariant(25)));
        QCOMPARE(notifications(backend).size(), 19);
        QVERIFY(!backend.PanelAction("dismissNotification", QDBusVariant("24")));
        QVERIFY(!backend.PanelAction("dismissNotification", QDBusVariant(-1)));
        QVERIFY(!backend.PanelAction("openNotification", QDBusVariant(1.5)));
        QVERIFY(!backend.PanelAction("openNotification", QDBusVariant(true)));
        QCOMPARE(daemon.invoked, 0);
    }
    void observingTheSameOwnerTwiceDoesNotRegisterAgain() {
        Backend backend(&settings, &media, &system, false);
        QTRY_COMPARE(notificationHealth(backend), QString("Connected"));
        const auto registrations = daemon.registrations;
        auto *watcher = backend.findChild<Notifications *>();
        QVERIFY(watcher);
        // Startup discovery and NameOwnerChanged can observe the same daemon.
        QVERIFY(QMetaObject::invokeMethod(watcher, "ownerChanged",
                                          Q_ARG(QString, service), Q_ARG(QString, QString()),
                                          Q_ARG(QString, daemon.bus.baseService())));
        QTest::qWait(80);
        QCOMPARE(daemon.registrations, registrations);
        daemon.notify(42, "Still connected");
        QTRY_COMPARE(notifications(backend).size(), 1);
        QCOMPARE(notifications(backend).first().toObject()["id"].toInt(), 42);
    }
    void bannerExpiresButExpandedContentRemainsAndPermissionWins() {
        Backend backend(&settings, &media, &system, false);
        QTRY_COMPARE(daemon.registrations, 1);
        QTRY_COMPARE(notificationHealth(backend), QString("Connected"));
        backend.ingest(normalize({{"hook_event_name", "UserPromptSubmit"},
                                  {"session_id", "notice"},
                                  {"cwd", "/demo/work"}},
                                 "claude_code"));
        daemon.notify(2);
        QTRY_COMPARE(snapshot(backend)["presentation"].toObject()["mode"].toString(),
                     QString("notification"));
        QVERIFY(snapshot(backend)["presentation"].toObject()["working"].toBool());
        backend.expanded(true);
        QTRY_COMPARE_WITH_TIMEOUT(snapshot(backend)["presentation"].toObject()["mode"].toString(),
                                  QString("idle"), 6500);
        QVERIFY(snapshot(backend)["presentation"].toObject()["working"].toBool());
        QCOMPARE(notifications(backend).size(), 1);
        daemon.notify(3);
        QTRY_COMPARE(notifications(backend).size(), 2);
        backend.ingest(normalize({{"hook_event_name", "PermissionRequest"},
                                  {"session_id", "notice"},
                                  {"cwd", "/demo/work"}},
                                 "claude_code"));
        QCOMPARE(snapshot(backend)["presentation"].toObject()["status"].toString(),
                 QString("waiting_permission"));
    }
    void respectsDndAndDoesNotReplaySuppressedBanners() {
        daemon.inhibited = true;
        Backend backend(&settings, &media, &system, false);
        QTRY_COMPARE(daemon.registrations, 1);
        QTRY_COMPARE(notificationHealth(backend), QString("Connected · Do Not Disturb"));
        daemon.notify(3);
        QTRY_COMPARE(notifications(backend).size(), 1);
        QVERIFY(snapshot(backend)["presentation"].toObject()["mode"] != "notification");
        daemon.setInhibited(false);
        QTRY_COMPARE(notificationHealth(backend), QString("Connected"));
        QVERIFY(snapshot(backend)["presentation"].toObject()["mode"] != "notification");
        daemon.notify(4);
        QTRY_COMPARE(snapshot(backend)["presentation"].toObject()["mode"].toString(),
                     QString("notification"));
        daemon.setInhibited(true);
        QTRY_VERIFY(snapshot(backend)["presentation"].toObject()["mode"] != "notification");
        QCOMPARE(notifications(backend).size(), 2);
    }
    void reconnectsAndRejectsPreviousDaemon() {
        Backend backend(&settings, &media, &system, false);
        QTRY_COMPARE(daemon.registrations, 1);
        QTRY_COMPARE(notificationHealth(backend), QString("Connected"));
        daemon.notify(1);
        QTRY_COMPARE(notifications(backend).size(), 1);
        QVERIFY(daemon.bus.unregisterService(service));
        QTRY_VERIFY(notifications(backend).isEmpty());
        Daemon replacement("replacement");
        NotificationProperties replacementProperties(&replacement);
        QVERIFY(replacement.bus.registerObject(
            path, &replacement, QDBusConnection::ExportAllSlots | QDBusConnection::ExportAdaptors));
        QVERIFY(replacement.bus.registerService(service));
        QTRY_COMPARE(replacement.registrations, 1);
        QTRY_COMPARE(notificationHealth(backend), QString("Connected"));
        daemon.notify(10, "Stale sender");
        QTest::qWait(30);
        QVERIFY(notifications(backend).isEmpty());
        replacement.notify(2, "New daemon");
        QTRY_COMPARE(notifications(backend).size(), 1);
        QCOMPARE(notifications(backend).first().toObject()["title"].toString(),
                 QString("New daemon"));
        replacement.bus.unregisterService(service);
        replacement.bus.unregisterObject(path);
    }
    void demoNeverRegistersOrConsumesLiveNotifications() {
        Backend backend(&settings, &media, &system, true);
        QTest::qWait(80);
        QCOMPARE(daemon.registrations, 0);
        QVERIFY(notifications(backend).isEmpty());
        auto msg = QDBusMessage::createMethodCall(QDBusConnection::sessionBus().baseService(),
                                                  "/NotificationWatcher",
                                                  "org.kde.NotificationWatcher", "Notify");
        msg.setArguments({uint(1), QString("Live"), uint(0), QString(), QString("Private"),
                          QString("Body"), QStringList(), QVariantMap(), -1});
        daemon.bus.asyncCall(msg, 500);
        QTest::qWait(30);
        QVERIFY(notifications(backend).isEmpty());
    }
    void delayedObjectExportRecoversWithoutOwnerChange() {
        daemon.bus.unregisterObject(path);
        Backend backend(&settings, &media, &system, false);
        // Let the first RegisterWatcher reach a daemon that has not exported its object yet.
        QTest::qWait(150);
        QCOMPARE(daemon.registrations, 0);
        QCOMPARE(notificationHealth(backend), QString("Unavailable"));
        QVERIFY(daemon.bus.registerObject(
            path, &daemon, QDBusConnection::ExportAllSlots | QDBusConnection::ExportAdaptors));
        daemon.setInhibited(false);
        QTRY_COMPARE_WITH_TIMEOUT(notificationHealth(backend), QString("Connected"), 2500);
        QCOMPARE(daemon.registrations, 1);
        daemon.notify(5);
        QTRY_COMPARE(notifications(backend).size(), 1);
    }
    void unavailableInhibitionPropertyRecoversWithoutReRegistering() {
        daemon.bus.unregisterObject(path);
        QVERIFY(daemon.bus.registerObject(path, &daemon, QDBusConnection::ExportAllSlots));
        Backend backend(&settings, &media, &system, false);
        QTRY_COMPARE(notificationHealth(backend), QString("Degraded · DND state unavailable"));
        daemon.notify(1);
        QTRY_COMPARE(notifications(backend).size(), 1);
        QVERIFY(snapshot(backend)["presentation"].toObject()["mode"] != "notification");
        daemon.bus.unregisterObject(path);
        QVERIFY(daemon.bus.registerObject(
            path, &daemon, QDBusConnection::ExportAllSlots | QDBusConnection::ExportAdaptors));
        QTRY_COMPARE_WITH_TIMEOUT(notificationHealth(backend), QString("Connected"), 2500);
        QCOMPARE(daemon.registrations, 1);
        QVERIFY(snapshot(backend)["presentation"].toObject()["mode"] != "notification");
    }
    void retriesBackOffAndStopOnShutdownOrOwnerChange() {
        daemon.registrationNotReady = true;
        {
            Backend backend(&settings, &media, &system, false);
            QTRY_COMPARE(daemon.registrations, 1);
            QTRY_COMPARE_WITH_TIMEOUT(daemon.registrations, 2, 1500);
            QTest::qWait(1100);
            QCOMPARE(daemon.registrations, 2);
        }
        QTest::qWait(1100);
        QCOMPARE(daemon.registrations, 2);
        Backend backend(&settings, &media, &system, false);
        QTRY_COMPARE(daemon.registrations, 3);
        QVERIFY(daemon.bus.unregisterService(service));
        Daemon replacement("retry-replacement");
        NotificationProperties replacementProperties(&replacement);
        QVERIFY(replacement.bus.registerObject(
            path, &replacement, QDBusConnection::ExportAllSlots | QDBusConnection::ExportAdaptors));
        QVERIFY(replacement.bus.registerService(service));
        QTRY_COMPARE(notificationHealth(backend), QString("Connected"));
        QTest::qWait(1200);
        QCOMPARE(replacement.registrations, 1);
        QCOMPARE(daemon.registrations, 3);
        replacement.bus.unregisterService(service);
        replacement.bus.unregisterObject(path);
    }
    void unknownDndFailsClosedAndRegistrationErrorsReportUnavailable() {
        daemon.bus.unregisterObject(path);
        QVERIFY(daemon.bus.registerObject(path, &daemon, QDBusConnection::ExportAllSlots));
        {
            Backend backend(&settings, &media, &system, false);
            QTRY_COMPARE(daemon.registrations, 1);
            QTRY_COMPARE(notificationHealth(backend), QString("Degraded · DND state unavailable"));
            daemon.notify(1);
            QTRY_COMPARE(notifications(backend).size(), 1);
            QVERIFY(snapshot(backend)["presentation"].toObject()["mode"] != "notification");
        }
        QTRY_COMPARE(daemon.unregistrations, 1);
        daemon.bus.unregisterObject(path);
        QVERIFY(daemon.bus.registerObject(
            path, &daemon, QDBusConnection::ExportAllSlots | QDBusConnection::ExportAdaptors));
        daemon.rejectRegistration = true;
        Backend backend(&settings, &media, &system, false);
        QTRY_COMPARE(daemon.registrations, 2);
        QTest::qWait(20);
        QCOMPARE(notificationHealth(backend), QString("Unavailable"));
        daemon.notify(2);
        QTest::qWait(1200);
        QVERIFY(notifications(backend).isEmpty());
        QCOMPARE(daemon.registrations, 2);
    }
};
QTEST_GUILESS_MAIN(NotificationTest)
#include "test_notifications.moc"
