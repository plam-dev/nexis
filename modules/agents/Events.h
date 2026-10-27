#pragma once
#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QSet>
#include <QVariantList>
namespace Nexis {
QJsonObject normalize(const QJsonObject &raw, const QString &provider);
class Registry {
  public:
    bool accept(const QJsonObject &event);
    QVariantList sessions() const;
    QVariantList history() const { return m_history; }
    void expire(qint64 now);
    void clearHistory() { m_history.clear(); }

  private:
    QHash<QString, QVariantMap> m_sessions;
    QHash<QString, QSet<QString>> m_agents, m_tools, m_permissions;
    QSet<QString> m_seen;
    QList<QString> m_seenOrder;
    QVariantList m_history;
    quint64 m_index = 0;
};
} // namespace Nexis
