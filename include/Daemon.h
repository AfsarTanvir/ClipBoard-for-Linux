#pragma once

#include "ClipboardWatcher.h"
#include "HistoryModel.h"
#include "HistoryStore.h"
#include "PopupWindow.h"

#include <QObject>

#include <optional>

// The background service. Owns the history, the clipboard watcher and the
// popup, and exposes commands on the session D-Bus, which is how GNOME
// keyboard shortcuts and the command line talk to the running instance:
//
//   gdbus call --session --dest io.github.afsartanvir.ClipboardLinux
//       --object-path /ClipboardLinux
//       --method io.github.afsartanvir.ClipboardLinux.Toggle
class Daemon : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.afsartanvir.ClipboardLinux")

public:
    static constexpr const char *ServiceName = "io.github.afsartanvir.ClipboardLinux";
    static constexpr const char *ObjectPath = "/ClipboardLinux";
    static constexpr const char *InterfaceName = "io.github.afsartanvir.ClipboardLinux";

    explicit Daemon(const QString &dbPath, QObject *parent = nullptr);

    // Opens the database and claims the D-Bus name. Returns false if that
    // fails, e.g. because another instance is already running.
    bool start();

    // D-Bus methods (PascalCase is the D-Bus naming convention).
public slots:
    Q_SCRIPTABLE void Toggle();
    Q_SCRIPTABLE void Show();
    // Pastes the item at position number (1 = newest). Returns false if
    // there is no such item.
    Q_SCRIPTABLE bool PasteIndex(int number);
    Q_SCRIPTABLE void ClearAll();
    Q_SCRIPTABLE void Quit();

private:
    void pasteItem(qint64 id);
    void togglePin(qint64 id);
    std::optional<ClipItem> findItem(qint64 id) const;

    HistoryStore m_store;
    ClipboardWatcher m_watcher;
    HistoryModel m_model;
    PopupWindow m_popup;
};
