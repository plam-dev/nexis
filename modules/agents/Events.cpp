#include "Events.h"
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QUuid>
#include <algorithm>
namespace Nexis {
static QString hash(const QString &s) {
    return QString::fromLatin1(
        QCryptographicHash::hash(s.toUtf8(), QCryptographicHash::Sha256).toHex());
}
static QString label(QString s) {
    s.replace('\\', '/');
    s = s.section('/', -1);
    s.replace(QRegularExpression("[^\\p{L}\\p{N} ._()-]"), "_");
    return s.left(64);
}
static const QHash<QString, QString> states = {
    {"SessionStart", "started"},    {"UserPromptSubmit", "running"},
    {"PreToolUse", "tool_running"}, {"PermissionRequest", "waiting_permission"},
    {"PostToolUse", "running"},     {"PostToolUseFailure", "running"},
    {"Stop", "turn_complete"},      {"SessionEnd", "session_ended"},
    {"PreCompact", "running"},      {"SubagentStart", ""},
    {"SubagentStop", ""},           {"Notification", ""}};
QJsonObject normalize(const QJsonObject &raw, const QString &provider) {
    if (provider != "claude_code" && provider != "qoder_app")
        return {};
    if (QJsonDocument(raw).toJson(QJsonDocument::Compact).size() > 64 * 1024)
        return {};
    const auto name = raw.value("hook_event_name").toString();
    if (!states.contains(name))
        return {};
    for (auto key :
         {"session_id", "cwd", "agent_id", "tool_use_id", "tool_name", "event_id", "timestamp"})
        if (raw.contains(key) && (!raw[key].isString() || raw[key].toString().size() > 4096))
            return {};
    auto cwd = raw.value("cwd").toString();
    if (cwd.contains(QChar(0)))
        return {};
    while (cwd.endsWith('/'))
        cwd.chop(1);
    const auto id = raw.value("session_id").toString();
    const auto now = QDateTime::currentDateTimeUtc();
    auto time = now;
    if (raw.contains("timestamp")) {
        time = QDateTime::fromString(raw["timestamp"].toString(), Qt::ISODateWithMs);
        if (!time.isValid() || time > now.addSecs(300))
            return {};
    }
    QString category = "unknown";
    const auto tool = raw["tool_name"].toString().toLower();
    if (tool == "edit" || tool == "write" || tool == "multiedit" || tool == "apply_patch")
        category = "edit";
    else if (tool == "bash" || tool == "shell" || tool == "run_command")
        category = "shell";
    else if (tool == "read" || tool == "glob" || tool == "grep" || tool == "search")
        category = "read";
    else if (tool == "agent" || tool == "task")
        category = "agent";
    else if (tool.startsWith("mcp__"))
        category = "integration";
    const auto uuid = QUuid(raw["event_id"].toString());
    QJsonObject out = {{"schema_version", 1},
                       {"event_id", uuid.isNull()
                                        ? QUuid::createUuid().toString(QUuid::WithoutBraces)
                                        : uuid.toString(QUuid::WithoutBraces)},
                       {"provider", provider},
                       {"source_event", name},
                       {"session_id", hash(id.isEmpty() ? "provisional:" + cwd : id)},
                       {"project_id", hash(cwd)},
                       {"project_label", cwd.isEmpty() ? "Unknown project" : label(cwd)},
                       {"confidence", id.isEmpty() ? "limited" : "observed"},
                       {"status", states.value(name)},
                       {"tool_kind", category},
                       {"occurred_at", time.toUTC().toString(Qt::ISODateWithMs)}};
    if (!raw["agent_id"].toString().isEmpty())
        out["subagent_id"] = hash(raw["agent_id"].toString());
    if (!raw["tool_use_id"].toString().isEmpty())
        out["tool_id"] = hash(raw["tool_use_id"].toString());
    if (!raw["tool_name"].toString().isEmpty() && raw["tool_input"].isObject())
        out["tool_fingerprint"] =
            hash(QString::fromUtf8(QJsonDocument(QJsonObject{{"tool_name", raw["tool_name"]},
                                                             {"tool_input", raw["tool_input"]}})
                                       .toJson(QJsonDocument::Compact)));
    return out;
}
bool Registry::accept(const QJsonObject &e) {
    static const QSet<QString> keys = {
        "schema_version", "event_id",      "provider",   "source_event",    "session_id",
        "project_id",     "project_label", "confidence", "status",          "tool_kind",
        "occurred_at",    "subagent_id",   "tool_id",    "tool_fingerprint"};
    if (QJsonDocument(e).toJson(QJsonDocument::Compact).size() > 16384 ||
        e["schema_version"].toInt() != 1)
        return false;
    for (auto it = e.begin(); it != e.end(); ++it)
        if (!keys.contains(it.key()) ||
            (it.key() != "schema_version" &&
             (!it.value().isString() || it.value().toString().size() > 256)))
            return false;
    const auto provider = e["provider"].toString(), name = e["source_event"].toString(),
               eid = e["event_id"].toString();
    if ((provider != "claude_code" && provider != "qoder_app") || !states.contains(name) ||
        QUuid(eid).isNull() || m_seen.contains(eid))
        return false;
    const QRegularExpression hex("^[a-f0-9]{64}$");
    for (auto field : {"session_id", "project_id"})
        if (!hex.match(e[field].toString()).hasMatch())
            return false;
    for (auto field : {"subagent_id", "tool_id", "tool_fingerprint"})
        if (e.contains(field) && !hex.match(e[field].toString()).hasMatch())
            return false;
    if (e["confidence"] != "observed" && e["confidence"] != "limited")
        return false;
    if (label(e["project_label"].toString()) != e["project_label"].toString() ||
        e["project_label"].toString().isEmpty())
        return false;
    if (!QStringList{"unknown", "edit", "shell", "read", "agent", "integration"}.contains(
            e["tool_kind"].toString()))
        return false;
    if (e["status"].toString() != states.value(name))
        return false;
    const auto time = QDateTime::fromString(e["occurred_at"].toString(), Qt::ISODateWithMs);
    const auto now = QDateTime::currentMSecsSinceEpoch();
    if (!time.isValid() || time.toMSecsSinceEpoch() > now + 300000 ||
        time.toMSecsSinceEpoch() < now - 24 * 3600000)
        return false;
    // Claude's cwd can change within a session; it is context, not session identity.
    const auto key = provider + ":" +
                     (provider == "claude_code" ? QString() : e["project_id"].toString() + ":") +
                     e["session_id"].toString();
    auto s = m_sessions.value(key);
    if (time.toMSecsSinceEpoch() < s.value("source_ms", 0).toLongLong())
        return false;
    if (s.value("status") == "session_ended" && name != "SessionStart" &&
        name != "UserPromptSubmit")
        return false;
    if (s.isEmpty() && m_sessions.size() >= 256) {
        auto oldest = m_sessions.begin();
        for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it)
            if (it.value()["received_ms"].toLongLong() < oldest.value()["received_ms"].toLongLong())
                oldest = it;
        m_agents.remove(oldest.key());
        m_tools.remove(oldest.key());
        m_permissions.remove(oldest.key());
        m_sessions.erase(oldest);
    }
    m_seen.insert(eid);
    m_seenOrder.append(eid);
    if (m_seenOrder.size() > 4096)
        m_seen.remove(m_seenOrder.takeFirst());
    s["key"] = key;
    s["provider"] = provider;
    s["project"] = e["project_label"].toString();
    s["confidence"] = e["confidence"].toString();
    s["lastEvent"] = name;
    s["source_ms"] = time.toMSecsSinceEpoch();
    s["received_ms"] = now;
    s["index"] = ++m_index;
    s["time"] = QDateTime::fromMSecsSinceEpoch(now).toString("HH:mm");
    const auto toolId = e.value("tool_id").toString("unknown");
    auto insertBounded = [&s](QSet<QString> &set, const QString &id) {
        if (set.contains(id))
            return;
        if (set.size() < 64)
            set.insert(id);
        else
            s["trackingLimited"] = true;
    };
    if (name == "SubagentStart") {
        if (e.contains("subagent_id"))
            insertBounded(m_agents[key], e["subagent_id"].toString());
        else
            s["unknownSubagent"] = true;
    } else if (name == "SubagentStop") {
        if (e.contains("subagent_id"))
            m_agents[key].remove(e["subagent_id"].toString());
    } else if (name == "PermissionRequest") {
        // Claude omits tool_use_id here; correlate without forwarding tool arguments.
        const auto permissionId = !e.contains("tool_id") && e.contains("tool_fingerprint")
                                      ? "call:" + e["tool_fingerprint"].toString()
                                      : toolId;
        insertBounded(m_permissions[key], permissionId);
        s["status"] = "waiting_permission";
    } else if (name == "PreToolUse" || name == "PostToolUse" || name == "PostToolUseFailure") {
        if (name == "PreToolUse") {
            insertBounded(m_tools[key], toolId);
            s["tool"] = e["tool_kind"].toString();
        } else {
            m_tools[key].remove(toolId);
            m_permissions[key].remove(toolId);
            if (e.contains("tool_fingerprint"))
                m_permissions[key].remove("call:" + e["tool_fingerprint"].toString());
        }
        s["status"] = !m_permissions[key].isEmpty() ? "waiting_permission"
                      : !m_tools[key].isEmpty()     ? "tool_running"
                                                    : "running";
        if (m_tools[key].isEmpty())
            s["tool"] = QString();
    } else if (!states.value(name).isEmpty()) {
        s["status"] = states.value(name);
        m_tools[key].clear();
        m_permissions[key].clear();
        s["tool"] = QString();
        if (name == "SessionEnd" || name == "SessionStart") {
            m_agents[key].clear();
            s["unknownSubagent"] = false;
            s["trackingLimited"] = false;
        }
    }
    if (s.value("status").toString().isEmpty())
        s["status"] = "unknown";
    if (s.value("trackingLimited").toBool())
        s["confidence"] = "limited";
    s["failed"] = name == "PostToolUseFailure";
    s["subagents"] = m_agents[key].size();
    s["activeTools"] = m_tools[key].size();
    if (name == "Stop")
        s["completed_ms"] = now;
    else if (name == "UserPromptSubmit" || name == "SessionStart" || name == "SessionEnd")
        s.remove("completed_ms");
    m_sessions[key] = s;
    QVariantMap h = {{"provider", provider}, {"project", s["project"]}, {"event", name},
                     {"time", s["time"]},    {"status", s["status"]},   {"received_ms", now}};
    m_history.prepend(h);
    if (m_history.size() > 300)
        m_history.removeLast();
    return true;
}
QVariantList Registry::sessions() const {
    QVariantList out;
    for (const auto &s : m_sessions)
        out << s;
    std::sort(out.begin(), out.end(), [](const QVariant &a, const QVariant &b) {
        auto aa = a.toMap(), bb = b.toMap();
        if ((aa["status"] == "waiting_permission") != (bb["status"] == "waiting_permission"))
            return aa["status"] == "waiting_permission";
        return aa["index"].toULongLong() > bb["index"].toULongLong();
    });
    return out;
}
void Registry::expire(qint64 now) {
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        auto &s = it.value();
        if (s["status"] != "session_ended" && s["status"] != "turn_complete" &&
            now - s["received_ms"].toLongLong() > 15 * 60000) {
            s["status"] = "stale";
            s["tool"] = QString();
            s["subagents"] = 0;
            s["activeTools"] = 0;
            m_agents.remove(it.key());
            m_tools.remove(it.key());
            m_permissions.remove(it.key());
        }
    }
}
} // namespace Nexis
