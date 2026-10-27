#include "modules/agents/Events.h"
#include "modules/media/Media.h"
#include <QtTest>
using namespace Nexis;
class CoreTest : public QObject {
    Q_OBJECT
    QJsonObject event(QString name, QString id = "one", QString provider = "claude_code",
                      QString cwd = "/work/project") {
        return normalize({{"hook_event_name", name},
                          {"session_id", id},
                          {"cwd", cwd},
                          {"agent_id", "child-1"},
                          {"tool_name", "Edit"}},
                         provider);
    }
  private slots:
    void lifecycle() {
        const QList<QPair<QString, QString>> cases = {{"SessionStart", "started"},
                                                      {"UserPromptSubmit", "running"},
                                                      {"PreToolUse", "tool_running"},
                                                      {"PermissionRequest", "waiting_permission"},
                                                      {"Notification", "waiting_permission"},
                                                      {"PostToolUse", "running"},
                                                      {"PostToolUseFailure", "running"},
                                                      {"PreCompact", "running"},
                                                      {"Stop", "turn_complete"},
                                                      {"UserPromptSubmit", "running"},
                                                      {"SessionEnd", "session_ended"}};
        for (const auto &provider : QStringList{"claude_code", "qoder_app"}) {
            Registry r;
            for (const auto &c : cases) {
                QVERIFY(r.accept(event(c.first, "one", provider)));
                QCOMPARE(r.sessions().size(), 1);
                QCOMPARE(r.sessions()[0].toMap()["status"].toString(), c.second);
            }
        }
    }
    void namespaceAndSubagents() {
        Registry r;
        QVERIFY(r.accept(event("SubagentStart")));
        QCOMPARE(r.sessions()[0].toMap()["subagents"].toInt(), 1);
        QVERIFY(r.accept(event("SubagentStop")));
        QCOMPARE(r.sessions()[0].toMap()["subagents"].toInt(), 0);
        r.accept(event("SessionStart", "one", "qoder_app"));
        r.accept(event("SessionStart", "two"));
        r.accept(event("SessionStart", "one", "claude_code", "/other/project"));
        QCOMPARE(r.sessions().size(), 4);
    }
    void privacyAndValidation() {
        QJsonObject raw = {{"hook_event_name", "PreToolUse"},
                           {"session_id", "secret-session"},
                           {"cwd", "/home/private/project"},
                           {"prompt", "TOPSECRET"},
                           {"tool_name", "Bash"},
                           {"tool_input", QJsonObject{{"command", "echo TOPSECRET"}}},
                           {"tool_response", "TOPSECRET"}};
        auto n = normalize(raw, "claude_code");
        QVERIFY(!n.isEmpty());
        auto json = QJsonDocument(n).toJson();
        QVERIFY(!json.contains("TOPSECRET"));
        QVERIFY(!json.contains("/home/private"));
        QVERIFY(!json.contains("secret-session"));
        QCOMPARE(n["tool_kind"].toString(), QString("shell"));
        QVERIFY(normalize(raw, "untrusted").isEmpty());
        raw["hook_event_name"] = "MadeUp";
        QVERIFY(normalize(raw, "qoder_app").isEmpty());
        raw["hook_event_name"] = "Stop";
        raw["session_id"] = QString(20000, 'x');
        QVERIFY(normalize(raw, "claude_code").isEmpty());
    }
    void duplicateOldAndStale() {
        Registry r;
        auto n = event("UserPromptSubmit");
        QVERIFY(r.accept(n));
        QVERIFY(!r.accept(n));
        auto old = event("Stop");
        old["occurred_at"] = "2000-01-01T00:00:00Z";
        QVERIFY(!r.accept(old));
        r.expire(QDateTime::currentMSecsSinceEpoch() + 16 * 60 * 1000);
        QCOMPARE(r.sessions()[0].toMap()["status"].toString(), QString("stale"));
        auto absent = normalize({{"hook_event_name", "Stop"}}, "qoder_app");
        QCOMPARE(absent["confidence"].toString(), QString("limited"));
    }
    void unrelatedToolDoesNotClearPermission() {
        Registry r;
        auto send = [&](QString eventName, QString tool) {
            return r.accept(normalize({{"hook_event_name", eventName},
                                       {"session_id", "one"},
                                       {"cwd", "/work/project"},
                                       {"tool_use_id", tool},
                                       {"tool_name", "Edit"}},
                                      "qoder_app"));
        };
        QVERIFY(send("PreToolUse", "A"));
        QVERIFY(send("PermissionRequest", "B"));
        QVERIFY(send("PostToolUse", "A"));
        QCOMPARE(r.sessions()[0].toMap()["status"].toString(), QString("waiting_permission"));
        QVERIFY(send("PostToolUse", "B"));
        QCOMPARE(r.sessions()[0].toMap()["status"].toString(), QString("running"));
    }
    void inFlightCollectionsAreBoundedAndReconciled() {
        Registry r;
        for (int i = 0; i < 100; ++i) {
            auto n = normalize({{"hook_event_name", "SubagentStart"},
                                {"session_id", "one"},
                                {"cwd", "/work/project"},
                                {"agent_id", QString::number(i)}},
                               "claude_code");
            r.accept(n);
        }
        QVERIFY(r.sessions()[0].toMap()["subagents"].toInt() <= 64);
        QCOMPARE(r.sessions()[0].toMap()["confidence"].toString(), QString("limited"));
        for (int i = 0; i < 100; ++i)
            r.accept(normalize({{"hook_event_name", "PreToolUse"},
                                {"session_id", "one"},
                                {"cwd", "/work/project"},
                                {"tool_use_id", QString::number(i)}},
                               "claude_code"));
        QVERIFY(r.sessions()[0].toMap()["activeTools"].toInt() <= 64);
        r.expire(QDateTime::currentMSecsSinceEpoch() + 16 * 60000);
        QCOMPARE(r.sessions()[0].toMap()["subagents"].toInt(), 0);
        QCOMPARE(r.sessions()[0].toMap()["activeTools"].toInt(), 0);
    }
    void mediaCapabilities() {
        QVariantMap p = {{"PlaybackStatus", "Playing"},
                         {"CanControl", true},
                         {"CanPause", true},
                         {"CanPlay", false},
                         {"CanGoNext", false},
                         {"Metadata", QVariantMap{{"xesam:title", "Track"},
                                                  {"xesam:artist", QStringList{"Artist"}},
                                                  {"mpris:length", qint64(120000000)}}}};
        auto m = mediaSnapshot(p);
        QCOMPARE(m["title"].toString(), QString("Track"));
        QCOMPARE(m["artist"].toString(), QString("Artist"));
        QVERIFY(mediaCan(p, "PlayPause"));
        QVERIFY(!mediaCan(p, "Next"));
        QVERIFY(!mediaCan(p, "SetPosition"));
        QVERIFY(!mediaCan(p, "OpenUri"));
        QMap<QString, QVariantMap> players = {{"a", {{"PlaybackStatus", "Paused"}}}, {"b", p}};
        QCOMPARE(choosePlayer(players, "", "a"), QString("b"));
        QCOMPARE(choosePlayer(players, "a", "b"), QString("a"));
    }
};
QTEST_GUILESS_MAIN(CoreTest)
#include "test_core.moc"
