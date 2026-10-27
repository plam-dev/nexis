#include "Compositor.h"
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QTimer>
namespace Nexis {
void Compositor::start(const QString &source, const QString &runtime, const QString &service) {
    m_service = service;
    m_name = service + ".fullscreen";
    m_script = runtime + "/fullscreen.js";
    QFile file(source);
    if (!file.open(QIODevice::ReadOnly))
        return;
    auto js = file.readAll();
    js.replace("__NEXIS_SERVICE__", service.toUtf8());
    QSaveFile script(m_script);
    if (!script.open(QIODevice::WriteOnly) || script.write(js) != js.size() || !script.commit())
        return;
    QFile::setPermissions(m_script, QFile::ReadOwner | QFile::WriteOwner);
    auto bus = QDBusConnection::sessionBus();
    bus.registerObject("/org/nexis/Compositor", this,
                       QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllProperties);
    auto *watch = new QDBusServiceWatcher("org.kde.KWin", bus,
                                          QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(watch, &QDBusServiceWatcher::serviceOwnerChanged, this,
            [this](const QString &, const QString &, const QString &owner) {
                m_loaded = false;
                m_owner = owner;
                m_screens.clear();
                m_health = "Degraded";
                emit changed();
                if (!owner.isEmpty())
                    load();
            });
    load();
}
void Compositor::load() {
    const auto generation = ++m_generation;
    auto bus = QDBusConnection::sessionBus();
    auto *owner = new QDBusPendingCallWatcher(
        bus.interface()->asyncCall("GetNameOwner", QString("org.kde.KWin")), this);
    connect(owner, &QDBusPendingCallWatcher::finished, this, [this, owner, generation] {
        QDBusPendingReply<QString> reply = *owner;
        owner->deleteLater();
        if (reply.isError() || generation != m_generation)
            return;
        m_owner = reply.value();
        // Reclaim our runtime observer after a crash or systemd stop bypassed destruction.
        auto remove = QDBusMessage::createMethodCall("org.kde.KWin", "/Scripting",
                                                     "org.kde.kwin.Scripting", "unloadScript");
        remove << m_name;
        auto *unload = new QDBusPendingCallWatcher(
            QDBusConnection::sessionBus().asyncCall(remove, 1000), this);
        connect(unload, &QDBusPendingCallWatcher::finished, this, [this, unload, generation] {
            unload->deleteLater();
            if (generation != m_generation)
                return;
            auto message = QDBusMessage::createMethodCall("org.kde.KWin", "/Scripting",
                                                          "org.kde.kwin.Scripting", "loadScript");
            message << m_script << m_name;
            auto *watch = new QDBusPendingCallWatcher(
                QDBusConnection::sessionBus().asyncCall(message, 1000), this);
            connect(watch, &QDBusPendingCallWatcher::finished, this, [this, watch, generation] {
                QDBusPendingReply<int> result = *watch;
                watch->deleteLater();
                if (generation != m_generation)
                    return;
                if (result.isError() || result.value() < 0) {
                    m_health = "Degraded";
                    emit changed();
                    return;
                }
                m_loaded = true;
                auto run = QDBusMessage::createMethodCall(
                    "org.kde.KWin", "/Scripting/Script" + QString::number(result.value()),
                    "org.kde.kwin.Script", "run");
                QDBusConnection::sessionBus().asyncCall(run, 1000);
                QTimer::singleShot(1500, this, [this, generation] {
                    if (generation == m_generation && m_health != "Connected") {
                        m_health = "Degraded";
                        emit changed();
                    }
                });
            });
        });
    });
}
Compositor::~Compositor() {
    if (m_loaded) {
        auto msg = QDBusMessage::createMethodCall("org.kde.KWin", "/Scripting",
                                                  "org.kde.kwin.Scripting", "unloadScript");
        msg << m_name;
        QDBusConnection::sessionBus().call(msg, QDBus::Block, 500);
    }
}
void Compositor::setScreen(const QString &name) {
    if (m_screen != name) {
        m_screen = name;
        emit changed();
    }
}
void Compositor::Report(const QString &json) {
    if (calledFromDBus() && message().service() != m_owner)
        return;
    if (json.size() > 4096)
        return;
    auto doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isArray() || doc.array().size() > 16)
        return;
    QSet<QString> screens;
    for (auto item : doc.array()) {
        auto name = item.toString();
        if (!item.isString() || name.isEmpty() || name.size() > 128)
            return;
        screens.insert(name);
    }
    m_screens = screens;
    m_health = "Connected";
    emit changed();
}
} // namespace Nexis
