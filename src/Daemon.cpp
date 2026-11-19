#include "Daemon.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QSettings>
#include <QStandardPaths>

#include <functional>

namespace {

constexpr int DefaultMaxItems = 25;

// Reads a setting from ~/.config/clipboard-linux/clipboard-linux.conf.
// Builds made before the organization name was set read
// ~/.config/Unknown Organization/clipboard-linux.conf instead, so a key the
// new file lacks is taken from there: nobody loses settings (or history,
// through a smaller maxItems) when upgrading. Then come Qt's system-wide
// system-wide files (/etc/xdg/...), then defaultValue. A value isValid rejects is
// reported and the next source is tried. Neither file is changed.
QVariant setting(const QString &key, const QVariant &defaultValue,
                 const std::function<bool(const QVariant &)> &isValid = {})
{
    QSettings userSettings;
    userSettings.setFallbacksEnabled(false);
    const QSettings oldSettings(
        QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
            + QStringLiteral("/Unknown Organization/clipboard-linux.conf"),
        QSettings::IniFormat);
    const QSettings systemSettings(QSettings::SystemScope, QCoreApplication::organizationName(),
                                   QCoreApplication::applicationName());
    const QSettings *const sources[] = {&userSettings, &oldSettings, &systemSettings};
    for (const QSettings *source : sources) {
        if (!source->contains(key))
            continue;
        const QVariant value = source->value(key);
        if (!isValid || isValid(value))
            return value;
        // "10,20" is read as a list, whose toString() is empty.
        const QString shown = value.typeId() == QMetaType::QStringList
            ? value.toStringList().join(QLatin1Char(','))
            : value.toString();
        qWarning("Daemon: ignoring invalid %s '%s' in %s", qPrintable(key), qPrintable(shown),
                 qPrintable(source->fileName()));
    }
    return defaultValue;
}

// history/maxItems must be a number; a typo must not shrink the history, so
// anything else is skipped. Numbers below 1 mean 1 (HistoryStore clamps).
int maxItemsSetting()
{
    return setting(QStringLiteral("history/maxItems"), DefaultMaxItems, [](const QVariant &value) {
               bool ok = false;
               value.toInt(&ok);
               return ok;
           })
        .toInt();
}

} // namespace

Daemon::Daemon(const QString &dbPath, QObject *parent)
    : QObject(parent)
    , m_store(dbPath, maxItemsSetting())
    , m_model(&m_store)
    , m_popup(&m_model)
{
    m_autoPaste = setting(QStringLiteral("paste/auto"), true).toBool();
    m_paster.setKeys(Paster::keysFromString(
        setting(QStringLiteral("paste/keys"), QStringLiteral("ctrl+v")).toString()));

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
        qWarning("Daemon: cannot connect to the session D-Bus.");
        return false;
    }

    // Claiming the name first doubles as the single-instance check.
    if (!bus.registerService(QString::fromLatin1(ServiceName))) {
        qWarning("Daemon: clipboard-linux is already running.");
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
    if (number < 1)
        return false;
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
