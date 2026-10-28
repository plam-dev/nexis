#include "System.h"
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QDBusVariant>
#include <QRegularExpression>
#include <QStandardPaths>
namespace Nexis {
System::System(QObject *p) : QObject(p) {
    auto bus = QDBusConnection::systemBus();
    bus.connect("org.freedesktop.NetworkManager", "/org/freedesktop/NetworkManager",
                "org.freedesktop.DBus.Properties", "PropertiesChanged", this, SLOT(refresh()));
    bus.connect("org.freedesktop.UPower", "", "org.freedesktop.DBus.Properties",
                "PropertiesChanged", this, SLOT(refresh()));
    auto *watch = new QDBusServiceWatcher("org.freedesktop.UPower", bus,
                                          QDBusServiceWatcher::WatchForOwnerChange, this);
    watch->addWatchedService("org.freedesktop.NetworkManager");
    connect(watch, &QDBusServiceWatcher::serviceOwnerChanged, this, &System::refresh);
    m_timer.setInterval(15000);
    connect(&m_timer, &QTimer::timeout, this,
            [this] { runAudio({"get-volume", "@DEFAULT_AUDIO_SINK@"}, true); });
    m_timer.start();
    QTimer::singleShot(0, this, &System::refresh);
}
void System::refresh() {
    auto bus = QDBusConnection::systemBus();
    auto get = QDBusMessage::createMethodCall("org.freedesktop.NetworkManager",
                                              "/org/freedesktop/NetworkManager",
                                              "org.freedesktop.DBus.Properties", "Get");
    get << QString("org.freedesktop.NetworkManager") << QString("State");
    auto *w = new QDBusPendingCallWatcher(bus.asyncCall(get, 1000), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this, w] {
        QDBusPendingReply<QDBusVariant> r = *w;
        const auto value = r.isError() ? 0 : r.value().variant().toUInt();
        m_state["network"] = value >= 70   ? "Connected"
                             : value >= 50 ? "Local connection"
                             : value == 40 ? "Connecting"
                                           : "Offline";
        if (r.isError())
            m_state["network"] = "Unavailable";
        emit changed();
        w->deleteLater();
    });
    auto battery = QDBusMessage::createMethodCall("org.freedesktop.UPower",
                                                  "/org/freedesktop/UPower/devices/DisplayDevice",
                                                  "org.freedesktop.DBus.Properties", "GetAll");
    battery << QString("org.freedesktop.UPower.Device");
    auto *bw = new QDBusPendingCallWatcher(bus.asyncCall(battery, 1000), this);
    connect(bw, &QDBusPendingCallWatcher::finished, this, [this, bw] {
        QDBusPendingReply<QVariantMap> r = *bw;
        const auto p = r.isError() ? QVariantMap() : r.value();
        m_state["batteryAvailable"] = p.value("IsPresent").toBool();
        m_state["battery"] = p.value("Percentage");
        m_state["charging"] = p.value("State").toUInt() == 1;
        m_state["critical"] = p.value("IsPresent").toBool() && p.value("State").toUInt() == 2 &&
                              p.value("Percentage").toDouble() < 5;
        emit changed();
        bw->deleteLater();
    });
    runAudio({"get-volume", "@DEFAULT_AUDIO_SINK@"}, true);
}
void System::runAudio(const QStringList &args, bool read) {
    if (m_audioBusy)
        return;
    const auto wpctl = QStandardPaths::findExecutable("wpctl");
    if (wpctl.isEmpty())
        return;
    m_audioBusy = true;
    auto *p = new QProcess(this);
    p->setProcessChannelMode(QProcess::SeparateChannels);
    auto *timeout = new QTimer(p);
    timeout->setSingleShot(true);
    connect(timeout, &QTimer::timeout, p, &QProcess::kill);
    timeout->start(800);
    connect(p, &QProcess::errorOccurred, this, [this, p](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            m_audioBusy = false;
            m_volumeFeedbackPending = false;
            m_state["volumeAvailable"] = false;
            emit changed();
            p->deleteLater();
        }
    });
    connect(p, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this, p, read](int code, QProcess::ExitStatus status) {
                m_audioBusy = false;
                if (read) {
                    const bool report = m_volumeFeedbackPending;
                    m_volumeFeedbackPending = false;
                    auto match = QRegularExpression("Volume: ([0-9.]+)(.*)")
                                     .match(QString::fromUtf8(p->readAllStandardOutput()));
                    m_state["volumeAvailable"] =
                        code == 0 && status == QProcess::NormalExit && match.hasMatch();
                    if (m_state["volumeAvailable"].toBool()) {
                        m_state["volume"] = qBound(0., match.captured(1).toDouble(), 1.);
                        m_state["muted"] = match.captured(2).contains("MUTED");
                        if (report)
                            emit volumeFeedback(m_state["volume"].toDouble(),
                                                m_state["muted"].toBool());
                    }
                } else {
                    m_volumeFeedbackPending = code == 0 && status == QProcess::NormalExit;
                    m_state["audioError"] = m_volumeFeedbackPending ? "" : "Output change failed";
                    QTimer::singleShot(0, this, [this] {
                        runAudio({"get-volume", "@DEFAULT_AUDIO_SINK@"}, true);
                    });
                }
                emit changed();
                p->deleteLater();
            });
    p->start(wpctl, args);
}
void System::setVolume(double value) {
    if (m_state["volumeAvailable"].toBool() && qIsFinite(value))
        runAudio(
            {"set-volume", "@DEFAULT_AUDIO_SINK@", QString::number(qBound(0., value, 1.), 'f', 2)},
            false);
}
void System::toggleMute() {
    if (m_state["volumeAvailable"].toBool())
        runAudio({"set-mute", "@DEFAULT_AUDIO_SINK@", "toggle"}, false);
}
} // namespace Nexis
