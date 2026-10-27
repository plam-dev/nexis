#include "services/core/Backend.h"
#include "services/core/Collector.h"
#include "services/core/Compositor.h"
#include "services/core/NotchGeometry.h"
#include <QCoreApplication>
#include <QCursor>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QLockFile>
#include <QPainterPath>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QRegion>
#include <QScreen>
#include <QTextStream>
#include <QTimer>
#include <QUuid>
#include <unistd.h>
#ifdef HAVE_LAYER_SHELL
#include <LayerShellQt/window.h>
#endif
using namespace Nexis;
static QString supportFile(const QString &name) {
    const auto installed = QCoreApplication::applicationDirPath() + "/../share/nexis/" + name;
    return QFileInfo::exists(installed)
               ? installed
               : QString(NEXIS_SOURCE_DIR) + "/" +
                     (name == "hooks.py" ? "integrations/hooks.py" : "scripts/" + name);
}
class Placement : public QObject {
    Q_OBJECT
  public:
    QQuickWindow *window;
    Settings *settings;
    Compositor *compositor;
    bool layered;
    Placement(QQuickWindow *w, Settings *s, Compositor *c, bool layer)
        : QObject(w), window(w), settings(s), compositor(c), layered(layer) {
        connect(s, &Settings::changed, this, &Placement::update);
        connect(w, &QWindow::widthChanged, this, &Placement::update);
        connect(w, &QWindow::heightChanged, this, &Placement::update);
        connect(qGuiApp, &QGuiApplication::screenAdded, this, &Placement::update);
        connect(qGuiApp, &QGuiApplication::screenRemoved, this,
                [this] { QTimer::singleShot(0, this, &Placement::update); });
        QObject::connect(w, SIGNAL(expandedChanged()), this, SLOT(update()));
        for (auto *screen : QGuiApplication::screens())
            connect(screen, &QScreen::geometryChanged, this, &Placement::update);
    }
  public slots:
    void update() {
        auto opts = settings->options();
        QScreen *screen = nullptr;
        for (auto *candidate : QGuiApplication::screens())
            if (candidate->name() == opts["screen"].toString())
                screen = candidate;
        if (!screen)
            screen = QGuiApplication::screenAt(QCursor::pos());
        if (!screen)
            screen = QGuiApplication::primaryScreen();
        if (!screen)
            return;
        const auto rect = screen->geometry();
        const int x = qBound(-(rect.width() - window->width()) / 2, opts["offsetX"].toInt(),
                             qMax(0, (rect.width() - window->width()) / 2));
        const int y = 0;
        window->setScreen(screen);
        compositor->setScreen(screen->name());
#ifdef HAVE_LAYER_SHELL
        if (layered) {
            auto *layer = LayerShellQt::Window::get(window);
            layer->setScreen(screen);
            layer->setAnchors(LayerShellQt::Window::AnchorTop);
            layer->setMargins(QMargins(x, y, -x, 0));
            // -1 uses the output edge instead of avoiding the panel's reserved zone.
            layer->setExclusiveZone(-1);
            layer->setDesiredSize(window->size());
            // Float over the panel without reserving or changing its layout space.
            layer->setLayer(LayerShellQt::Window::LayerOverlay);
            layer->setActivateOnShow(false);
            layer->setCloseOnDismissed(false);
            layer->setKeyboardInteractivity(
                window->property("expanded").toBool()
                    ? LayerShellQt::Window::KeyboardInteractivityOnDemand
                    : LayerShellQt::Window::KeyboardInteractivityNone);
        } else
#endif
            window->setPosition(rect.x() + (rect.width() - window->width()) / 2 + x, rect.y() + y);
        const auto mask = notchPath(window->size());
        window->setMask(QRegion(mask.toFillPolygon().toPolygon()));
    }
};
int main(int argc, char **argv) {
    QStringList arguments;
    for (int i = 1; i < argc; ++i)
        arguments << QString::fromLocal8Bit(argv[i]);
    const bool cli = arguments.contains("doctor") || arguments.value(0) == "hooks" ||
                     arguments.value(0) == "job" || arguments.contains("--snapshot") ||
                     arguments.contains("--help") || arguments.contains("--version");
    const bool headless = cli || arguments.contains("--headless");
    if (qEnvironmentVariableIsEmpty("XDG_RUNTIME_DIR") &&
        QFileInfo::exists("/run/user/" + QString::number(getuid())))
        qputenv("XDG_RUNTIME_DIR", ("/run/user/" + QString::number(getuid())).toLocal8Bit());
    if (qEnvironmentVariableIsEmpty("DBUS_SESSION_BUS_ADDRESS") &&
        !qEnvironmentVariableIsEmpty("XDG_RUNTIME_DIR"))
        qputenv("DBUS_SESSION_BUS_ADDRESS",
                ("unix:path=" + qEnvironmentVariable("XDG_RUNTIME_DIR") + "/bus").toLocal8Bit());
    if (!headless && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM") &&
        qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY") &&
        QFileInfo::exists(qEnvironmentVariable("XDG_RUNTIME_DIR") + "/wayland-0")) {
        qputenv("WAYLAND_DISPLAY", "wayland-0");
        qputenv("QT_QPA_PLATFORM", "wayland");
    }
    std::unique_ptr<QCoreApplication> application;
    if (headless)
        application = std::make_unique<QCoreApplication>(argc, argv);
    else {
        QQuickStyle::setStyle("Fusion");
        application = std::make_unique<QGuiApplication>(argc, argv);
    }
    QCoreApplication::setApplicationName("nexis");
    QCoreApplication::setOrganizationName("nexis");
    QCoreApplication::setApplicationVersion("0.1.0");
    if (arguments.contains("--version")) {
        QTextStream(stdout) << "NEXIS 0.1.0\n";
        return 0;
    }
    if (arguments.contains("--help")) {
        QTextStream(stdout)
            << "NEXIS — native desktop activity island\n\nUsage: nexis [--demo] [--expanded] "
               "[--windowed] [--headless]\n       nexis doctor\n       nexis hooks "
               "install|status|uninstall --provider claude|qoder [--dry-run]\n       nexis "
               "--snapshot\n\nQoder targets the new desktop app. Hooks and autostart are opt-in.\n";
        return 0;
    }
    if (arguments.contains("doctor"))
        return QProcess::execute("bash", {supportFile("doctor.sh")});
    if (arguments.value(0) == "hooks") {
        auto args = arguments.mid(1);
        args << "--bridge" << QCoreApplication::applicationDirPath() + "/nexis-hook-bridge";
        return QProcess::execute("python3", QStringList{supportFile("hooks.py")} + args);
    }
    if (arguments.value(0) == "job") {
        auto value = [&](QString key, QString fallback = QString()) {
            auto i = arguments.indexOf(key);
            return i >= 0 ? arguments.value(i + 1) : fallback;
        };
        bool progressValid = false;
        auto progress = value("--progress", "0").toInt(&progressValid);
        QJsonObject event = {
            {"schema_version", 1},
            {"provider", "local_job"},
            {"event_id", QUuid::createUuid().toString(QUuid::WithoutBraces)},
            {"job_id", value("--id")},
            {"label", value("--label", "Local job")},
            {"event", arguments.value(1)},
            {"progress", progress},
            {"occurred_at", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
        Jobs validator;
        if (!progressValid || !validator.accept(event)) {
            QTextStream(stderr) << "Use: nexis job started|progress|completed|failed --id NAME "
                                   "--label LABEL [--progress 0..100]\n";
            return 1;
        }
        QLocalSocket socket;
        socket.connectToServer(qEnvironmentVariable("XDG_RUNTIME_DIR") + "/nexis/events.sock",
                               QIODevice::WriteOnly);
        if (!socket.waitForConnected(30)) {
            QTextStream(stderr) << "NEXIS collector unavailable.\n";
            return 1;
        }
        socket.write(QJsonDocument(event).toJson(QJsonDocument::Compact) + '\n');
        if (socket.bytesToWrite() && !socket.waitForBytesWritten(30))
            return 1;
        return 0;
    }
    if (arguments.contains("--snapshot")) {
        QDBusInterface iface("org.nexis.Desktop", "/org/nexis/Desktop", "org.nexis.Desktop");
        QDBusReply<QString> reply = iface.call("Snapshot");
        if (!reply.isValid()) {
            QTextStream(stderr) << "NEXIS is not running on the session bus.\n";
            return 1;
        }
        QTextStream(stdout) << reply.value() << "\n";
        return 0;
    }
    const bool demo = arguments.contains("--demo");
    const auto dir = runtimeDirectory(demo);
    if (dir.isEmpty()) {
        QTextStream(stderr) << "NEXIS requires a private, user-owned XDG_RUNTIME_DIR.\n";
        return 1;
    }
    QLockFile lock(dir + "/instance.lock");
    if (!lock.tryLock(0)) {
        QTextStream(stderr) << "NEXIS is already running.\n";
        return 1;
    }
    Settings settings;
    if (!headless) {
        QIcon::setThemeName(settings.colors()["iconTheme"].toString());
        QObject::connect(&settings, &Settings::changed, application.get(), [&settings] {
            QIcon::setThemeName(settings.colors()["iconTheme"].toString());
        });
    }
    Media media;
    System system;
    Backend backend(&settings, &media, &system, demo);
    Collector collector;
    if (!demo && !collector.start(dir)) {
        QTextStream(stderr) << collector.error() << "\n";
        return 1;
    }
    QObject::connect(&collector, &Collector::eventReceived, &backend, &Backend::ingest);
    auto bus = QDBusConnection::sessionBus();
    const auto service = demo ? "org.nexis.Desktop.Demo" : "org.nexis.Desktop";
    // Export snapshots, signals and the tightly scoped panel action allowlist.
    bus.registerService(service);
    bus.registerObject("/org/nexis/Desktop", &backend,
                       QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals);
    Compositor compositor;
    std::unique_ptr<QQmlApplicationEngine> engine;
    if (!headless) {
        engine = std::make_unique<QQmlApplicationEngine>();
        engine->rootContext()->setContextProperty("compositor", &compositor);
        engine->rootContext()->setContextProperty("settings", &settings);
        engine->rootContext()->setContextProperty("backend", &backend);
        engine->rootContext()->setContextProperty("media", &media);
        engine->rootContext()->setContextProperty("systemInfo", &system);
        engine->load(QUrl("qrc:/nexis/Main.qml"));
        if (engine->rootObjects().isEmpty())
            return 1;
        auto *window = qobject_cast<QQuickWindow *>(engine->rootObjects().first());
        if (!window)
            return 1;
        bool layered = false;
#ifdef HAVE_LAYER_SHELL
        layered = !arguments.contains("--windowed") &&
                  QGuiApplication::platformName().startsWith("wayland");
#endif
        if (layered)
            compositor.start(supportFile("kwin-fullscreen.js"), dir, service);
        auto *placement = new Placement(window, &settings, &compositor, layered);
        placement->update();
        window->setProperty("ready", true);
        if (arguments.contains("--expanded"))
            QTimer::singleShot(100, window, [window] { window->setProperty("expanded", true); });
        if (arguments.contains("--smoke-test")) {
            QTimer::singleShot(50, window, [window] { window->setProperty("expanded", true); });
            for (int tab = 0; tab < 4; ++tab)
                QTimer::singleShot(200 + tab * 180, window,
                                   [window, tab] { window->setProperty("tab", tab); });
            QTimer::singleShot(1000, window,
                               [window] { window->setProperty("showSettings", true); });
            QTimer::singleShot(1250, window, [window] { window->setProperty("expanded", false); });
            QTimer::singleShot(1500, application.get(), &QCoreApplication::quit);
        }
        if (arguments.contains("--assert-surface"))
            QTimer::singleShot(950, window, [window] {
                const auto image = window->grabWindow();
                const auto ratio = window->devicePixelRatio();
                if (image.width() != qRound(window->width() * ratio) ||
                    image.height() != qRound(window->height() * ratio)) {
                    QTextStream(stderr) << "Surface mismatch: logical " << window->width() << "x"
                                        << window->height() << " @ " << ratio << ", buffer "
                                        << image.width() << "x" << image.height() << "\n";
                    QCoreApplication::exit(2);
                }
            });
        const auto shot = arguments.indexOf("--screenshot");
        if (shot >= 0 && shot + 1 < arguments.size())
            QTimer::singleShot(900, window, [window, file = arguments[shot + 1]] {
                window->grabWindow().save(file);
            });
    }
    const auto timed = arguments.indexOf("--run-for");
    if (timed >= 0 && timed + 1 < arguments.size())
        QTimer::singleShot(qBound(100, arguments[timed + 1].toInt(), 3600000), application.get(),
                           &QCoreApplication::quit);
    return application->exec();
}
#include "main.moc"
