#include "services/core/Presentation.h"
#include <QtTest>
using namespace Nexis;
class PresentationTest : public QObject {
    Q_OBJECT
    QVariantMap session(QString status, QString provider = "qoder_app", qint64 received = 10000) {
        return {{"key", provider},
                {"provider", provider},
                {"project", "Private project"},
                {"status", status},
                {"received_ms", received}};
    }
  private slots:
    void pausedTrackKeepsItsArtworkContextAndAgentAnimation() {
        const QVariantMap media{{"service", "player"}, {"playing", false}, {"title", "Paused track"}};
        auto state = presentation({}, media, {}, 10000);
        QCOMPARE(state["mode"].toString(), QString("music"));
        QCOMPARE(state["status"].toString(), QString("paused"));
        QVERIFY(!state["listening"].toBool());
        state = presentation({session("running")}, media, {}, 10000);
        QCOMPARE(state["mode"].toString(), QString("music"));
        QVERIFY(state["working"].toBool());
    }
    void idlePetAndMusicOnly() {
        auto state = presentation({}, {}, {}, 10000);
        QCOMPARE(state["mode"].toString(), QString("idle"));
        QVERIFY(!state["working"].toBool());
        QVERIFY(!state["listening"].toBool());
        QCOMPARE(state["title"].toString(), QString());
        state = presentation({}, {{"playing", true}, {"title", "Some track"}}, {}, 10000);
        QCOMPARE(state["mode"].toString(), QString("music"));
        QCOMPARE(state["title"].toString(), QString("Some track"));
        QVERIFY(state["listening"].toBool());
        QVERIFY(!state["working"].toBool());
    }
    void workingAndMusicCoexist() {
        auto state = presentation({session("running")}, {}, {}, 10000);
        QCOMPARE(state["mode"].toString(), QString("idle"));
        QVERIFY(state["title"].toString().isEmpty());
        QVERIFY(state["working"].toBool());
        state = presentation({session("tool_running")}, {{"playing", true}, {"title", "Bambina"}},
                             {}, 10000);
        QCOMPARE(state["mode"].toString(), QString("music"));
        QVERIFY(state["working"].toBool());
        QVERIFY(state["listening"].toBool());
        QCOMPARE(state["title"].toString(), QString("Bambina"));
        QVERIFY(!state.values().contains("Private project"));
    }
    void missingUnknownOrStoppedMusicReturnsToClockWhileAgentWorks() {
        for (const auto &title : {QString(), QString("   "), QString("Unknown Music"),
                                  QString(" unknown TRACK ")}) {
            auto state = presentation({session("running")},
                                      {{"service", "player"}, {"playing", true}, {"title", title}},
                                      {}, 10000);
            QCOMPARE(state["mode"].toString(), QString("idle"));
            QVERIFY(state["title"].toString().isEmpty());
            QVERIFY(state["working"].toBool());
            QVERIFY(!state["listening"].toBool());
        }
        const auto stopped = presentation({session("running")},
            {{"service", "player"}, {"title", "Old track"}, {"playbackStatus", "Stopped"}}, {}, 10000);
        QCOMPARE(stopped["mode"].toString(), QString("idle"));
        QVERIFY(stopped["working"].toBool());
        const auto state =
            presentation({}, {{"playing", true}, {"title", QString(500, 'x')}}, {}, 10000);
        QCOMPARE(state["title"].toString().size(), 200);
    }
    void completionWithoutMusicReturnsToClockEvenWithAnotherWorkingAgent() {
        const QVariantList sessions{session("turn_complete"), session("running", "claude_code")};
        auto state = presentation(sessions, {}, {}, 13999);
        QCOMPARE(state["mode"].toString(), QString("completion"));
        QVERIFY(state["working"].toBool());
        state = presentation(sessions, {}, {}, 14000);
        QCOMPARE(state["mode"].toString(), QString("idle"));
        QVERIFY(state["working"].toBool());
    }
    void completionLastsFourSecondsThenReturnsToMusic() {
        auto s = session("turn_complete");
        auto state = presentation({s}, {{"playing", true}, {"title", "Track"}}, {}, 13999);
        QCOMPARE(state["mode"].toString(), QString("completion"));
        QVERIFY(state["title"].toString().contains("Qoder"));
        QVERIFY(!state["working"].toBool());
        QVERIFY(state["listening"].toBool());
        state = presentation({s}, {{"playing", true}, {"title", "Track"}}, {}, 14000);
        QCOMPARE(state["mode"].toString(), QString("music"));
        state = presentation({s}, {}, {}, 14000);
        QCOMPARE(state["mode"].toString(), QString("idle"));
    }
    void anotherAgentKeepsTypingAndPermissionDoesNotPretendToWork() {
        auto state = presentation({session("turn_complete"), session("running", "claude_code")},
                                  {{"playing", true}, {"title", "Track"}}, {}, 10000);
        QCOMPARE(state["mode"].toString(), QString("completion"));
        QVERIFY(state["working"].toBool());
        QVERIFY(state["listening"].toBool());
        state = presentation({session("waiting_permission")}, {{"playing", true}, {"title", "Track"}}, {}, 10000);
        QCOMPARE(state["mode"].toString(), QString("attention"));
        QVERIFY(!state["working"].toBool());
        QVERIFY(state["listening"].toBool());
        state = presentation({session("stale"), session("session_ended")}, {}, {}, 10000);
        QCOMPARE(state["mode"].toString(), QString("idle"));
    }
};
QTEST_GUILESS_MAIN(PresentationTest)
#include "test_presentation.moc"
