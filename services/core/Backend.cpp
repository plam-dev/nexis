#include "Backend.h"
#include <QCursor>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QScreen>
#include <QStandardPaths>
#include <QTimer>
#include <algorithm>
namespace Nexis {
Backend::Backend(Settings *s, Media *m, System *sys, bool demo, QObject *p)
    : QObject(p), m_notifications(!demo, this), m_settings(s), m_media(m), m_system(sys),
      m_demo(demo) {
    connect(m, &Media::changed, this, &Backend::changed);
    connect(sys, &System::changed, this, &Backend::changed);
    connect(&m_notifications, &Notifications::changed, this, &Backend::changed);
    connect(s, &Settings::changed, this, [this] {
        if (!m_settings->options()["history"].toBool()) {
            m_registry.clearHistory();
            m_jobHistory.clear();
        }
        emit changed();
    });
    connect(this, &Backend::changed, this, &Backend::StateChanged);
    auto *ttl = new QTimer(this);
    ttl->setInterval(30000);
    connect(ttl, &QTimer::timeout, this, [this] {
        m_registry.expire(QDateTime::currentMSecsSinceEpoch());
        emit changed();
    });
    ttl->start();
    if (auto gui = qobject_cast<QGuiApplication *>(QCoreApplication::instance())) {
        connect(gui, &QGuiApplication::screenAdded, this, &Backend::changed);
        connect(gui, &QGuiApplication::screenRemoved, this, &Backend::changed);
    }
    if (demo) {
        for (auto provider : {QString("claude_code"), QString("qoder_app")}) {
            auto base =
                QJsonObject{{"session_id", provider},
                            {"cwd", provider == "claude_code" ? "/demo/nexis" : "/demo/website"},
                            {"hook_event_name", "UserPromptSubmit"}};
            ingest(normalize(base, provider));
            base["hook_event_name"] =
                provider == "claude_code" ? "PermissionRequest" : "PreToolUse";
            base["tool_name"] = "Edit";
            ingest(normalize(base, provider));
        }
    }
}
void Backend::ingest(const QJsonObject &e) {
    if (e["provider"] == "local_job") {
        if (!m_jobs.accept(e))
            return;
        if (m_settings->options()["history"].toBool()) {
            m_jobHistory.prepend(
                QVariantMap{{"provider", "local_job"},
                            {"project", e["label"].toString()},
                            {"event", e["event"].toString()},
                            {"time", QDateTime::currentDateTime().toString("HH:mm")},
                            {"received_ms", QDateTime::currentMSecsSinceEpoch()}});
            if (m_jobHistory.size() > 100)
                m_jobHistory.removeLast();
        }
    } else {
        if (!m_registry.accept(e))
            return;
        m_lastProvider[e["provider"].toString()] = QDateTime::currentMSecsSinceEpoch();
        if (!m_settings->options()["history"].toBool())
            m_registry.clearHistory();
    }
    emit changed();
    QTimer::singleShot(4010, this, [this] { emit changed(); });
}
QVariantList Backend::history() const {
    auto out = m_registry.history() + m_jobHistory;
    std::sort(out.begin(), out.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap()["received_ms"].toLongLong() > b.toMap()["received_ms"].toLongLong();
    });
    return out;
}
void Backend::clearHistory() {
    m_registry.clearHistory();
    m_jobHistory.clear();
    emit changed();
}
void Backend::dismissNotification(uint id) {
    m_notifications.dismiss(id);
}
void Backend::openNotification(uint id) {
    m_notifications.open(id);
}
void Backend::expanded(bool value) {
    if (value && !m_expanded)
        m_frozen = primary().value("key").toString();
    m_expanded = value;
    if (!value)
        m_frozen.clear();
    m_media->setExpanded(value);
    emit changed();
}
void Backend::drag(bool active) {
    m_dragging = active;
    if (active) {
        m_dragStart = QCursor::pos();
        auto opts = m_settings->options();
        m_offsetStart = QPoint(opts["offsetX"].toInt(), opts["offsetY"].toInt());
    }
}
void Backend::moveDrag() {
    if (m_dragging) {
        auto p = m_offsetStart + QCursor::pos() - m_dragStart;
        m_settings->set("offsetX", p.x());
        m_settings->set("offsetY", p.y());
    }
}
void Backend::quit() {
    QCoreApplication::quit();
}
QStringList Backend::screens() const {
    QStringList out;
    if (qobject_cast<QGuiApplication *>(QCoreApplication::instance()))
        for (auto *s : QGuiApplication::screens())
            out << s->name();
    return out;
}
QVariantMap Backend::primary() const {
    auto opts = m_settings->options();
    auto list = sessions();
    const auto now = QDateTime::currentMSecsSinceEpoch();
    auto agent = [](QVariantMap s) {
        s["kind"] = "agents";
        s["title"] = s["project"];
        return s;
    };
    if (m_system->state()["critical"].toBool())
        return {{"kind", "system"},
                {"title", "Battery critically low"},
                {"status", "critical"},
                {"key", "system"}};
    const bool attentionAllowed =
        opts["pinned"].toString().isEmpty() || opts["attentionOverride"].toBool();
    if (attentionAllowed) {
        for (const auto &v : list) {
            auto s = v.toMap();
            if (s["status"] == "waiting_permission" && s["key"] == m_attention)
                return agent(s);
        }
        for (const auto &v : list) {
            auto s = v.toMap();
            if (s["status"] == "waiting_permission") {
                m_attention = s["key"].toString();
                return agent(s);
            }
        }
    }
    if (m_expanded && m_frozen == "idle")
        return {{"kind", "idle"}, {"title", "NEXIS"}, {"status", "Ready"}, {"key", "idle"}};
    if (m_expanded)
        for (const auto &v : jobs())
            if (v.toMap()["key"].toString() == m_frozen)
                return v.toMap();
    if (m_expanded && m_frozen == "media" && !m_media->current().isEmpty())
        return {{"kind", "media"},
                {"title", m_media->current()["title"]},
                {"status", m_media->current()["playing"].toBool() ? "Playing" : "Paused"},
                {"key", "media"}};
    if (m_expanded && !m_frozen.isEmpty())
        for (const auto &v : list)
            if (v.toMap()["key"].toString() == m_frozen)
                return agent(v.toMap());
    if (opts["pinned"] == "agents")
        for (const auto &v : list) {
            auto s = v.toMap();
            if (s["status"] != "session_ended")
                return agent(s);
        }
    if (opts["pinned"] == "media" && !m_media->current().isEmpty())
        return {{"kind", "media"},
                {"title", m_media->current()["title"]},
                {"status", m_media->current()["playing"].toBool() ? "Playing" : "Paused"},
                {"key", "media"}};
    for (const auto &v : jobs()) {
        auto job = v.toMap();
        if (job["status"] == "failed" && now - job["received_ms"].toLongLong() < 5000)
            return job;
        if (job["status"] == "completed" && now - job["received_ms"].toLongLong() < 5000)
            return job;
    }
    for (const auto &v : list) {
        auto s = v.toMap();
        if (s["status"] == "turn_complete" && now - s["received_ms"].toLongLong() < 5000)
            return agent(s);
    }
    for (const auto &v : list) {
        auto s = v.toMap();
        if (s["key"] == m_ongoing &&
            QStringList{"running", "tool_running", "waiting_permission"}.contains(
                s["status"].toString()))
            return agent(s);
    }
    for (const auto &v : list) {
        auto s = v.toMap();
        if (QStringList{"running", "tool_running", "waiting_permission"}.contains(
                s["status"].toString())) {
            m_ongoing = s["key"].toString();
            return agent(s);
        }
    }
    for (const auto &v : jobs())
        if (QStringList{"started", "progress"}.contains(v.toMap()["status"].toString()))
            return v.toMap();
    if (!m_media->current().isEmpty() && m_media->current()["playing"].toBool())
        return {{"kind", "media"},
                {"title", m_media->current()["title"]},
                {"status", "Playing"},
                {"key", "media"}};
    return {{"kind", "idle"}, {"title", "NEXIS"}, {"status", "Ready"}, {"key", "idle"}};
}
QVariantList Backend::health() const {
    QVariantList out;
    const auto now = QDateTime::currentMSecsSinceEpoch();
    for (const auto &p : QStringList{"claude_code", "qoder_app"}) {
        bool configured = false;
        QFile file(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) +
                   (p == "claude_code" ? "/.claude/settings.json" : "/.qoder/settings.json"));
        if (file.open(QIODevice::ReadOnly) && file.size() < 1024 * 1024) {
            auto hooks = QJsonDocument::fromJson(file.readAll()).object()["hooks"].toObject();
            int count = 0;
            for (auto it = hooks.begin(); it != hooks.end(); ++it)
                for (auto group : it.value().toArray())
                    for (auto hook : group.toObject()["hooks"].toArray())
                        if (hook.toObject()["command"].toString().endsWith(
                                " # nexis:v1:" +
                                (p == "claude_code" ? QString("claude") : QString("qoder"))))
                            ++count;
            configured = count >= 12;
        }
        QString status =
            !m_lastProvider.contains(p)
                ? (configured ? "Configured · awaiting event" : "Not configured")
                : (now - m_lastProvider.value(p) > 15 * 60000 ? "Degraded · no recent event"
                                                              : "Connected");
        out << QVariantMap{{"name", p == "claude_code" ? "Claude Code" : "Qoder"},
                           {"status", status},
                           {"detail", p == "claude_code" ? "Local hook events"
                                                         : "New desktop app · local hook events"}};
    }
    out << QVariantMap{{"name", "Media"},
                       {"status", m_media->players().isEmpty() ? "No MPRIS player" : "Connected"},
                       {"detail", "Playback through the selected player"}};
    out << QVariantMap{
        {"name", "Notifications"},
        {"status", m_notifications.status()},
        {"detail", "KDE watcher · Plasma keeps ownership and may show its own banners"}};
    out << QVariantMap{{"name", "Downloads"},
                       {"status", "Unavailable"},
                       {"detail", "No validated download bridge"}};
    return out;
}
QVariantMap Backend::presentationState() const {
    auto out = Nexis::presentation(sessions(), m_media->current(), m_system->state(),
                                   QDateTime::currentMSecsSinceEpoch());
    const auto notice = m_notifications.banner();
    if (!notice.isEmpty() && out["status"] != "critical" && out["status"] != "waiting_permission") {
        out["mode"] = "notification";
        out["status"] = "notification";
        out["title"] = notice["title"].toString().isEmpty() ? notice["appName"] : notice["title"];
        out["subtitle"] = notice["appName"];
        out["notificationId"] = notice["id"];
    }
    return out;
}
bool Backend::PanelAction(const QString &action, const QDBusVariant &value) {
    const auto v = value.variant();
    const auto type = v.metaType().id();
    const bool numeric = type == QMetaType::Double || type == QMetaType::Float ||
                         type == QMetaType::Int || type == QMetaType::UInt ||
                         type == QMetaType::LongLong || type == QMetaType::ULongLong;
    if (action == "playPause")
        m_media->control("PlayPause");
    else if (action == "next")
        m_media->control("Next");
    else if (action == "previous")
        m_media->control("Previous");
    else if (action == "selectPlayer" && v.metaType() == QMetaType::fromType<QString>())
        m_media->select(v.toString());
    else if (action == "seek" && numeric && qIsFinite(v.toDouble()) && v.toDouble() >= 0 &&
             v.toDouble() <= 1)
        m_media->seek(v.toDouble());
    else if (action == "volume" && numeric && qIsFinite(v.toDouble()) && v.toDouble() >= 0 &&
             v.toDouble() <= 1)
        m_system->setVolume(v.toDouble());
    else if (action == "mute")
        m_system->toggleMute();
    else if (action == "clearHistory")
        clearHistory();
    else if ((action == "dismissNotification" || action == "openNotification") && numeric &&
             qIsFinite(v.toDouble()) && v.toDouble() >= 1 && v.toDouble() <= 4294967295. &&
             v.toDouble() == double(v.toUInt())) {
        if (action == "dismissNotification")
            dismissNotification(v.toUInt());
        else
            openNotification(v.toUInt());
    } else if (action == "reducedMotion" && v.metaType() == QMetaType::fromType<bool>())
        m_settings->set("reducedMotion", v.toBool());
    else if (action == "expanded" && v.metaType() == QMetaType::fromType<bool>())
        expanded(v.toBool());
    else
        return false;
    return true;
}
QString Backend::Snapshot() const {
    return QString::fromUtf8(
        QJsonDocument(
            QJsonObject{{"sessions", QJsonArray::fromVariantList(sessions())},
                        {"history", QJsonArray::fromVariantList(history())},
                        {"jobs", QJsonArray::fromVariantList(jobs())},
                        {"notifications", QJsonArray::fromVariantList(notifications())},
                        {"primary", QJsonObject::fromVariantMap(primary())},
                        {"presentation", QJsonObject::fromVariantMap(presentationState())},
                        {"media", QJsonObject::fromVariantMap(m_media->current())},
                        {"players", QJsonArray::fromVariantList(m_media->players())},
                        {"system", QJsonObject::fromVariantMap(m_system->state())},
                        {"options", QJsonObject::fromVariantMap(m_settings->options())},
                        {"darkTheme", m_settings->colors()["dark"].toBool()},
                        {"health", QJsonArray::fromVariantList(health())},
                        {"demo", m_demo}})
            .toJson(QJsonDocument::Compact));
}
} // namespace Nexis
