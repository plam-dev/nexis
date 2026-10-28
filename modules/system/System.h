#pragma once
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariantMap>
namespace Nexis {
class System : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap state READ state NOTIFY changed)
  public:
    explicit System(QObject *parent = nullptr);
    QVariantMap state() const { return m_state; }
    Q_INVOKABLE void setVolume(double value);
    Q_INVOKABLE void toggleMute();
  signals:
    void changed();
    void volumeFeedback(double volume, bool muted);
  private slots:
    void refresh();

  private:
    void runAudio(const QStringList &arguments, bool read);
    QVariantMap m_state = {{"network", "Unavailable"},
                           {"batteryAvailable", false},
                           {"volumeAvailable", false},
                           {"critical", false},
                           {"audioError", ""}};
    bool m_audioBusy = false;
    bool m_volumeFeedbackPending = false;
    QTimer m_timer;
};
} // namespace Nexis
