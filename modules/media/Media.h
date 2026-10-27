#pragma once
#include <QDBusMessage>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <QVariantMap>
namespace Nexis {
QVariantMap mediaSnapshot(const QVariantMap &properties);
bool mediaCan(const QVariantMap &player, const QString &action);
QString choosePlayer(const QMap<QString, QVariantMap> &players, const QString &pinned,
                     const QString &last);
class Media : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList players READ players NOTIFY changed)
    Q_PROPERTY(QVariantMap current READ current NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
  public:
    explicit Media(QObject *parent = nullptr);
    QVariantList players() const;
    QVariantMap current() const;
    QString error() const { return m_error; }
    Q_INVOKABLE void select(const QString &service);
    Q_INVOKABLE void control(const QString &action);
    Q_INVOKABLE void seek(double fraction);
    void setExpanded(bool expanded);
  signals:
    void changed();
  private slots:
    void discover();
    void ownerChanged(const QString &, const QString &, const QString &);
    void propertiesChanged(const QString &, const QVariantMap &, const QStringList &,
                           const QDBusMessage &);
    void seeked(qlonglong, const QDBusMessage &);

  private:
    void refresh(const QString &service);
    void refreshPosition();
    QMap<QString, QVariantMap> m_players;
    QString m_pinned, m_last, m_error;
    QSet<QString> m_subscribed, m_ownerPending;
    QHash<QString, int> m_generation;
    QHash<QString, QString> m_owners;
    QTimer m_position;
};
} // namespace Nexis
