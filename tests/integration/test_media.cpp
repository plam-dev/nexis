#include "modules/media/Media.h"
#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>
using namespace Nexis;
class Player : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")
    Q_PROPERTY(QString PlaybackStatus MEMBER status)
    Q_PROPERTY(QVariantMap Metadata MEMBER metadata)
    Q_PROPERTY(bool CanControl READ yes)
    Q_PROPERTY(bool CanPlay READ yes)
    Q_PROPERTY(bool CanPause READ yes)
    Q_PROPERTY(bool CanGoNext MEMBER canNext)
    Q_PROPERTY(bool CanGoPrevious READ yes)
    Q_PROPERTY(bool CanSeek READ yes)
    Q_PROPERTY(qlonglong Position MEMBER position)
  public:
    QDBusConnection bus;
    QString status;
    QVariantMap metadata;
    bool canNext = false;
    int controls = 0;
    qlonglong position = 0;
    Player(QString connection, QString title, QString state)
        : bus(QDBusConnection::connectToBus(QDBusConnection::SessionBus, connection)),
          status(state) {
        metadata = {{"xesam:title", title},
                    {"xesam:artist", QStringList{"Test artist"}},
                    {"mpris:length", qlonglong(120000000)},
                    {"mpris:trackid", QVariant::fromValue(QDBusObjectPath("/track/one"))}};
    }
    bool yes() const { return true; }
    void update() {
        auto signal = QDBusMessage::createSignal(
            "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", "PropertiesChanged");
        signal << QString("org.mpris.MediaPlayer2.Player")
               << QVariantMap{{"PlaybackStatus", status}, {"Metadata", metadata}} << QStringList();
        bus.send(signal);
    }
  public slots:
    void PlayPause() {
        ++controls;
        status = status == "Playing" ? "Paused" : "Playing";
        update();
    }
    void Next() { ++controls; }
    void Previous() { ++controls; }
    void SetPosition(const QDBusObjectPath &track, qlonglong p) {
        if (track.path() == "/track/one")
            position = p;
    }
};
class Root : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")
    Q_PROPERTY(QString Identity READ identity)
  public:
    Root(Player *p) : QDBusAbstractAdaptor(p) {}
    QString identity() const { return "NEXIS test player"; }
};
class MediaTest : public QObject {
    Q_OBJECT
  private slots:
    void discoveryCapsExportedPlayers() {
        Player player("bounded", "Bounded track", "Paused");
        Root root(&player);
        QVERIFY(player.bus.registerObject("/org/mpris/MediaPlayer2", &player,
                                          QDBusConnection::ExportAllProperties |
                                              QDBusConnection::ExportAllSlots |
                                              QDBusConnection::ExportAdaptors));
        QStringList names;
        for (int i = 0; i < 40; ++i) {
            const auto name = QString("org.mpris.MediaPlayer2.nexis_bound_%1").arg(i);
            QVERIFY(player.bus.registerService(name));
            names << name;
        }
        Media media;
        QTRY_COMPARE(media.players().size(), 32);
        for (const auto &name : names)
            QVERIFY(player.bus.unregisterService(name));
        QTRY_COMPARE(media.players().size(), 0);
    }
    void exportedMetadataIsBoundedAndAllowlisted() {
        const auto huge = QString(1100000, 'x');
        const auto snapshot =
            mediaSnapshot({{"DebugBlob", huge},
                           {"Metadata", QVariantMap{{"xesam:title", huge},
                                                    {"private:blob", huge},
                                                    {"xesam:artist", QStringList{huge}}}},
                           {"PlaybackStatus", "Playing"}});
        QVERIFY(!snapshot.contains("DebugBlob"));
        QVERIFY(!snapshot.contains("Metadata"));
        QCOMPARE(snapshot["title"].toString().size(), 200);
        QVERIFY(snapshot["playing"].toBool());
        QCOMPARE(snapshot["playbackStatus"].toString(), QString("Playing"));
        QVERIFY(QJsonDocument(QJsonObject::fromVariantMap(snapshot)).toJson().size() < 4096);
    }
    void discoverSelectControlAndReconnect() {
        Player first("first", "First track", "Playing"), second("second", "Second track", "Paused");
        Root root1(&first), root2(&second);
        for (auto *p : {&first, &second})
            QVERIFY(p->bus.registerObject("/org/mpris/MediaPlayer2", p,
                                          QDBusConnection::ExportAllProperties |
                                              QDBusConnection::ExportAllSlots |
                                              QDBusConnection::ExportAdaptors));
        QVERIFY(first.bus.registerService("org.mpris.MediaPlayer2.nexis_test_a"));
        QVERIFY(second.bus.registerService("org.mpris.MediaPlayer2.nexis_test_b"));
        Media media;
        QTRY_COMPARE(media.players().size(), 2);
        QTRY_COMPARE(media.current()["title"].toString(), QString("First track"));
        media.select("org.mpris.MediaPlayer2.nexis_test_b");
        QCOMPARE(media.current()["title"].toString(), QString("Second track"));
        media.control("PlayPause");
        QTRY_COMPARE(second.controls, 1);
        QCOMPARE(first.controls, 0);
        QTRY_VERIFY(media.current()["playing"].toBool());
        media.control("Next");
        QTest::qWait(30);
        QCOMPARE(second.controls, 1);
        media.seek(.5);
        QTRY_COMPARE(second.position, qlonglong(60000000));
        second.metadata["xesam:title"] = "Updated track";
        second.update();
        QTRY_COMPARE(media.current()["title"].toString(), QString("Updated track"));
        second.status = "Stopped";
        second.metadata.clear();
        second.update();
        QTRY_COMPARE(media.current()["playbackStatus"].toString(), QString("Stopped"));
        QCOMPARE(media.current()["title"].toString(), QString("Unknown track"));
        second.status = "Playing";
        second.metadata["xesam:title"] = "Updated track";
        second.update();
        QTRY_COMPARE(media.current()["title"].toString(), QString("Updated track"));
        QVERIFY(second.bus.unregisterService("org.mpris.MediaPlayer2.nexis_test_b"));
        QTRY_COMPARE(media.players().size(), 1);
        QCOMPARE(media.current()["title"].toString(), QString("First track"));
        QVERIFY(second.bus.registerService("org.mpris.MediaPlayer2.nexis_test_b"));
        QTRY_COMPARE(media.players().size(), 2);
        QTRY_COMPARE(media.current()["title"].toString(), QString("Updated track"));
    }
};
QTEST_GUILESS_MAIN(MediaTest)
#include "test_media.moc"
