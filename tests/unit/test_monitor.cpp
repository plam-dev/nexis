#include "modules/system/Monitor.h"
#include <QtTest>
using namespace Nexis;
class MonitorTest : public QObject {
    Q_OBJECT
  private slots:
    void sampleReportsUtilizationStorageAndTopFiveApps() {
        Monitor monitor(true);
        QSignalSpy spy(&monitor, &Monitor::changed);
        monitor.setActive(true);
        QTest::qWait(2300);
        QVERIFY(spy.count() >= 2);
        const auto s = monitor.state();
        QVERIFY(s["available"].toBool());
        QVERIFY(s["cpu"].toDouble() >= 0 && s["cpu"].toDouble() <= 100);
        const auto mem = s["memory"].toMap();
        QVERIFY(mem["total"].toDouble() > 0 && mem["used"].toDouble() <= mem["total"].toDouble());
        QVERIFY(!s["storage"].toList().isEmpty());
        const auto apps = s["apps"].toList();
        QVERIFY(apps.size() >= 1 && apps.size() <= 5);
        QVERIFY(!apps.first().toMap()["name"].toString().isEmpty());
        monitor.setActive(false);
        const auto count = spy.count();
        QTest::qWait(2300);
        QCOMPARE(spy.count(), count);
    }
    void demoModeNeverSamplesTheHost() {
        Monitor monitor(false);
        QSignalSpy spy(&monitor, &Monitor::changed);
        monitor.setActive(true);
        QTest::qWait(2300);
        QCOMPARE(spy.count(), 0);
        QVERIFY(monitor.state()["available"].toBool());
    }
};
QTEST_MAIN(MonitorTest)
#include "test_monitor.moc"
