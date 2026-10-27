#include "apps/panel-client/DesktopClient.h"
#include <QDBusConnection>
#include <QDBusVariant>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>
using namespace Nexis;
class Source : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.nexis.Desktop")
  public:
    int requests = 0, actions = 0;
    QString title = "Music";
  public slots:
    QString Snapshot() {
        ++requests;
        return QString::fromUtf8(
            QJsonDocument(
                QJsonObject{{"presentation",
                             QJsonObject{{"title", title}, {"working", true}, {"listening", true}}},
                            {"sessions", QJsonArray()}})
                .toJson());
    }
    bool PanelAction(const QString &, const QDBusVariant &) {
        ++actions;
        return true;
    }
  signals:
    void StateChanged();
};
class ClientTest : public QObject {
    Q_OBJECT
  private slots:
    void initialSignalsRestartAndAllowlistedActions() {
        auto bus = QDBusConnection::sessionBus();
        Source source;
        QVERIFY(bus.registerService("org.nexis.Desktop"));
        QVERIFY(bus.registerObject("/org/nexis/Desktop", &source,
                                   QDBusConnection::ExportAllSlots |
                                       QDBusConnection::ExportAllSignals));
        DesktopClient client;
        QTRY_VERIFY(client.connected());
        QCOMPARE(client.data()["presentation"].toMap()["title"].toString(), QString("Music"));
        source.title = "Qoder · Done";
        emit source.StateChanged();
        QTRY_COMPARE(client.data()["presentation"].toMap()["title"].toString(), source.title);
        for (int i = 0; i < 100; i++)
            emit source.StateChanged();
        QTest::qWait(100);
        QVERIFY(source.requests < 10);
        client.action("grantPermission", true);
        QTest::qWait(20);
        QCOMPARE(source.actions, 0);
        client.action("playPause");
        QTRY_COMPARE(source.actions, 1);
        QVERIFY(bus.unregisterService("org.nexis.Desktop"));
        QTRY_VERIFY(!client.connected());
        QVERIFY(client.data().isEmpty());
        QVERIFY(bus.registerService("org.nexis.Desktop"));
        QTRY_VERIFY(client.connected());
        client.setServiceName("org.untrusted.Service");
        QCOMPARE(client.serviceName(), QString("org.nexis.Desktop"));
    }
};
QTEST_GUILESS_MAIN(ClientTest)
#include "test_panel_client.moc"
