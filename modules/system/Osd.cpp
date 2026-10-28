#include "Osd.h"
#include <QDBusConnection>
#include <QDBusServiceWatcher>
namespace Nexis {
Osd::Osd(bool enabled, QObject *parent) : QObject(parent), m_enabled(enabled) {
    m_expiry.setSingleShot(true);
    m_expiry.setInterval(3000);
    m_expiry.setTimerType(Qt::PreciseTimer);
    connect(&m_expiry, &QTimer::timeout, this, &Osd::clear);
    if (!enabled)
        return;
    auto bus = QDBusConnection::sessionBus();
    const QString service = "org.kde.plasmashell", path = "/org/kde/osdService";
    const QString iface = "org.kde.osdService";
    // A named sender also follows Plasma owner changes and rejects other processes.
    bus.connect(service, path, iface, "osdProgress", this,
                SLOT(progress(QString, int, int, QString)));
    bus.connect(service, path, iface, "osdText", this, SLOT(text(QString, QString)));
    auto *watcher =
        new QDBusServiceWatcher(service, bus, QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(watcher, &QDBusServiceWatcher::serviceOwnerChanged, this, &Osd::clear);
}
void Osd::clear() {
    m_expiry.stop();
    if (m_banner.isEmpty())
        return;
    m_banner.clear();
    emit changed();
}
void Osd::show(const QVariantMap &banner) {
    if (!m_enabled)
        return;
    m_banner = banner;
    m_expiry.start();
    emit changed();
}
void Osd::progress(const QString &icon, int percent, int maximum, const QString &text) {
    if (maximum <= 0 || percent < 0 || percent > maximum)
        return;
    const auto detail = text.left(200).simplified();
    QString label;
    if (icon.startsWith("audio-volume"))
        label = "Volume";
    else if (icon.startsWith("video-display-brightness"))
        label = "Brightness";
    else if (icon.startsWith("input-keyboard-brightness"))
        label = "Keyboard brightness";
    else if (icon.startsWith("microphone-sensitivity"))
        label = "Microphone";
    else
        label = detail.isEmpty() ? QString("System") : detail;
    show({{"mode", "feedback"},
          {"status", "feedback"},
          {"title", (label + " · " + QString::number(percent) + "%").left(200)},
          {"subtitle", label == detail ? QString() : detail},
          {"progress", double(percent) / maximum}});
}
void Osd::text(const QString &, const QString &value) {
    const auto title = value.left(200).simplified();
    if (!title.isEmpty())
        show({{"mode", "feedback"}, {"status", "feedback"}, {"title", title}});
}
void Osd::volumeChanged(double volume, bool muted) {
    if (muted)
        text("audio-volume-muted", "Audio Muted");
    else
        progress("audio-volume", qRound(volume * 100), 100, {});
}
} // namespace Nexis
