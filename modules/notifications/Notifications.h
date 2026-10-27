#pragma once
#include <QDBusContext>
#include <QDBusMessage>
#include <QObject>
#include <QTimer>
#include <QVariantMap>
namespace Nexis {
class Notifications : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.NotificationWatcher")
  public:
    explicit Notifications(bool enabled, QObject *parent = nullptr);
    ~Notifications() override;
    QVariantList list() const { return m_list; }
    QVariantMap banner() const;
    QString status() const;
    void dismiss(uint id);
    void open(uint id);
  public slots:
    Q_SCRIPTABLE void Notify(uint id, const QString &app, uint replacesId, const QString &icon,
                             const QString &title, const QString &body, const QStringList &actions,
                             const QVariantMap &hints, int timeout);
    Q_SCRIPTABLE void CloseNotification(uint id);
  signals:
    void changed();
  private slots:
    void ownerChanged(const QString &, const QString &, const QString &);
    void propertiesChanged(const QString &, const QVariantMap &, const QStringList &,
                           const QDBusMessage &);
    void notificationClosed(uint id, uint reason, const QDBusMessage &message);

  private:
    void setOwner(const QString &owner);
    void registerWatcher();
    void readInhibition();
    void retryLater();
    bool trusted() const;
    QVariantList m_list;
    QString m_owner;
    QTimer m_bannerTimer, m_retryTimer;
    uint m_bannerId = 0;
    int m_generation = 0, m_inhibitionGeneration = 0;
    int m_retryDelay = 1000;
    bool m_enabled, m_exported = false, m_registered = false;
    bool m_inhibited = true, m_inhibitionKnown = false;
};
} // namespace Nexis
