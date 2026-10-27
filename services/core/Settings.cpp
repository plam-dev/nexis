#include "Settings.h"
#include <QColor>
#include <QDir>
#include <QStandardPaths>
namespace Nexis {
Settings::Settings(QObject *p) : QObject(p), m_store("nexis", "Desktop") {
    const auto config = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    m_watcher.addPath(config);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { emit changed(); });
    if (QFile::exists(config + "/kdeglobals")) {
        m_watcher.addPath(config + "/kdeglobals");
        connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this, config] {
            if (!m_watcher.files().contains(config + "/kdeglobals"))
                m_watcher.addPath(config + "/kdeglobals");
            emit changed();
        });
    }
}
QVariantMap Settings::options() const {
    QVariantMap defaults = {{"middleClick", "media"},
                            {"theme", "auto"},
                            {"reducedMotion", false},
                            {"hoverExpand", false},
                            {"fullscreenOverlay", false},
                            {"idleClock", true},
                            {"attentionOverride", true},
                            {"pinned", ""},
                            {"screen", ""},
                            {"offsetX", 0},
                            {"offsetY", 12},
                            {"history", true}};
    for (auto it = defaults.begin(); it != defaults.end(); ++it) {
        auto value = m_store.value(it.key(), it.value());
        // INI values can reload as strings; keep bool/number types at the QML/JSON boundary.
        if (value.convert(it.value().metaType()))
            it.value() = value;
    }
    const auto screen = defaults["screen"].toString();
    if (!screen.isEmpty()) {
        defaults["offsetX"] = m_store.value("positions/" + screen + "/x", 0).toInt();
        defaults["offsetY"] = m_store.value("positions/" + screen + "/y", 12).toInt();
    }
    return defaults;
}
void Settings::set(const QString &k, const QVariant &v) {
    const auto opts = options();
    if (!opts.contains(k))
        return;
    QVariant value = v;
    if (k == "theme" && !QStringList{"auto", "dark", "light"}.contains(v.toString()))
        return;
    if (k == "middleClick" && !QStringList{"media", "toggle", "none"}.contains(v.toString()))
        return;
    if (k == "pinned" && !QStringList{"", "media", "agents"}.contains(v.toString()))
        return;
    if (k == "offsetX")
        value = qBound(-3000, v.toInt(), 3000);
    if (k == "offsetY")
        value = qBound(0, v.toInt(), 2000);
    if (k == "screen")
        value = v.toString().left(128);
    if (k == "offsetX" || k == "offsetY") {
        auto screen = opts["screen"].toString();
        if (!screen.isEmpty())
            m_store.setValue("positions/" + screen + (k == "offsetX" ? "/x" : "/y"), value);
    }
    m_store.setValue(k, value);
    m_store.sync();
    emit changed();
}
void Settings::resetPosition() {
    set("offsetX", 0);
    set("offsetY", 12);
}
QVariantMap Settings::colors() const {
    QSettings kde(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
                      "/kdeglobals",
                  QSettings::IniFormat);
    auto color = [&](const QString &key, const QColor &fallback) {
        auto entry = kde.value(key);
        auto parts = entry.metaType() == QMetaType::fromType<QStringList>()
                         ? entry.toStringList()
                         : entry.toString().split(',');
        return parts.size() >= 3 ? QColor(parts[0].toInt(), parts[1].toInt(), parts[2].toInt())
                                 : fallback;
    };
    const auto mode = options()["theme"].toString();
    const QColor desktop = color("Colors:Window/BackgroundNormal", QColor("#282828"));
    const bool dark = mode == "dark" || (mode == "auto" && desktop.lightness() < 128);
    const bool follow = mode == "auto";
    return {{"background", follow ? desktop : QColor(dark ? "#282828" : "#fafafa")},
            {"surface", follow ? color("Colors:View/BackgroundNormal", QColor("#303030"))
                               : QColor(dark ? "#303030" : "#eeeeee")},
            {"text", follow ? color("Colors:Window/ForegroundNormal", QColor("#dfdfdf"))
                            : QColor(dark ? "#dfdfdf" : "#242424")},
            {"muted", QColor(dark ? "#aaaaaa" : "#616161")},
            {"accent", color("Colors:Selection/BackgroundNormal", QColor("#1a73e8"))},
            {"border", QColor(dark ? "#494949" : "#d4d4d4")},
            {"dark", dark},
            {"iconTheme", kde.value("Icons/Theme", "breeze").toString()},
            {"name", kde.value("KDE/LookAndFeelPackage", "System theme").toString()}};
}
} // namespace Nexis
