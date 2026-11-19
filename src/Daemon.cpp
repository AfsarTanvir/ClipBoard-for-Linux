#include "Daemon.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>

#include <functional>
#include <limits>

namespace {

constexpr int DefaultMaxItems = 25;

// Where a setting is looked up; the first file that has the key decides:
//   ~/.config/clipboard-linux/clipboard-linux.conf      the documented file
//   ~/.config/Unknown Organization/clipboard-linux.conf what builds made before
//       the organization name was set read, so upgrading keeps those settings
//   ~/.config/clipboard-linux.conf                      Qt's organization-wide file
//   /etc/xdg/clipboard-linux/...                        system-wide files
// If isValid rejects that value it is reported and `rejected` is set, so the
// caller can pick a safe fallback; lower files are not consulted, so a typo
// can't bring back an older value. No file is ever changed.
struct Lookup
{
    QVariant value;
    bool found = false;
    bool rejected = false; // the deciding file's value was refused by isValid
};

Lookup lookup(const QString &key, const std::function<bool(const QVariant &)> &isValid = {})
{
    const QString org = QCoreApplication::organizationName();
    const QString app = QCoreApplication::applicationName();
    QSettings userApp(QSettings::UserScope, org, app);
    userApp.setFallbacksEnabled(false);
    const QSettings oldFile(
        QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
            + QStringLiteral("/Unknown Organization/clipboard-linux.conf"),
        QSettings::IniFormat);
    QSettings userOrg(QSettings::UserScope, org);
    userOrg.setFallbacksEnabled(false);
    const QSettings system(QSettings::SystemScope, org, app);

    Lookup result;
    const QSettings *const sources[] = {&userApp, &oldFile, &userOrg, &system};
    for (const QSettings *source : sources) {
        if (!source->contains(key))
            continue;
        const QVariant value = source->value(key);
        if (!isValid || isValid(value)) {
            result.value = value;
            result.found = true;
            return result;
        }
        result.rejected = true;
        // "10,20" is read as a list, whose toString() is empty.
        const QString shown = value.typeId() == QMetaType::QStringList
            ? value.toStringList().join(QLatin1Char(','))
            : value.toString();
        qWarning("Daemon: ignoring invalid %s '%s' in %s", qPrintable(key), qPrintable(shown),
                 qPrintable(source->fileName()));
        return result;
    }
    return result;
}

QVariant setting(const QString &key, const QVariant &defaultValue)
{
    const Lookup result = lookup(key);
    return result.found ? result.value : defaultValue;
}

// history/maxItems: numbers below 1 mean 1 (HistoryStore clamps; the
// documented minimum). A value that isn't a number must never prune the
// history, so it keeps it all.
int maxItemsSetting()
{
    const Lookup result = lookup(QStringLiteral("history/maxItems"), [](const QVariant &value) {
        bool ok = false;
        value.toInt(&ok);
        return ok;
    });
    if (result.found)
        return result.value.toInt();
    if (result.rejected) {
        qWarning("Daemon: keeping all history until history/maxItems is fixed");
        return std::numeric_limits<int>::max();
    }
    return DefaultMaxItems;
}

// paste/auto: true/false, 1/0, yes/no or on/off. Pasting sends keystrokes to
// another app, so a value we can't read means off, never on.
bool autoPasteSetting()
{
    static const QStringList on = {QStringLiteral("true"), QStringLiteral("1"),
                                   QStringLiteral("yes"), QStringLiteral("on")};
    static const QStringList off = {QStringLiteral("false"), QStringLiteral("0"),
                                    QStringLiteral("no"), QStringLiteral("off")};
    const auto text = [](const QVariant &value) { return value.toString().trimmed().toLower(); };
    const Lookup result = lookup(QStringLiteral("paste/auto"), [&](const QVariant &value) {
        return on.contains(text(value)) || off.contains(text(value));
    });
    if (result.found)
        return on.contains(text(result.value));
    if (result.rejected) {
        qWarning("Daemon: auto-paste is off until paste/auto is fixed");
        return false;
    }
    return true;
}

} // namespace

Daemon::Daemon(const QString &dbPath, QObject *parent)
    : QObject(parent)
    , m_store(dbPath, maxItemsSetting())
    , m_model(&m_store)
    , m_popup(&m_model)
{
    m_autoPaste = autoPasteSetting();
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
