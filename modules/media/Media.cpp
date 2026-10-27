#include "Media.h"
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QFileInfo>
#include <QImageReader>
#include <QStandardPaths>
#include <QUrl>
namespace Nexis {
static const QString path = "/org/mpris/MediaPlayer2", iface = "org.mpris.MediaPlayer2.Player";
static QVariantMap mapValue(const QVariant &v) {
    return v.metaType() == QMetaType::fromType<QDBusArgument>() ? qdbus_cast<QVariantMap>(v)
                                                                : v.toMap();
}
QVariantMap mediaSnapshot(const QVariantMap &p) {
    const auto meta = mapValue(p.value("Metadata"));
    QVariantMap out;
    out["title"] = meta.value("xesam:title", "Unknown track").toString().left(200);
    out["artist"] = meta.value("xesam:artist").toStringList().join(", ").left(200);
    out["duration"] = meta.value("mpris:length", 0).toLongLong();
    out["position"] = p.value("Position", 0).toLongLong();
    out["playing"] = p.value("PlaybackStatus") == "Playing";
    out["art"] = QString();
    const QUrl art(meta.value("mpris:artUrl").toString());
    if (art.isLocalFile() && art.toString().size() <= 2048) {
        const QFileInfo f(art.toLocalFile());
        const auto canonical = f.canonicalFilePath();
        const auto home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/";
        const auto cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/";
        if (canonical.size() <= 2048 && f.isFile() && f.size() <= 4 * 1024 * 1024 &&
            (canonical.startsWith(home) || canonical.startsWith(cache) ||
             canonical.startsWith("/tmp/"))) {
            QImageReader reader(canonical);
            auto size = reader.size();
            if (size.isValid() && size.width() <= 4096 && size.height() <= 4096 &&
                reader.format() != "svg")
                out["art"] = QUrl::fromLocalFile(canonical);
        }
    }
    out["canPlayPause"] = mediaCan(p, "PlayPause");
    out["canNext"] = mediaCan(p, "Next");
    out["canPrevious"] = mediaCan(p, "Previous");
    out["canSeek"] = mediaCan(p, "SetPosition") && out["duration"].toLongLong() > 0;
    return out;
}
bool mediaCan(const QVariantMap &p, const QString &action) {
    if (!p.value("CanControl").toBool())
        return false;
    if (action == "PlayPause")
        return p.value(p.value("PlaybackStatus") == "Playing" ? "CanPause" : "CanPlay").toBool();
    if (action == "Next")
        return p.value("CanGoNext").toBool();
    if (action == "Previous")
        return p.value("CanGoPrevious").toBool();
    if (action == "SetPosition")
        return p.value("CanSeek").toBool();
    return false;
}
QString choosePlayer(const QMap<QString, QVariantMap> &players, const QString &pinned,
                     const QString &last) {
    if (players.contains(pinned))
        return pinned;
    for (auto it = players.cbegin(); it != players.cend(); ++it)
        if (it.value().value("PlaybackStatus") == "Playing")
            return it.key();
    if (players.contains(last))
        return last;
    return players.isEmpty() ? QString() : players.firstKey();
}
Media::Media(QObject *parent) : QObject(parent) {
    auto bus = QDBusConnection::sessionBus();
    bus.connect("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
                "NameOwnerChanged", this, SLOT(ownerChanged(QString, QString, QString)));
    m_position.setInterval(1000);
    connect(&m_position, &QTimer::timeout, this, &Media::refreshPosition);
    QTimer::singleShot(0, this, &Media::discover);
}
void Media::ownerChanged(const QString &service, const QString &, const QString &owner) {
    if (!service.startsWith("org.mpris.MediaPlayer2."))
        return;
    auto bus = QDBusConnection::sessionBus();
    m_players.remove(service);
    m_ownerPending.remove(service);
    m_subscribed.remove(service);
    m_owners.remove(service);
    ++m_generation[service];
    bus.disconnect(service, path, "org.freedesktop.DBus.Properties", "PropertiesChanged", this,
                   SLOT(propertiesChanged(QString, QVariantMap, QStringList, QDBusMessage)));
    bus.disconnect(service, path, iface, "Seeked", this, SLOT(seeked(qlonglong, QDBusMessage)));
    if (!owner.isEmpty()) {
        if (m_owners.size() >= 32)
            return;
        m_owners[service] = owner;
        refresh(service);
    } else
        emit changed();
}
void Media::discover() {
    auto *w = new QDBusPendingCallWatcher(
        QDBusConnection::sessionBus().interface()->asyncCall("ListNames"), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this, w] {
        QDBusPendingReply<QStringList> r = *w;
        if (!r.isError())
            for (const auto &s : r.value())
                if (s.startsWith("org.mpris.MediaPlayer2."))
                    refresh(s);
        w->deleteLater();
    });
}
void Media::refresh(const QString &service) {
    auto bus = QDBusConnection::sessionBus();
    if (!m_owners.contains(service)) {
        if (m_owners.size() + m_ownerPending.size() >= 32)
            return;
        if (m_ownerPending.contains(service))
            return;
        m_ownerPending.insert(service);
        const int generation = m_generation.value(service);
        auto *owner =
            new QDBusPendingCallWatcher(bus.interface()->asyncCall("GetNameOwner", service), this);
        connect(owner, &QDBusPendingCallWatcher::finished, this,
                [this, owner, service, generation] {
                    QDBusPendingReply<QString> r = *owner;
                    m_ownerPending.remove(service);
                    if (!r.isError() && generation == m_generation.value(service)) {
                        m_owners[service] = r.value();
                        refresh(service);
                    }
                    owner->deleteLater();
                });
        return;
    }
    if (!m_subscribed.contains(service)) {
        m_subscribed.insert(service);
        bus.connect(service, path, "org.freedesktop.DBus.Properties", "PropertiesChanged", this,
                    SLOT(propertiesChanged(QString, QVariantMap, QStringList, QDBusMessage)));
        bus.connect(service, path, iface, "Seeked", this, SLOT(seeked(qlonglong, QDBusMessage)));
    }
    const int generation = ++m_generation[service];
    auto msg =
        QDBusMessage::createMethodCall(service, path, "org.freedesktop.DBus.Properties", "GetAll");
    msg << iface;
    auto *w = new QDBusPendingCallWatcher(bus.asyncCall(msg, 1000), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this, w, service, generation] {
        QDBusPendingReply<QVariantMap> r = *w;
        if (!r.isError() && generation == m_generation.value(service)) {
            auto identity = m_players.value(service).value("identity");
            m_players[service] = r.value();
            m_players[service]["identity"] = identity;
            emit changed();
        }
        w->deleteLater();
    });
    auto id =
        QDBusMessage::createMethodCall(service, path, "org.freedesktop.DBus.Properties", "Get");
    id << QString("org.mpris.MediaPlayer2") << QString("Identity");
    auto *iw = new QDBusPendingCallWatcher(bus.asyncCall(id, 1000), this);
    connect(iw, &QDBusPendingCallWatcher::finished, this, [this, iw, service, generation] {
        QDBusPendingReply<QDBusVariant> r = *iw;
        if (!r.isError() && generation == m_generation.value(service)) {
            m_players[service]["identity"] = r.value().variant().toString().left(100);
            emit changed();
        }
        iw->deleteLater();
    });
}
void Media::propertiesChanged(const QString &which, const QVariantMap &changed,
                              const QStringList &invalidated, const QDBusMessage &message) {
    if (which != iface)
        return;
    for (const auto &service : m_players.keys())
        if (m_owners.value(service) == message.service()) {
            if (!invalidated.isEmpty()) {
                refresh(service);
                return;
            }
            for (auto it = changed.begin(); it != changed.end(); ++it)
                m_players[service][it.key()] = it.value();
            if (changed.contains("Metadata"))
                m_players[service]["Position"] = 0;
            emit this->changed();
        }
}
void Media::seeked(qlonglong, const QDBusMessage &) {
    refreshPosition();
}
QVariantList Media::players() const {
    QVariantList list;
    for (auto it = m_players.cbegin(); it != m_players.cend(); ++it) {
        auto p = mediaSnapshot(it.value());
        p["service"] = it.key();
        p["identity"] = it.value().value("identity", it.key().mid(23));
        list << p;
    }
    return list;
}
QVariantMap Media::current() const {
    const auto service = choosePlayer(m_players, m_pinned, m_last);
    if (service.isEmpty())
        return {};
    auto out = mediaSnapshot(m_players.value(service));
    out["service"] = service;
    out["identity"] = m_players.value(service).value("identity", service.mid(23));
    return out;
}
void Media::select(const QString &service) {
    if (m_players.contains(service)) {
        m_pinned = service;
        m_last = service;
        emit changed();
        refreshPosition();
    }
}
void Media::control(const QString &action) {
    auto service = choosePlayer(m_players, m_pinned, m_last);
    if (!mediaCan(m_players.value(service), action))
        return;
    auto msg = QDBusMessage::createMethodCall(service, path, iface, action);
    auto *w = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(msg, 1000), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this, w] {
        QDBusPendingReply<> r = *w;
        m_error = r.isError() ? r.error().message() : QString();
        emit changed();
        w->deleteLater();
    });
    m_last = service;
}
void Media::seek(double fraction) {
    const auto service = choosePlayer(m_players, m_pinned, m_last);
    const auto p = m_players.value(service);
    if (!mediaCan(p, "SetPosition") || !qIsFinite(fraction))
        return;
    const auto meta = mapValue(p.value("Metadata"));
    const auto track = qvariant_cast<QDBusObjectPath>(meta.value("mpris:trackid"));
    if (track.path().isEmpty() || track.path() == "/org/mpris/MediaPlayer2/TrackList/NoTrack")
        return;
    const auto duration = meta.value("mpris:length").toLongLong();
    if (duration <= 0)
        return;
    auto msg = QDBusMessage::createMethodCall(service, path, iface, "SetPosition");
    msg << QVariant::fromValue(track)
        << QVariant::fromValue(qlonglong(qBound(0., fraction, 1.) * duration));
    QDBusConnection::sessionBus().asyncCall(msg, 1000);
}
void Media::setExpanded(bool expanded) {
    if (expanded) {
        m_position.start();
        refreshPosition();
    } else
        m_position.stop();
}
void Media::refreshPosition() {
    auto service = choosePlayer(m_players, m_pinned, m_last);
    if (service.isEmpty())
        return;
    auto msg =
        QDBusMessage::createMethodCall(service, path, "org.freedesktop.DBus.Properties", "Get");
    msg << iface << QString("Position");
    auto *w = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(msg, 700), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this, w, service] {
        QDBusPendingReply<QDBusVariant> r = *w;
        if (!r.isError() && m_players.contains(service)) {
            m_players[service]["Position"] = r.value().variant();
            emit changed();
        }
        w->deleteLater();
    });
}
} // namespace Nexis
