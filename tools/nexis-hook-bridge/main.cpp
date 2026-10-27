#include "modules/agents/Events.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QLocalSocket>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    QString provider;
    auto i = args.indexOf("--provider");
    if (i >= 0 && i + 1 < args.size()) {
        auto p = args[i + 1];
        provider = p == "claude" ? "claude_code" : p == "qoder" ? "qoder_app" : "";
    }
    if (provider.isEmpty())
        return 0;
    QElapsedTimer timer;
    timer.start();
    QByteArray raw;
    char buf[4096];
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
    while (timer.elapsed() < 35) {
        struct pollfd fd{STDIN_FILENO, POLLIN, 0};
        if (poll(&fd, 1, qMax(0, 35 - int(timer.elapsed()))) <= 0)
            return 0;
        const auto n = read(STDIN_FILENO, buf, sizeof(buf));
        if (n == 0)
            break;
        if (n < 0) {
            if (errno == EAGAIN)
                continue;
            return 0;
        }
        raw.append(buf, n);
        if (raw.size() > 65536)
            return 0;
    }
    if (timer.elapsed() >= 35)
        return 0;
    QJsonParseError err;
    auto doc = QJsonDocument::fromJson(raw, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return 0;
    auto event = Nexis::normalize(doc.object(), provider);
    if (event.isEmpty())
        return 0;
    const auto runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (runtime.isEmpty())
        return 0;
    QLocalSocket socket;
    socket.connectToServer(runtime + "/nexis/events.sock", QIODevice::WriteOnly);
    if (!socket.waitForConnected(20))
        return 0;
    struct ucred cred{};
    socklen_t length = sizeof(cred);
    if (getsockopt(socket.socketDescriptor(), SOL_SOCKET, SO_PEERCRED, &cred, &length) ||
        cred.uid != getuid())
        return 0;
    auto data = QJsonDocument(event).toJson(QJsonDocument::Compact) + '\n';
    socket.write(data);
    if (socket.bytesToWrite())
        socket.waitForBytesWritten(20);
    socket.abort();
    return 0;
}
