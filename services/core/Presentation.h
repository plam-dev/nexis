#pragma once
#include <QVariantList>
#include <QVariantMap>
namespace Nexis {
QVariantMap presentation(const QVariantList &sessions, const QVariantMap &media,
                         const QVariantMap &system, qint64 now);
}
