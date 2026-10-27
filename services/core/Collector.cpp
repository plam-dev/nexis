#include "Collector.h"
#include <QDateTime>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QTimer>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
namespace Nexis {
QString runtimeDirectory(bool demo) {
    const auto base = qEnvironmentVariable("XDG_RUNTIME_DIR");
    struct stat st{};
    if (base.isEmpty() || ::lstat(base.toLocal8Bit().constData(), &st) || !S_ISDIR(st.st_mode) ||
        st.st_uid != getuid() || (st.st_mode & 0077))
        return {};
    auto dir = base + (demo ? "/nexis-demo" : "/nexis");
    if (::mkdir(dir.toLocal8Bit().constData(), 0700) && errno != EEXIST)
        return {};
    if (::lstat(dir.toLocal8Bit().constData(), &st) || !S_ISDIR(st.st_mode) ||
        st.st_uid != getuid() || (st.st_mode & 0077))
        return {};
    return dir;
}
bool Collector::start(const QString &directory) {
    if (directory.isEmpty()) {
        m_error = "A private XDG_RUNTIME_DIR is required.";
        return false;
    }
    const auto path = directory + "/events.sock";
    QLocalServer::removeServer(path);
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    m_server.setMaxPendingConnections(16);
    if (!m_server.listen(path)) {
        m_error = m_server.errorString();
        return false;
    }
    ::chmod(path.toLocal8Bit().constData(), 0600);
    connect(&m_server, &QLocalServer::newConnection, this, [this] {
        while (m_server.hasPendingConnections()) {
            auto *s = m_server.nextPendingConnection();
            struct ucred cred{};
            socklen_t length = sizeof(cred);
            if (m_clients >= 16 ||
                getsockopt(s->socketDescriptor(), SOL_SOCKET, SO_PEERCRED, &cred, &length) ||
                cred.uid != getuid()) {
                s->abort();
                s->deleteLater();
                continue;
            }
            ++m_clients;
            s->setReadBufferSize(16385);
            connect(s, &QLocalSocket::disconnected, this, [this, s] {
                --m_clients;
                s->deleteLater();
            });
            QTimer::singleShot(500, s, [s] { s->abort(); });
            auto read = [this, s] {
                if (s->bytesAvailable() > 16384) {
                    s->abort();
                    return;
                }
                if (!s->canReadLine())
                    return;
                const auto data = s->readLine(16385);
                const auto second = QDateTime::currentSecsSinceEpoch();
                if (second != m_second) {
                    m_second = second;
                    m_rate = 0;
                }
                if (++m_rate <= 100 && data.size() <= 16384) {
                    QJsonParseError e;
                    auto doc = QJsonDocument::fromJson(data, &e);
                    if (e.error == QJsonParseError::NoError && doc.isObject())
                        emit eventReceived(doc.object());
                }
                s->disconnectFromServer();
            };
            connect(s, &QLocalSocket::readyRead, this, read);
            if (s->bytesAvailable())
                read();
        }
    });
    return true;
}
} // namespace Nexis
