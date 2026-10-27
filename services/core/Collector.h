#pragma once
#include <QLocalServer>
#include <QObject>
namespace Nexis {
QString runtimeDirectory(bool demo = false);
class Collector : public QObject {
    Q_OBJECT
  public:
    explicit Collector(QObject *parent = nullptr) : QObject(parent) {}
    bool start(const QString &directory);
    QString error() const { return m_error; }
  signals:
    void eventReceived(const QJsonObject &event);

  private:
    QLocalServer m_server;
    QString m_error;
    int m_clients = 0, m_rate = 0;
    qint64 m_second = 0;
};
} // namespace Nexis
