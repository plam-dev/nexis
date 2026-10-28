#pragma once
#include <QHash>
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariantMap>
namespace Nexis {
// Local utilization sampler for the System Monitoring tab. It reads /proc, sysfs, mounted
// volumes and (when present) nvidia-smi, and only runs while the island is expanded.
class Monitor : public QObject {
    Q_OBJECT
  public:
    explicit Monitor(bool live, QObject *parent = nullptr);
    QVariantMap state() const { return m_state; }
    void setActive(bool active);
  signals:
    void changed();

  private:
    struct Cpu {
        quint64 total = 0, idle = 0;
    };
    struct Proc {
        quint64 ticks = 0;
    };
    void sample();
    void sampleCpu();
    void sampleMemory();
    void sampleStorage();
    void sampleGpu();
    void sampleApps();
    void queryNvidia();
    QVariantList amdGpus() const;
    bool m_live;
    QTimer m_timer;
    QVariantMap m_state;
    Cpu m_cpu;
    QHash<qint64, quint64> m_ticks;
    qint64 m_lastApps = 0;
    QVariantList m_nvidia;
    bool m_nvidiaBusy = false, m_nvidiaMissing = false;
};
} // namespace Nexis
