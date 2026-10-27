#pragma once
#include <QDBusContext>
#include <QObject>
#include <QSet>
namespace Nexis {
class Compositor : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.nexis.Compositor")
    Q_PROPERTY(bool fullscreen READ fullscreen NOTIFY changed)
    Q_PROPERTY(QString health READ health NOTIFY changed)
  public:
    explicit Compositor(QObject *parent = nullptr) : QObject(parent) {}
    ~Compositor();
    void start(const QString &scriptPath, const QString &runtime, const QString &service);
    void setScreen(const QString &screen);
    bool fullscreen() const { return m_screens.contains(m_screen); }
    QString health() const { return m_health; }
  public slots:
    void Report(const QString &json);
  signals:
    void changed();

  private:
    void load();
    QString m_screen, m_script, m_service, m_name, m_owner, m_health = "Unavailable";
    QSet<QString> m_screens;
    bool m_loaded = false;
    quint64 m_generation = 0;
};
} // namespace Nexis
