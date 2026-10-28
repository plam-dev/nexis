#include "Notifications.h"
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QTextDocument>
namespace Nexis {
static const QString service = "org.freedesktop.Notifications";
static const QString path = "/org/freedesktop/Notifications";
static const QString manager = "org.kde.NotificationManager";
static QDBusMessage call(const QString &owner, const QString &method) {
    return QDBusMessage::createMethodCall(owner, path, manager, method);
}
static QString plainText(const QString &value, int limit) {
    // Notification markup is untrusted; never resolve image or other external resources.
    class PlainDocument : public QTextDocument {
        QVariant loadResource(int, const QUrl &) override { return {}; }
    } document;
    document.setHtml(value.left(8192));
    auto text = document.toPlainText();
    text.remove(QChar::ObjectReplacementCharacter);
    text.removeIf([](QChar c) {
        const auto u = c.unicode();
        return (c.category() == QChar::Other_Control && c != '\n' && c != '\t') ||
               (u >= 0x202a && u <= 0x202e) || (u >= 0x2066 && u <= 0x2069);
    });
    return text.trimmed().left(limit);
}
Notifications::Notifications(bool enabled, QObject *parent) : QObject(parent), m_enabled(enabled) {
    m_retryTimer.setSingleShot(true);
    connect(&m_retryTimer, &QTimer::timeout, this, [this] {
        if (m_registered)
            readInhibition();
        else
            registerWatcher();
    });
    m_bannerTimer.setSingleShot(true);
    m_bannerTimer.setTimerType(Qt::PreciseTimer);
    m_bannerTimer.setInterval(5000);
    connect(&m_bannerTimer, &QTimer::timeout, this, [this] {
        m_bannerId = 0;
        emit changed();
    });
    if (!m_enabled)
        return;
    auto bus = QDBusConnection::sessionBus();
    m_exported = bus.registerObject("/NotificationWatcher", "org.kde.NotificationWatcher", this,
                                    QDBusConnection::ExportScriptableSlots);
    if (!m_exported)
        return;
    bus.connect("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
                "NameOwnerChanged", this, SLOT(ownerChanged(QString, QString, QString)));
    bus.connect(service, path, "org.freedesktop.DBus.Properties", "PropertiesChanged", this,
                SLOT(propertiesChanged(QString, QVariantMap, QStringList, QDBusMessage)));
    bus.connect(service, path, service, "NotificationClosed", this,
                SLOT(notificationClosed(uint, uint, QDBusMessage)));
    const int generation = m_generation;
    auto *owner =
        new QDBusPendingCallWatcher(bus.interface()->asyncCall("GetNameOwner", service), this);
    connect(owner, &QDBusPendingCallWatcher::finished, this, [this, owner, generation] {
        QDBusPendingReply<QString> reply = *owner;
        if (generation == m_generation && !reply.isError())
            setOwner(reply.value());
        owner->deleteLater();
    });
}
Notifications::~Notifications() {
    m_retryTimer.stop();
    if (m_registered && !m_owner.isEmpty())
        QDBusConnection::sessionBus().asyncCall(call(m_owner, "UnRegisterWatcher"), 1000);
    if (m_exported)
        QDBusConnection::sessionBus().unregisterObject("/NotificationWatcher");
}
void Notifications::ownerChanged(const QString &name, const QString &, const QString &owner) {
    if (name == service)
        setOwner(owner);
}
void Notifications::setOwner(const QString &owner) {
    if (owner == m_owner)
        return;
    ++m_generation;
    ++m_inhibitionGeneration;
    m_retryTimer.stop();
    m_retryDelay = 1000;
    m_owner = owner;
    m_registered = m_inhibitionKnown = false;
    m_inhibited = true;
    m_list.clear();
    m_bannerId = 0;
    m_bannerTimer.stop();
    emit changed();
    registerWatcher();
}
void Notifications::retryLater() {
    if (m_enabled && m_exported && !m_owner.isEmpty() && !m_retryTimer.isActive()) {
        m_retryTimer.start(m_retryDelay);
        m_retryDelay = qMin(m_retryDelay * 2, 30000);
    }
}
void Notifications::registerWatcher() {
    if (!m_enabled || !m_exported || m_owner.isEmpty())
        return;
    const int generation = m_generation;
    auto *watch = new QDBusPendingCallWatcher(
        QDBusConnection::sessionBus().asyncCall(call(m_owner, "RegisterWatcher"), 1000), this);
    connect(watch, &QDBusPendingCallWatcher::finished, this, [this, watch, generation] {
        QDBusPendingReply<> reply = *watch;
        if (generation == m_generation) {
            m_registered = !reply.isError();
            if (m_registered) {
                m_retryDelay = 1000;
                readInhibition();
            } else if (reply.error().type() != QDBusError::AccessDenied)
                retryLater();
            emit changed();
        }
        watch->deleteLater();
    });
}
void Notifications::readInhibition() {
    if (!m_registered || m_owner.isEmpty())
        return;
    m_retryTimer.stop();
    const int generation = ++m_inhibitionGeneration;
    auto msg =
        QDBusMessage::createMethodCall(m_owner, path, "org.freedesktop.DBus.Properties", "Get");
    msg << service << QString("Inhibited");
    auto *watch =
        new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(msg, 1000), this);
    connect(watch, &QDBusPendingCallWatcher::finished, this, [this, watch, generation] {
        QDBusPendingReply<QDBusVariant> reply = *watch;
        if (generation == m_inhibitionGeneration) {
            m_inhibitionKnown = !reply.isError() &&
                                reply.value().variant().metaType() == QMetaType::fromType<bool>();
            m_inhibited = !m_inhibitionKnown || reply.value().variant().toBool();
            if (m_inhibitionKnown)
                m_retryDelay = 1000;
            else if (!reply.isError() || reply.error().type() != QDBusError::AccessDenied)
                retryLater();
            if (m_inhibited) {
                m_bannerId = 0;
                m_bannerTimer.stop();
            }
            emit changed();
        }
        watch->deleteLater();
    });
}
void Notifications::propertiesChanged(const QString &iface, const QVariantMap &values,
                                      const QStringList &invalidated, const QDBusMessage &message) {
    if (!m_registered || message.service() != m_owner || iface != service)
        return;
    if (invalidated.contains("Inhibited")) {
        m_inhibitionKnown = false;
        m_inhibited = true;
        m_bannerId = 0;
        m_bannerTimer.stop();
        readInhibition();
    } else if (values.contains("Inhibited")) {
        ++m_inhibitionGeneration;
        m_inhibitionKnown = values["Inhibited"].metaType() == QMetaType::fromType<bool>();
        m_inhibited = !m_inhibitionKnown || values["Inhibited"].toBool();
        if (m_inhibitionKnown) {
            m_retryTimer.stop();
            m_retryDelay = 1000;
        } else if (m_registered)
            retryLater();
        if (m_inhibited) {
            m_bannerId = 0;
            m_bannerTimer.stop();
        }
    }
    emit changed();
}
bool Notifications::trusted() const {
    return m_registered && calledFromDBus() && !m_owner.isEmpty() && message().service() == m_owner;
}
void Notifications::Notify(uint id, const QString &app, uint replacesId, const QString &,
                           const QString &title, const QString &body, const QStringList &actions,
                           const QVariantMap &, int) {
    if (!trusted() || !id)
        return;
    m_list.removeIf([id, replacesId](const QVariant &value) {
        const auto old = value.toMap()["id"].toUInt();
        return old == id || (replacesId && old == replacesId);
    });
    bool hasDefault = false;
    for (int i = 0; i + 1 < qMin(actions.size(), qsizetype(128)); i += 2)
        if (actions[i] == "default")
            hasDefault = true;
    m_list.prepend(QVariantMap{{"id", id},
                               {"appName", plainText(app, 100)},
                               {"title", plainText(title, 200)},
                               {"body", plainText(body, 2000)},
                               {"hasDefaultAction", hasDefault}});
    if (m_list.size() > 20)
        m_list.removeLast();
    if (m_inhibitionKnown && !m_inhibited) {
        m_bannerId = id;
        m_bannerTimer.start();
    }
    emit changed();
}
void Notifications::CloseNotification(uint id) {
    if (trusted())
        dismiss(id);
}
void Notifications::notificationClosed(uint id, uint, const QDBusMessage &message) {
    if (!m_owner.isEmpty() && message.service() == m_owner)
        dismiss(id);
}
void Notifications::dismiss(uint id) {
    const auto removed =
        m_list.removeIf([id](const QVariant &value) { return value.toMap()["id"].toUInt() == id; });
    if (m_bannerId == id) {
        m_bannerId = 0;
        m_bannerTimer.stop();
    }
    if (removed)
        emit changed();
}
void Notifications::open(uint id) {
    if (!m_registered)
        return;
    for (const auto &value : m_list) {
        const auto item = value.toMap();
        if (item["id"].toUInt() == id && item["hasDefaultAction"].toBool()) {
            auto msg = call(m_owner, "InvokeAction");
            msg << id << QString("default");
            QDBusConnection::sessionBus().asyncCall(msg, 1000);
            return;
        }
    }
}
QVariantMap Notifications::banner() const {
    if (m_bannerId && m_bannerTimer.isActive() && m_inhibitionKnown && !m_inhibited)
        for (const auto &value : m_list)
            if (value.toMap()["id"].toUInt() == m_bannerId)
                return value.toMap();
    return {};
}
QString Notifications::status() const {
    if (!m_enabled)
        return "Demo · live bridge disabled";
    if (!m_registered)
        return "Unavailable";
    if (!m_inhibitionKnown)
        return "Degraded · DND state unavailable";
    return m_inhibited ? "Connected · Do Not Disturb" : "Connected";
}
} // namespace Nexis
