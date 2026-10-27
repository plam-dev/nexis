#pragma once
#include "Jobs.h"
#include "Presentation.h"
#include "Settings.h"
#include "modules/agents/Events.h"
#include "modules/media/Media.h"
#include "modules/notifications/Notifications.h"
#include "modules/system/System.h"
#include <QDBusVariant>
#include <QObject>
#include <QPoint>
namespace Nexis {
class Backend : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.nexis.Desktop")
    Q_PROPERTY(QVariantList sessions READ sessions NOTIFY changed)
    Q_PROPERTY(QVariantList history READ history NOTIFY changed)
    Q_PROPERTY(QVariantList jobs READ jobs NOTIFY changed)
    Q_PROPERTY(QVariantList notifications READ notifications NOTIFY changed)
    Q_PROPERTY(QVariantMap primary READ primary NOTIFY changed)
    Q_PROPERTY(QVariantMap presentation READ presentationState NOTIFY changed)
    Q_PROPERTY(QVariantList health READ health NOTIFY changed)
    Q_PROPERTY(QStringList screens READ screens NOTIFY changed)
    Q_PROPERTY(bool demo READ demo CONSTANT)
  public:
    Backend(Settings *settings, Media *media, System *system, bool demo, QObject *parent = nullptr);
    QVariantList sessions() const { return m_registry.sessions(); }
    QVariantList history() const;
    QVariantList jobs() const { return m_jobs.list(); }
    QVariantList notifications() const { return m_notifications.list(); }
    QVariantMap primary() const;
    QVariantMap presentationState() const;
    QVariantList health() const;
    QStringList screens() const;
    bool demo() const { return m_demo; }
    void ingest(const QJsonObject &event);
    Q_INVOKABLE void clearHistory();
    Q_INVOKABLE void dismissNotification(uint id);
    Q_INVOKABLE void openNotification(uint id);
    Q_INVOKABLE bool action(const QString &name, const QVariant &value = QVariant()) {
        return PanelAction(name, QDBusVariant(value));
    }
    Q_INVOKABLE void expanded(bool value);
    Q_INVOKABLE void quit();
    Q_INVOKABLE void drag(bool active);
    Q_INVOKABLE void moveDrag();
  public slots:
    QString Snapshot() const;
    bool PanelAction(const QString &action, const QDBusVariant &value);
  signals:
    void changed();
    void StateChanged();

  private:
    QPoint m_dragStart, m_offsetStart;
    bool m_dragging = false;
    mutable QString m_ongoing, m_attention;
    Jobs m_jobs;
    QVariantList m_jobHistory;
    Registry m_registry;
    Notifications m_notifications;
    Settings *m_settings;
    Media *m_media;
    System *m_system;
    bool m_demo;
    bool m_expanded = false;
    QString m_frozen;
    QHash<QString, qint64> m_lastProvider;
};
} // namespace Nexis
