#pragma once
#include <QObject>
#include <QTimer>
#include <QVariantMap>
namespace Nexis {
class DesktopClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap data READ data NOTIFY changed)
    Q_PROPERTY(bool connected READ connected NOTIFY changed)
    Q_PROPERTY(QString serviceName READ serviceName WRITE setServiceName NOTIFY serviceChanged)
  public:
    explicit DesktopClient(QObject *parent = nullptr);
    QVariantMap data() const { return m_data; }
    bool connected() const { return m_connected; }
    QString serviceName() const { return m_service; }
    void setServiceName(const QString &name);
    Q_INVOKABLE void action(const QString &name, const QVariant &value = QVariant());
  signals:
    void changed();
    void serviceChanged();
  private slots:
    void schedule();
    void refresh();
    void ownerChanged(const QString &, const QString &, const QString &);

  private:
    QVariantMap m_data;
    QString m_service = "org.nexis.Desktop";
    bool m_connected = false, m_pending = false, m_dirty = false;
    quint64 m_generation = 0;
    QTimer m_coalesce;
};
} // namespace Nexis
