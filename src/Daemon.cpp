#include "Daemon.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QSettings>

Daemon::Daemon(const QString &dbPath, QObject *parent)
    : QObject(parent)
    , m_store(dbPath, QSettings().value(QStringLiteral("history/maxItems"), 25).toInt())
{
    connect(&m_watcher, &ClipboardWatcher::textCopied, &m_store, &HistoryStore::addText);
}

bool Daemon::start()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qWarning("Cannot connect to the session D-Bus.");
        return false;
    }

    // Claiming the name first doubles as the single-instance check.
    if (!bus.registerService(QString::fromLatin1(ServiceName))) {
        qWarning("clipboard-linux is already running.");
        return false;
    }

    if (!m_store.open())
        return false;

    return bus.registerObject(QString::fromLatin1(ObjectPath), this,
                              QDBusConnection::ExportScriptableSlots);
}

bool Daemon::PasteIndex(int number)
{
    const QList<ClipItem> items = m_store.items();
    if (number < 1 || number > items.size())
        return false;
    pasteItem(items[number - 1].id);
    return true;
}

void Daemon::ClearAll()
{
    m_store.clearUnpinned();
}

void Daemon::Quit()
{
    QCoreApplication::quit();
}

void Daemon::pasteItem(qint64 id)
{
    for (const ClipItem &item : m_store.items()) {
        if (item.id == id) {
            m_watcher.setText(item.text);
            return;
        }
    }
}
