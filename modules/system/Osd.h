#pragma once
#include <QObject>
#include <QTimer>
#include <QVariantMap>
namespace Nexis {
class Osd : public QObject {
    Q_OBJECT
  public:
    explicit Osd(bool enabled, QObject *parent = nullptr);
    QVariantMap banner() const { return m_banner; }
    void volumeChanged(double volume, bool muted);
  signals:
    void changed();
  private slots:
    void progress(const QString &icon, int percent, int maximum, const QString &text);
    void text(const QString &icon, const QString &text);

  private:
    void show(const QVariantMap &banner);
    void clear();
    bool m_enabled;
    QVariantMap m_banner;
    QTimer m_expiry;
};
} // namespace Nexis
