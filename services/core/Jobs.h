#pragma once
#include <QJsonObject>
#include <QMap>
#include <QSet>
#include <QVariantList>
namespace Nexis {
class Jobs {
  public:
    bool accept(const QJsonObject &event);
    QVariantList list() const;

  private:
    QMap<QString, QVariantMap> m_jobs;
    QSet<QString> m_seen;
    QList<QString> m_order;
};
} // namespace Nexis
