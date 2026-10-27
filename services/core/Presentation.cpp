#include "Presentation.h"
namespace Nexis {
QVariantMap presentation(const QVariantList &sessions, const QVariantMap &media,
                         const QVariantMap &system, qint64 now) {
    int working = 0;
    QVariantMap permission, complete;
    for (const auto &value : sessions) {
        const auto s = value.toMap();
        const auto status = s.value("status").toString();
        if (status == "running" || status == "tool_running")
            ++working;
        if (status == "waiting_permission" && permission.isEmpty())
            permission = s;
        const auto completed = s.value("completed_ms", s.value("received_ms")).toLongLong();
        if (status == "turn_complete" && completed <= now && now - completed < 4000 &&
            (complete.isEmpty() ||
             completed >
                 complete.value("completed_ms", complete.value("received_ms")).toLongLong()))
            complete = s;
    }
    const bool listening = media.value("playing").toBool();
    QVariantMap out = {{"mode", "idle"},          {"title", ""},
                       {"working", working > 0},  {"listening", listening},
                       {"workingCount", working}, {"status", "idle"}};
    auto agentTitle = [](const QVariantMap &s) {
        return s.value("provider") == "claude_code" ? QString("Claude Code") : QString("Qoder");
    };
    if (system.value("critical").toBool()) {
        out["mode"] = "attention";
        out["title"] = "Battery critically low";
        out["status"] = "critical";
    } else if (!permission.isEmpty()) {
        out["mode"] = "attention";
        out["title"] = agentTitle(permission) + " · Permission";
        out["status"] = "waiting_permission";
    } else if (!complete.isEmpty()) {
        out["mode"] = "completion";
        out["title"] = agentTitle(complete) + " · Done";
        out["status"] = "turn_complete";
    } else if (listening) {
        out["mode"] = "music";
        const auto title = media.value("title").toString().trimmed().left(200);
        out["title"] = title.isEmpty() ? QString("Music") : title;
        out["status"] = "playing";
    } else if (working) {
        out["mode"] = "working";
        out["title"] = "Working";
        out["status"] = "running";
    }
    return out;
}
} // namespace Nexis
