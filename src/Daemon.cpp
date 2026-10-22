#include "Daemon.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QSettings>

Daemon::Daemon(const QString &dbPath, QObject *parent)
    : QObject(parent)
    , m_store(dbPath, QSettings().value(QStringLiteral("history/maxItems"), 25).toInt())
    , m_model(&m_store)
    , m_popup(&m_model)
{
    // Settings live in ~/.config/clipboard-linux/clipboard-linux.conf
    QSettings settings;
    m_autoPaste = settings.value(QStringLiteral("paste/auto"), true).toBool();
    m_paster.setKeys(Paster::keysFromString(
        settings.value(QStringLiteral("paste/keys"), QStringLiteral("ctrl+v")).toString()));

    connect(&m_watcher, &ClipboardWatcher::textCopied, &m_store, &HistoryStore::addText);

    connect(&m_popup, &PopupWindow::pasteRequested, this, &Daemon::pasteItem);
    connect(&m_popup, &PopupWindow::pinToggleRequested, this, &Daemon::togglePin);
    connect(&m_popup, &PopupWindow::removeRequested, &m_store, &HistoryStore::remove);
    connect(&m_popup, &PopupWindow::clearAllRequested, this, &Daemon::ClearAll);
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
    m_model.reload();

    return bus.registerObject(QString::fromLatin1(ObjectPath), this,
                              QDBusConnection::ExportScriptableSlots);
}

void Daemon::Toggle()
{
    m_popup.toggle();
}

void Daemon::Show()
{
    m_popup.showPopup();
}

bool Daemon::PasteIndex(int number)
{
    const ClipItem *item = m_model.itemAt(number - 1);
    if (!item)
        return false;
    pasteItem(item->id);
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
    const auto item = findItem(id);
    if (!item)
        return;
    m_watcher.setText(item->text);
    if (m_autoPaste)
        m_paster.paste();
}

void Daemon::togglePin(qint64 id)
{
    if (const auto item = findItem(id))
        m_store.setPinned(id, !item->pinned);
}

std::optional<ClipItem> Daemon::findItem(qint64 id) const
{
    for (int row = 0; row < m_model.rowCount(); ++row) {
        const ClipItem *item = m_model.itemAt(row);
        if (item->id == id)
            return *item;
    }
    return std::nullopt;
}
