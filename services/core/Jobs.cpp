#include "Jobs.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QUuid>
#include <algorithm>
namespace Nexis {
bool Jobs::accept(const QJsonObject &e) {
    const QSet<QString> allowed = {"schema_version", "provider", "event_id", "job_id",
                                   "label",          "event",    "progress", "occurred_at"};
    if (e.size() != allowed.size() || e["schema_version"].toInt() != 1 ||
        e["provider"] != "local_job" || QJsonDocument(e).toJson().size() > 2048)
        return false;
    for (auto it = e.begin(); it != e.end(); ++it)
        if (!allowed.contains(it.key()))
            return false;
    const auto id = e["job_id"].toString(), name = e["label"].toString(),
               event = e["event"].toString(), eid = e["event_id"].toString();
    if (!QRegularExpression("^[a-zA-Z0-9_-]{1,64}$").match(id).hasMatch() ||
        !QRegularExpression("^[\\p{L}\\p{N} _().-]{1,64}$").match(name).hasMatch())
        return false;
    if (!QStringList{"started", "progress", "completed", "failed"}.contains(event) ||
        QUuid(eid).isNull() || m_seen.contains(eid))
        return false;
    const auto number = e["progress"].toDouble(-1);
    if (number < 0 || number > 100 || number != int(number))
        return false;
    const auto source = QDateTime::fromString(e["occurred_at"].toString(), Qt::ISODateWithMs);
    const auto now = QDateTime::currentMSecsSinceEpoch();
    if (!source.isValid() || source.toMSecsSinceEpoch() > now + 300000 ||
        source.toMSecsSinceEpoch() < now - 24 * 3600000 ||
        source.toMSecsSinceEpoch() < m_jobs.value(id).value("source_ms", 0).toLongLong())
        return false;
    if (m_jobs.contains(id) &&
        QStringList{"completed", "failed"}.contains(m_jobs[id]["status"].toString()) &&
        event != "started")
        return false;
    m_seen.insert(eid);
    m_order << eid;
    if (m_order.size() > 1024)
        m_seen.remove(m_order.takeFirst());
    if (!m_jobs.contains(id) && m_jobs.size() >= 32) {
        auto oldest = m_jobs.begin();
        for (auto it = m_jobs.begin(); it != m_jobs.end(); ++it)
            if (it.value()["received_ms"].toLongLong() < oldest.value()["received_ms"].toLongLong())
                oldest = it;
        m_jobs.erase(oldest);
    }
    m_jobs[id] = {{"key", "job:" + id},
                  {"kind", "jobs"},
                  {"title", name},
                  {"status", event},
                  {"progress", event == "completed" ? 100 : number},
                  {"received_ms", now},
                  {"source_ms", source.toMSecsSinceEpoch()}};
    return true;
}
QVariantList Jobs::list() const {
    QVariantList out;
    const auto now = QDateTime::currentMSecsSinceEpoch();
    for (auto s : m_jobs) {
        if (QStringList{"started", "progress"}.contains(s["status"].toString()) &&
            now - s["received_ms"].toLongLong() > 15 * 60000)
            s["status"] = "stale";
        out << s;
    }
    std::sort(out.begin(), out.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap()["received_ms"].toLongLong() > b.toMap()["received_ms"].toLongLong();
    });
    return out;
}
} // namespace Nexis
