#include "Daemon.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDBusInterface>
#include <QDBusReply>
#include <QStandardPaths>

#include <cstdio>

namespace {

void setAppInfo()
{
    QCoreApplication::setApplicationName(QStringLiteral("clipboard-linux"));
    QCoreApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
}

// Sends one command to the running daemon over D-Bus.
int runClient()
{
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral(
        "Clipboard history manager. Run without arguments to start the background service."));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption pasteOption(QStringLiteral("paste"),
        QStringLiteral("Paste history item <n> (1 = newest)."), QStringLiteral("n"));
    const QCommandLineOption clearAllOption(QStringLiteral("clear-all"),
        QStringLiteral("Remove all items except pinned ones."));
    const QCommandLineOption quitOption(QStringLiteral("quit"),
        QStringLiteral("Stop the background service."));
    parser.addOptions({pasteOption, clearAllOption, quitOption});
    parser.process(*QCoreApplication::instance());

    QDBusInterface daemon(QString::fromLatin1(Daemon::ServiceName),
                          QString::fromLatin1(Daemon::ObjectPath),
                          QString::fromLatin1(Daemon::InterfaceName),
                          QDBusConnection::sessionBus());
    if (!daemon.isValid()) {
        std::fprintf(stderr, "clipboard-linux is not running.\n");
        return 1;
    }

    QDBusMessage reply;
    if (parser.isSet(pasteOption)) {
        bool ok = false;
        const int number = parser.value(pasteOption).toInt(&ok);
        if (!ok) {
            std::fprintf(stderr, "--paste needs a number.\n");
            return 2;
        }
        const QDBusReply<bool> pasted = daemon.call(QStringLiteral("PasteIndex"), number);
        if (!pasted.isValid()) {
            std::fprintf(stderr, "%s\n", qPrintable(pasted.error().message()));
            return 1;
        }
        if (!pasted.value()) {
            std::fprintf(stderr, "There is no history item %d.\n", number);
            return 1;
        }
        return 0;
    } else if (parser.isSet(clearAllOption)) {
        reply = daemon.call(QStringLiteral("ClearAll"));
    } else if (parser.isSet(quitOption)) {
        reply = daemon.call(QStringLiteral("Quit"));
    } else {
        parser.showHelp(2);
    }

    if (reply.type() == QDBusMessage::ErrorMessage) {
        std::fprintf(stderr, "%s\n", qPrintable(reply.errorMessage()));
        return 1;
    }
    return 0;
}

} // namespace

int main(int argc, char *argv[])
{
    // Any argument means "send a command to the running instance"; that
    // does not need a GUI connection.
    if (argc > 1) {
        QCoreApplication app(argc, argv);
        setAppInfo();
        return runClient();
    }

    // GNOME on Wayland only lets the focused window read the clipboard.
    // Running through XWayland lets us see every copy, so default to xcb.
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "xcb");

    QApplication app(argc, argv);
    setAppInfo();

    // The app lives in the background; hiding the popup must not quit it.
    QApplication::setQuitOnLastWindowClosed(false);

    // ~/.local/share/clipboard-linux/history.db
    const QString dbPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QStringLiteral("/clipboard-linux/history.db");
    Daemon daemon(dbPath);
    if (!daemon.start())
        return 1;

    return QApplication::exec();
}
