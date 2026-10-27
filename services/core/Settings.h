#pragma once
#include <QFileSystemWatcher>
#include <QObject>
#include <QSettings>
#include <QVariantMap>
namespace Nexis {
class Settings : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap options READ options NOTIFY changed)
    Q_PROPERTY(QVariantMap colors READ colors NOTIFY changed)
  public:
    explicit Settings(QObject *parent = nullptr);
    QVariantMap options() const;
    QVariantMap colors() const;
    Q_INVOKABLE void set(const QString &key, const QVariant &value);
    Q_INVOKABLE void resetPosition();
  signals:
    void changed();

  private:
    QSettings m_store;
    QFileSystemWatcher m_watcher;
};
} // namespace Nexis
