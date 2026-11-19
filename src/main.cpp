#include "Daemon.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>

#include <cstdio>

namespace {

void setAppInfo()
{
    // QSettings builds its path from both names. Without an organization
    // name Qt uses "Unknown Organization", so set it to get
    // ~/.config/clipboard-linux/clipboard-linux.conf.
    QCoreApplication::setOrganizationName(QStringLiteral("clipboard-linux"));
    QCoreApplication::setApplicationName(QStringLiteral("clipboard-linux"));
    QCoreApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
}

// Versions before the organization name was set read their settings from
// ~/.config/Unknown Organization/clipboard-linux.conf. Move them to the new
// location, so nobody loses settings (and history, through a smaller
// maxItems) when upgrading. Moving, not copying, makes this happen only once.
void migrateOldSettings()
{
    const QString newPath = QSettings().fileName();
    const QString oldDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/Unknown Organization");
    const QString oldPath = oldDir + QStringLiteral("/clipboard-linux.conf");
    if (!QFile::exists(oldPath))
        return;

    if (QFile::exists(newPath)) {
        // The new file was never read, the old one was in effect: copy the
        // old values over it so nothing the user relied on changes, then set
        // the old file aside. QSettings drops comments when it rewrites the
        // file, so keep the user's version as .bak first.
        const QString newBackup = newPath + QStringLiteral(".bak");
        QFile::remove(newBackup);
        if (!QFile::copy(newPath, newBackup)) {
            qWarning("Settings: cannot back up %s, not merging old settings %s",
                     qPrintable(newPath), qPrintable(oldPath));
            return;
        }
        {
            const QSettings oldSettings(oldPath, QSettings::IniFormat);
            QSettings settings;
            const QStringList keys = oldSettings.allKeys();
            for (const QString &key : keys)
                settings.setValue(key, oldSettings.value(key));
            settings.sync();
            if (settings.status() != QSettings::NoError) {
                qWarning("Settings: cannot merge old settings %s into %s",
                         qPrintable(oldPath), qPrintable(newPath));
                return;
            }
        }
        // The old file must not be merged again on the next start, or it
        // would undo the user's later edits: rename it, or else delete it.
        const QString oldBackup = oldPath + QStringLiteral(".old");
        QFile::remove(oldBackup);
        if (!QFile::rename(oldPath, oldBackup) && !QFile::remove(oldPath)) {
            qWarning("Settings: merged %s but cannot rename or delete it; it will be merged "
                     "again next time", qPrintable(oldPath));
            return;
        }
        qInfo("Settings: merged old settings %s into %s (previous file kept as %s)",
              qPrintable(oldPath), qPrintable(newPath), qPrintable(newBackup));
        return;
    }

    QDir().mkpath(QFileInfo(newPath).absolutePath());
    if (!QFile::rename(oldPath, newPath)) {
        qWarning("Settings: cannot move old settings from %s to %s", qPrintable(oldPath),
                 qPrintable(newPath));
        return;
    }
    QDir().rmdir(oldDir); // only succeeds if nothing else is in it
    qInfo("Settings: moved settings from %s to %s", qPrintable(oldPath), qPrintable(newPath));
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

    // Check the usage before contacting the daemon, so a typo is reported
    // as such even when the daemon is not running.
    int number = 0;
    if (parser.isSet(pasteOption)) {
        bool ok = false;
        number = parser.value(pasteOption).toInt(&ok);
        if (!ok || number < 1) {
            std::fprintf(stderr, "--paste needs a number of 1 or more.\n");
            return 2;
        }
    } else if (!parser.isSet(clearAllOption) && !parser.isSet(quitOption)) {
        parser.showHelp(2);
    }

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
    } else {
        reply = daemon.call(QStringLiteral("Quit"));
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
    migrateOldSettings();

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
