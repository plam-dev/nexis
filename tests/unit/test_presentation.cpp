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
        QCOMPARE(state["title"].toString(), QString("Working"));
        QVERIFY(state["working"].toBool());
        state = presentation({session("tool_running")}, {{"playing", true}, {"title", "Bambina"}},
                             {}, 10000);
        QCOMPARE(state["mode"].toString(), QString("music"));
        QVERIFY(state["working"].toBool());
        QVERIFY(state["listening"].toBool());
        QCOMPARE(state["title"].toString(), QString("Bambina"));
        QVERIFY(!state.values().contains("Private project"));
    }
    void musicTitleIsBoundedWithAnEmptyFallback() {
        for (const auto &title : {QString(), QString("   ")}) {
            const auto state = presentation({}, {{"playing", true}, {"title", title}}, {}, 10000);
            QCOMPARE(state["title"].toString(), QString("Music"));
        }
        const auto state =
            presentation({}, {{"playing", true}, {"title", QString(500, 'x')}}, {}, 10000);
        QCOMPARE(state["title"].toString().size(), 200);
    }
    void completionLastsFourSecondsThenReturnsToMusic() {
        auto s = session("turn_complete");
        auto state = presentation({s}, {{"playing", true}}, {}, 13999);
        QCOMPARE(state["mode"].toString(), QString("completion"));
        QVERIFY(state["title"].toString().contains("Qoder"));
        QVERIFY(!state["working"].toBool());
        QVERIFY(state["listening"].toBool());
        state = presentation({s}, {{"playing", true}}, {}, 14000);
        QCOMPARE(state["mode"].toString(), QString("music"));
        state = presentation({s}, {}, {}, 14000);
        QCOMPARE(state["mode"].toString(), QString("idle"));
    }
    void anotherAgentKeepsTypingAndPermissionDoesNotPretendToWork() {
        auto state = presentation({session("turn_complete"), session("running", "claude_code")},
                                  {{"playing", true}}, {}, 10000);
        QCOMPARE(state["mode"].toString(), QString("completion"));
        QVERIFY(state["working"].toBool());
        QVERIFY(state["listening"].toBool());
        state = presentation({session("waiting_permission")}, {{"playing", true}}, {}, 10000);
        QCOMPARE(state["mode"].toString(), QString("attention"));
        QVERIFY(!state["working"].toBool());
        QVERIFY(state["listening"].toBool());
        state = presentation({session("stale"), session("session_ended")}, {}, {}, 10000);
        QCOMPARE(state["mode"].toString(), QString("idle"));
    }
};
QTEST_GUILESS_MAIN(PresentationTest)
#include "test_presentation.moc"
