#include "Monitor.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStorageInfo>
#include <algorithm>
#include <unistd.h>
namespace Nexis {
namespace {
QByteArray slurp(const QString &path, qint64 limit = 65536) {
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.read(limit) : QByteArray();
}
} // namespace
Monitor::Monitor(bool live, QObject *p) : QObject(p), m_live(live) {
    m_timer.setInterval(2000);
    connect(&m_timer, &QTimer::timeout, this, &Monitor::sample);
    if (!m_live) {
        m_state = {
            {"available", true},
            {"cpu", 42.},
            {"memory", QVariantMap{{"used", 9.6e9}, {"total", 16e9}, {"swapUsed", 0.}, {"swapTotal", 8e9}}},
            {"storage", QVariantList{QVariantMap{{"path", "/"}, {"used", 210e9}, {"total", 500e9}}}},
            {"gpus", QVariantList{QVariantMap{{"name", "GPU"}, {"util", 31.}, {"memUsed", 1.2e9}, {"memTotal", 6e9}}}},
            {"apps", QVariantList{QVariantMap{{"name", "firefox"}, {"cpu", 18.}, {"mem", 1.4e9}},
                                  QVariantMap{{"name", "plasmashell"}, {"cpu", 6.}, {"mem", 4.2e8}}}}};
    }
}
void Monitor::setActive(bool active) {
    if (!m_live || active == m_timer.isActive())
        return;
    if (active) {
        m_cpu = {};
        m_ticks.clear();
        m_lastApps = 0;
        sample();
        m_timer.start();
    } else
        m_timer.stop();
}
void Monitor::sample() {
    sampleCpu();
    sampleMemory();
    sampleStorage();
    sampleGpu();
    sampleApps();
    m_state["available"] = true;
    emit changed();
}
void Monitor::sampleCpu() {
    const auto line = slurp("/proc/stat", 512).split('\n').value(0).simplified().split(' ');
    if (line.size() < 5)
        return;
    Cpu now;
    for (int i = 1; i < line.size() && i <= 8; ++i)
        now.total += line[i].toULongLong();
    now.idle = line[4].toULongLong() + line.value(5).toULongLong();
    if (m_cpu.total && now.total > m_cpu.total)
        m_state["cpu"] = 100. * (1. - double(now.idle - m_cpu.idle) / double(now.total - m_cpu.total));
    m_cpu = now;
}
void Monitor::sampleMemory() {
    QHash<QByteArray, double> kb;
    for (const auto &l : slurp("/proc/meminfo").split('\n')) {
        const auto parts = l.simplified().split(' ');
        if (parts.size() >= 2)
            kb[parts[0]] = parts[1].toDouble() * 1024;
    }
    const double total = kb["MemTotal:"];
    m_state["memory"] = QVariantMap{{"total", total},
                                    {"used", std::max(0., total - kb["MemAvailable:"])},
                                    {"swapTotal", kb["SwapTotal:"]},
                                    {"swapUsed", std::max(0., kb["SwapTotal:"] - kb["SwapFree:"])}};
}
void Monitor::sampleStorage() {
    static const QStringList real = {"ext4", "btrfs", "xfs", "f2fs", "ntfs", "ntfs3", "exfat", "vfat", "zfs"};
    QVariantList out;
    QSet<QByteArray> devices;
    for (const auto &v : QStorageInfo::mountedVolumes()) {
        if (!v.isValid() || !v.isReady() || v.bytesTotal() <= 0 ||
            !real.contains(QString::fromUtf8(v.fileSystemType())))
            continue;
        const auto path = v.rootPath();
        if (path.startsWith("/boot") || path.startsWith("/var/lib") || path.startsWith("/run"))
            continue;
        if (devices.contains(v.device()))
            continue; // subvolumes of one filesystem share the same space
        devices.insert(v.device());
        out.append(QVariantMap{{"path", path},
                               {"used", double(v.bytesTotal() - v.bytesAvailable())},
                               {"total", double(v.bytesTotal())}});
        if (out.size() == 4)
            break;
    }
    m_state["storage"] = out;
}
QVariantList Monitor::amdGpus() const {
    QVariantList out;
    for (const auto &card : QDir("/sys/class/drm").entryList({"card?"}, QDir::Dirs)) {
        const QString dev = "/sys/class/drm/" + card + "/device/";
        const auto busy = slurp(dev + "gpu_busy_percent", 16).trimmed();
        if (busy.isEmpty())
            continue;
        out.append(QVariantMap{{"name", "AMD GPU"},
                               {"util", busy.toDouble()},
                               {"memUsed", slurp(dev + "mem_info_vram_used", 32).trimmed().toDouble()},
                               {"memTotal", slurp(dev + "mem_info_vram_total", 32).trimmed().toDouble()}});
    }
    return out;
}
void Monitor::sampleGpu() {
    queryNvidia();
    m_state["gpus"] = m_nvidia + amdGpus();
}
void Monitor::queryNvidia() {
    if (m_nvidiaBusy || m_nvidiaMissing)
        return;
    const auto exe = QStandardPaths::findExecutable("nvidia-smi");
    if (exe.isEmpty()) {
        m_nvidiaMissing = true;
        return;
    }
    m_nvidiaBusy = true;
    auto *p = new QProcess(this);
    auto *timeout = new QTimer(p);
    timeout->setSingleShot(true);
    connect(timeout, &QTimer::timeout, p, &QProcess::kill);
    timeout->start(1500);
    connect(p, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this, p](int code, QProcess::ExitStatus status) {
                m_nvidiaBusy = false;
                QVariantList out;
                if (code == 0 && status == QProcess::NormalExit)
                    for (const auto &l : p->readAllStandardOutput().split('\n')) {
                        const auto f = l.split(',');
                        if (f.size() < 4)
                            continue;
                        out.append(QVariantMap{{"name", QString::fromUtf8(f[0]).trimmed()},
                                               {"util", f[1].trimmed().toDouble()},
                                               {"memUsed", f[2].trimmed().toDouble() * 1048576},
                                               {"memTotal", f[3].trimmed().toDouble() * 1048576}});
                    }
                m_nvidia = out;
                m_state["gpus"] = m_nvidia + amdGpus();
                emit changed();
                p->deleteLater();
            });
    connect(p, &QProcess::errorOccurred, this, [this, p](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            m_nvidiaBusy = false;
            m_nvidiaMissing = true;
            p->deleteLater();
        }
    });
    p->start(exe, {"--query-gpu=name,utilization.gpu,memory.used,memory.total",
                   "--format=csv,noheader,nounits"});
}
void Monitor::sampleApps() {
    static const double tick = double(sysconf(_SC_CLK_TCK));
    static const double page = double(sysconf(_SC_PAGESIZE));
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const double elapsed = m_lastApps ? double(now - m_lastApps) / 1000. : 0.;
    struct Agg {
        double cpu = 0, mem = 0;
    };
    QHash<QString, Agg> byName;
    QHash<qint64, quint64> seen;
    // Only the comm name and numeric counters are read; command lines are never touched.
    for (const auto &entry : QDir("/proc").entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool ok = false;
        const qint64 pid = entry.toLongLong(&ok);
        if (!ok)
            continue;
        const auto stat = slurp("/proc/" + entry + "/stat", 1024);
        const int open = stat.indexOf('('), close = stat.lastIndexOf(')');
        if (open < 0 || close < open)
            continue;
        const auto rest = stat.mid(close + 2).split(' ');
        if (rest.size() < 22)
            continue;
        const quint64 ticks = rest[11].toULongLong() + rest[12].toULongLong();
        const double rss = rest[21].toDouble() * page;
        seen[pid] = ticks;
        const auto name = QString::fromUtf8(stat.mid(open + 1, close - open - 1));
        auto &agg = byName[name];
        agg.mem += rss;
        if (elapsed > 0 && m_ticks.contains(pid) && ticks >= m_ticks[pid])
            agg.cpu += 100. * double(ticks - m_ticks[pid]) / tick / elapsed;
    }
    m_ticks = seen;
    m_lastApps = now;
    QList<QPair<QString, Agg>> ranked;
    for (auto it = byName.cbegin(); it != byName.cend(); ++it)
        ranked.append({it.key(), it.value()});
    std::sort(ranked.begin(), ranked.end(), [](const auto &a, const auto &b) {
        return a.second.cpu != b.second.cpu ? a.second.cpu > b.second.cpu : a.second.mem > b.second.mem;
    });
    QVariantList top;
    for (int i = 0; i < std::min<qsizetype>(5, ranked.size()); ++i)
        top.append(QVariantMap{{"name", ranked[i].first}, {"cpu", ranked[i].second.cpu}, {"mem", ranked[i].second.mem}});
    m_state["apps"] = top;
}
} // namespace Nexis
