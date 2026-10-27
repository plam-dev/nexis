#include "DesktopClient.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
namespace Nexis {
static const QString path = "/org/nexis/Desktop", iface = "org.nexis.Desktop";
DesktopClient::DesktopClient(QObject *p) : QObject(p) {
    m_coalesce.setSingleShot(true);
    m_coalesce.setInterval(25);
    connect(&m_coalesce, &QTimer::timeout, this, &DesktopClient::refresh);
    auto bus = QDBusConnection::sessionBus();
    bus.connect("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
                "NameOwnerChanged", this, SLOT(ownerChanged(QString, QString, QString)));
    bus.connect(m_service, path, iface, "StateChanged", this, SLOT(schedule()));
    schedule();
}
void DesktopClient::setServiceName(const QString &name) {
    if (name == m_service || (name != "org.nexis.Desktop" && name != "org.nexis.Desktop.Demo"))
        return;
    auto bus = QDBusConnection::sessionBus();
    bus.disconnect(m_service, path, iface, "StateChanged", this, SLOT(schedule()));
    ++m_generation;
    m_pending = false;
    m_dirty = false;
    m_service = name;
    m_data.clear();
    m_connected = false;
    bus.connect(m_service, path, iface, "StateChanged", this, SLOT(schedule()));
    emit serviceChanged();
    emit changed();
    schedule();
}
void DesktopClient::schedule() {
    if (!m_coalesce.isActive())
        m_coalesce.start();
}
void DesktopClient::refresh() {
    if (m_pending) {
        m_dirty = true;
        return;
    }
    m_pending = true;
    const auto generation = m_generation;
    auto message = QDBusMessage::createMethodCall(m_service, path, iface, "Snapshot");
    auto *watch =
        new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message, 1000), this);
    connect(watch, &QDBusPendingCallWatcher::finished, this, [this, watch, generation] {
        QDBusPendingReply<QString> reply = *watch;
        watch->deleteLater();
        if (generation != m_generation)
            return;
        m_pending = false;
        auto document = reply.isError() || reply.value().size() > 1024 * 1024
                            ? QJsonDocument()
                            : QJsonDocument::fromJson(reply.value().toUtf8());
        m_connected = document.isObject();
        m_data = m_connected ? document.object().toVariantMap() : QVariantMap();
        emit changed();
        if (m_dirty) {
            m_dirty = false;
            schedule();
        }
    });
}
void DesktopClient::ownerChanged(const QString &name, const QString &, const QString &owner) {
    if (name != m_service)
        return;
    ++m_generation;
    m_pending = false;
    m_dirty = false;
    m_connected = false;
    m_data.clear();
    emit changed();
    if (!owner.isEmpty())
        schedule();
}
void DesktopClient::action(const QString &name, const QVariant &value) {
    static const QSet<QString> allowed = {"playPause",
                                          "next",
                                          "previous",
                                          "selectPlayer",
                                          "seek",
                                          "volume",
                                          "mute",
                                          "clearHistory",
                                          "reducedMotion",
                                          "expanded",
                                          "dismissNotification",
                                          "openNotification"};
    if (!m_connected || !allowed.contains(name))
        return;
    auto message = QDBusMessage::createMethodCall(m_service, path, iface, "PanelAction");
    message << name << QVariant::fromValue(QDBusVariant(value.isValid() ? value : QVariant(false)));
    QDBusConnection::sessionBus().asyncCall(message, 1000);
}
} // namespace Nexis
