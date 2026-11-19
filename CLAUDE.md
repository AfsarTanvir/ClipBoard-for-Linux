# CLAUDE.md

Guidance for Claude Code when working in this repository.

## What this is

`clipboard-linux` is a clipboard history manager for GNOME on Wayland, modelled on the
Windows **Win + V** popup. One binary is both the background service (daemon) and its
command-line client. It is written in C++20 with Qt 6 (Widgets, SQL, D-Bus), stores history
in SQLite, and uses libxcb and the kernel's `/dev/uinput` to work around Wayland's limits.

Target platform: Ubuntu 26.04, GNOME 50, Wayland. User-facing setup, usage and settings
are in [README.md](README.md); this file covers how the code works and how to change it.

## Commands

Dependencies (Debian/Ubuntu):

```bash
sudo apt install build-essential cmake pkg-config qt6-base-dev libqt6sql6-sqlite libxcb1-dev
```

Build, test, run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug     # configure (BUILD_TESTING is ON by default)
cmake --build build -j                           # build app + tests
ctest --test-dir build --output-on-failure       # run all tests
./build/test_history_store duplicateMovesToTop   # run one QtTest function
./build/clipboard-linux                          # run the daemon in the foreground
```

Talking to a running daemon:

```bash
./build/clipboard-linux --paste 2      # paste history item 2 (1 = newest)
./build/clipboard-linux --clear-all    # remove everything except pinned items
./build/clipboard-linux --quit         # stop the daemon
gdbus call --session --dest io.github.afsartanvir.ClipboardLinux \
  --object-path /ClipboardLinux --method io.github.afsartanvir.ClipboardLinux.Toggle
```

Only one daemon can run at a time (see "Single instance" below). If an installed copy is
already running (autostart, `~/.local/bin`, or a system package), stop it with
`clipboard-linux --quit` before starting a build from `./build/`.

Requires Qt 6.5 or newer (`find_package(Qt6 6.5 …)`). The version string comes from
`project(VERSION …)` in CMakeLists.txt via the `APP_VERSION` define (`--version`).

There is no formatter or linter config; the compiler flags `-Wall -Wextra -Wpedantic` are
the lint, on both the app and the test target. Keep new code warning-free.

## Layout

```
CMakeLists.txt            app target, test target, install rule
include/                  all headers (one class per file)
src/                      matching .cpp files + main.cpp
tests/                    QtTest unit tests (HistoryStore only)
packaging/                udev rule, autostart .desktop, GNOME shortcut installer (Python)
install.sh / uninstall.sh per-user install into ~/.local, uses sudo for the udev rule
doc/                      local notes, gitignored, never committed
```

Every new `.h`/`.cpp` must be added to `add_executable(clipboard-linux …)` in
[CMakeLists.txt](CMakeLists.txt) by hand (no globbing). The test target compiles
`src/HistoryStore.cpp` directly and links only `Qt6::Sql` and `Qt6::Test`, so if
`HistoryStore` gains new dependencies, update `test_history_store` too.

## How it works

### Process start ([src/main.cpp](src/main.cpp))

- **Any command-line argument → client mode.** A `QCoreApplication` (no GUI) parses
  `--paste <n>`, `--clear-all`, `--quit` (plus `--help`, `--version`), makes one D-Bus call to
  the running daemon and exits. The usage is checked before the daemon is contacted. Exit
  codes: `0` ok (also `--help`/`--version`); `1` unknown option (rejected by
  `QCommandLineParser::process`), daemon not running, call failed, or no such item; `2`
  `--paste` isn't a number of 1 or more, or no action was given (help is printed).
- **No arguments → daemon mode.** Sets `QT_QPA_PLATFORM=xcb` unless the user already set it,
  creates a `QApplication` with `setQuitOnLastWindowClosed(false)` (hiding the popup must not
  quit), and starts `Daemon` with the database at
  `~/.local/share/clipboard-linux/history.db` (`QStandardPaths::GenericDataLocation`).
- `setAppInfo()` sets the organization and application name (both `clipboard-linux`) and the
  version. `QSettings` builds its path from the two names, so it must run before any
  `QSettings` object is created. Without the organization name Qt would use
  "Unknown Organization" and read the wrong file. Earlier builds did exactly that (see
  Settings for how the old file is still honoured).

### Data flow

```
copy in any app
  → QClipboard::dataChanged → ClipboardWatcher::onClipboardChanged
  → textCopied(text) → HistoryStore::addText → changed()
  → HistoryModel::reload (full model reset) → PopupWindow list repaints

Super+V (GNOME custom shortcut) → gdbus → Daemon::Toggle → PopupWindow::toggle
user picks an item → PopupWindow hides, emits pasteRequested(id)
  → Daemon::pasteItem → ClipboardWatcher::setText (clipboard now holds the item)
  → Paster::paste (waits for modifiers to be released, then presses Ctrl+V via uinput)
```

### Classes

**`Daemon`** ([include/Daemon.h](include/Daemon.h), [src/Daemon.cpp](src/Daemon.cpp))
The background service. Owns, by value, the `HistoryStore`, `ClipboardWatcher`,
`HistoryModel`, `PopupWindow` and `Paster`, and wires their signals together in its
constructor. Reads settings there too. `start()` connects to the session bus, claims the
service name, opens the database, reloads the model and exports itself. Private helpers:
`pasteItem(id)`, `togglePin(id)` and `findItem(id)` (a linear search over the model's rows).

**`ClipboardWatcher`** ([src/ClipboardWatcher.cpp](src/ClipboardWatcher.cpp))
Listens to `QClipboard::dataChanged` for the `Clipboard` mode (not the primary selection) and
emits `textCopied` when `mime->hasText()`, so pure images are ignored. It drops copies whose
`x-kde-passwordManagerHint` MIME data is `secret` (KeePassXC, Bitwarden, KDE), and file copies
from GNOME Files (`isFileCopy`: URLs plus `x-special/gnome-copied-files`, local or remote,
because Ctrl+C in Files also offers the paths as text). KDE's `application/x-kde4-urilist` is
deliberately not used: KIO adds it to copied web links too. A path copied as plain text, or a
web link, is kept. `setText()`
remembers the text in `m_ownText` so the next change it causes is not reported; this is what
keeps pasting an old item from moving it to the top of the history.

**`HistoryStore`** ([src/HistoryStore.cpp](src/HistoryStore.cpp))
SQLite persistence through Qt SQL. Each instance opens its own named connection
(`history-<uuid>`), so several stores (e.g. in tests) can coexist. Emits `changed()` after every
mutation. Rules:
- `addText` ignores blank text and text longer than `MaxTextLength` (1,000,000 characters).
- Duplicates are detected by a SHA-256 hash of the UTF-8 text (`hash` column, `UNIQUE`); copying
  existing text updates its `created_at` instead of inserting (`ON CONFLICT(hash) DO UPDATE`).
- `prune()` keeps the newest `maxUnpinned` unpinned items (minimum 1) and runs after an add,
  after `setPinned`, and in `setMaxUnpinned`. Pinned items are never pruned.
- `clearUnpinned()` deletes every unpinned row; `remove(id)` deletes one row, pinned or not.
- `items()` returns everything ordered `created_at DESC, id DESC`. Pinned items are **not**
  sorted to the top; they keep their place by time.

**`HistoryModel`** ([src/HistoryModel.cpp](src/HistoryModel.cpp))
Read-only `QAbstractListModel` caching `QList<ClipItem>`. Roles: `IdRole`, `TextRole`
(also `DisplayRole`), `PinnedRole`, `CreatedAtRole`. `reload()` does a full
`beginResetModel`/`endResetModel` on every store change. The constructor must not query the
store: the store isn't open yet at that point, and `Daemon::start()` calls `reload()` after
`open()`. Row numbers here are the numbers the user sees (row 0 = item 1).

**`PopupWindow`** ([src/PopupWindow.cpp](src/PopupWindow.cpp))
Frameless, always-on-top `Qt::Tool` window (380×480, rounded, painted in `paintEvent`). Holds a
`QListView` in a `QStackedWidget` with an empty-state label, a "Clear all" button and a hint
line. All input goes through `eventFilter` → `handleKey` / `handleClick`:
- Enter or click pastes; 1–9 (without Ctrl/Alt) pastes that row; P toggles pin; Delete removes;
  Ctrl+Delete clears all; Esc hides. Arrow/Page/Home/End go to the list; every other key is
  swallowed.
- It emits `pasteRequested`, `pinToggleRequested`, `removeRequested`, `clearAllRequested` and
  never touches the store itself.
- `activateRow` hides the popup **before** emitting `pasteRequested`, so focus returns to the
  target app before the paste keys are sent.
- It hides itself when it loses activation (click elsewhere), like Windows.
- `showPopup` centres on the primary screen (under XWayland the cursor position is stale), then
  focuses via `X11Activation::activate`, retrying once after 150 ms.
- `m_lastRow` keeps the selection across model resets after a pin or delete.

**`ItemDelegate`** ([src/ItemDelegate.cpp](src/ItemDelegate.cpp))
Paints each row as a card: number badge for rows 1–9, up to `PreviewLines` (3) elided lines of
text, pin and delete buttons. The static `pinButtonRect` / `deleteButtonRect` are the hit areas
`PopupWindow::handleClick` uses, so painting and click handling share one geometry.

**`Theme`** ([include/Theme.h](include/Theme.h))
Header-only dark palette (Windows 11 clipboard look). Use these colours instead of literals.

**`Paster`** ([src/Paster.cpp](src/Paster.cpp))
Creates a virtual keyboard on `/dev/uinput` once at startup (the compositor needs time to notice
a new device). `paste()` opens the physical keyboards under `/dev/input/event*` (those with A
and Z keys), polls every 10 ms until Ctrl/Alt/Shift/Super are all released (at most 1500 ms),
waits 100 ms more, then presses and releases the configured keys. Without read access to
`/dev/input` (the `input` group) it cannot see held keys and just waits the full 1500 ms.
Without write access to `/dev/uinput`, `isAvailable()` is false and items are only copied.
Only `KEY_LEFTCTRL`, `KEY_LEFTSHIFT`, `KEY_V` and `KEY_INSERT` are registered with
`UI_SET_KEYBIT`; a new paste key combination must register its keys there too.

**`X11Activation`** ([src/X11Activation.cpp](src/X11Activation.cpp))
Sends `_NET_ACTIVE_WINDOW` with source "pager" and no timestamp. Qt's `activateWindow()` sends
a stale timestamp that GNOME's focus-stealing prevention rejects. Does nothing outside X11.

## D-Bus API

Service `io.github.afsartanvir.ClipboardLinux`, object `/ClipboardLinux`, interface
`io.github.afsartanvir.ClipboardLinux` (constants in [include/Daemon.h](include/Daemon.h)).
Methods are the `Q_SCRIPTABLE` public slots, exported with `ExportScriptableSlots`, and use
PascalCase by D-Bus convention:

| Method | Args → result | Effect |
|---|---|---|
| `Toggle` | — | show or hide the popup |
| `Show` | — | show the popup |
| `PasteIndex` | `int number` → `bool` | paste item `number` (1 = newest); `false` if there is no such item |
| `ClearAll` | — | remove all unpinned items |
| `Quit` | — | quit the daemon |

The same names are hard-coded in the `DBUS_CALL` string of
[packaging/gnome-shortcuts.py](packaging/gnome-shortcuts.py) and in the client code in
`main.cpp`. Renaming a method or the service breaks the installed GNOME shortcuts, so change
them together (users must re-run `install.sh`).

**Single instance:** `Daemon::start()` claims the service name before opening the database;
if the name is taken, it logs "already running" and `main` exits with 1.

## Database

`~/.local/share/clipboard-linux/history.db`, WAL mode, created in `HistoryStore::open()`:

```sql
CREATE TABLE IF NOT EXISTS items (
  id         INTEGER PRIMARY KEY AUTOINCREMENT,
  content    TEXT    NOT NULL,
  hash       TEXT    NOT NULL UNIQUE,   -- SHA-256 hex of the UTF-8 content
  pinned     INTEGER NOT NULL DEFAULT 0,
  created_at INTEGER NOT NULL           -- ms since epoch; refreshed on re-copy
);
CREATE INDEX IF NOT EXISTS items_created_at ON items(created_at DESC);
```

There is no schema version or migration code; `CREATE TABLE IF NOT EXISTS` will not alter
an existing table. Any schema change needs a migration path for databases users already have
(e.g. `PRAGMA user_version` checks in `open()`) and a test in `survivesReopen` style.

## Settings

`~/.config/clipboard-linux/clipboard-linux.conf`, read once through the `setting()` helper in
the `Daemon` constructor (the app must be restarted after editing). Builds made before the
organization name was set read `~/.config/Unknown Organization/clipboard-linux.conf` instead,
so `setting()` takes any key the new file lacks from that old file; when both have a key, the
new (documented) file wins. Neither file is ever moved or rewritten. Read new settings through
`setting()` too. The path
depends on the organization and application names set in `setAppInfo()`; changing either moves
users' settings. `uninstall.sh --purge` deletes this folder, the history folder, the state
folder and the old `Unknown Organization` settings file (honouring `XDG_CONFIG_HOME`,
`XDG_DATA_HOME`, `XDG_STATE_HOME`). `install.sh` and `uninstall.sh` also put the autostart entry
under `XDG_CONFIG_HOME`.

| Key | Default | Meaning |
|---|---|---|
| `history/maxItems` | `25` | unpinned items to keep (minimum 1) |
| `paste/auto` | `true` | `false` = only put the item on the clipboard |
| `paste/keys` | `ctrl+v` | `ctrl+v`, `ctrl+shift+v` (terminals) or `shift+insert`; anything else falls back to `ctrl+v` |

A new setting should be read in the `Daemon` constructor and documented in the README's
Settings section.

## Why the Wayland workarounds exist (don't "simplify" them away)

Wayland blocks three things a clipboard manager needs:

1. **Reading every copy in the background.** GNOME only lets the focused Wayland window read
   the clipboard, so the daemon runs through XWayland (`QT_QPA_PLATFORM=xcb`), where clipboard
   changes are shared. Switching to the native `wayland` platform stops history recording.
2. **Global shortcuts.** Apps can't grab keys on Wayland, so
   [packaging/gnome-shortcuts.py](packaging/gnome-shortcuts.py) registers GNOME custom shortcuts
   (Super+V → `Toggle`, Ctrl+Alt+1…9 → `PasteIndex n`) that call the daemon with `gdbus`. It
   removes Super+V from GNOME's notification list (`toggle-message-tray`) and adds Super+M only
   if removing Super+V left the list empty. It records what it changed (`removed <Super>v`,
   `added <Super>m`) in `~/.local/state/clipboard-linux/notification-shortcut-changes`
   (`XDG_STATE_HOME`, kept apart from the settings so resetting them doesn't lose it); the file
   is written even when empty. `uninstall` undoes only the recorded changes, so the user's own
   bindings stay. Upgrading over an install that predates the record counts as "removed
   Super+V", as that version's uninstall always restored it (whether it added Super+M is
   unknown, so Super+M is left alone). If the record can't be read, install leaves it untouched
   and uninstall treats it as missing; if it can't be written, the script warns and carries on. With no record at
   all, uninstall puts Super+V back only if our shortcuts were still installed (so running it
   twice, or without installing, changes nothing). GNOME's default list is Super+V and Super+M, so on a
   stock setup install drops Super+V and uninstall puts it back.
   The script keeps the user's own custom shortcuts and is safe to run twice.
3. **Pasting into another app.** Apps can't send keys to other windows, so `Paster` uses a
   `/dev/uinput` virtual keyboard. Access comes from the udev rule
   [packaging/60-clipboard-linux-uinput.rules](packaging/60-clipboard-linux-uinput.rules)
   (`uaccess` tag: only the active session's user).

Plus a focus fix: `X11Activation` exists because GNOME otherwise only flashes the popup instead
of focusing it.

## Testing

- Unit tests cover only `HistoryStore`
  ([tests/test_history_store.cpp](tests/test_history_store.cpp), `QTEST_GUILESS_MAIN`, a fresh
  SQLite file per test in a `QTemporaryDir`). Existing cases: `addsNewestFirst`,
  `ignoresBlankText`, `duplicateMovesToTop`, `capsUnpinnedButKeepsPinned`,
  `clearUnpinnedKeepsPinned`, `removeDeletesOneItem`, `survivesReopen`, `emitsChanged`.
- Tests that depend on ordering sleep a few ms (`QThread::msleep(2)`) between adds, because
  order comes from millisecond timestamps.
- Any change to `HistoryStore` behaviour needs a test here.
- The watcher, popup, D-Bus and paste paths have no automated tests; they need a real GNOME
  Wayland session to check by hand (copy text, Super+V, pick an item, see it pasted). Say so
  when a change can't be verified in the current environment.

## Conventions

- **Style:** Qt conventions: `m_` member prefix, `QStringLiteral` for string literals,
  `QLatin1String` for comparisons, `#pragma once`, helpers in an anonymous namespace at the top
  of the `.cpp`, `constexpr` named constants for sizes and timings (prefer them in new code;
  layout margins and stylesheet pixel values are still literals). 4-space indent, braces on
  their own line for classes and functions.
- **Comments** explain *why* (platform quirks, ordering constraints), not what the code does.
  Each class header starts with a short comment on its purpose.
- **Errors:** log with `qWarning("Class: …")` and degrade gracefully (e.g. no uinput → copy
  only); don't throw.
- **Ownership:** `Daemon` holds its components by value; widgets are parented to Qt objects.
- **Commits:** Conventional Commits with a scope: `feat(store): …`, `fix(ui): …`,
  `feat(paste): …`, `feat(dbus): …`, `feat(watcher): …`, `feat(packaging): …`, `build: …`,
  `docs: …`. Imperative, lower-case subject; the body explains why.
- User-visible changes (keys, settings, CLI options, install steps) also update
  [README.md](README.md).

## Don't

- Don't run `install.sh`, `uninstall.sh` or `packaging/gnome-shortcuts.py` unless asked: they
  use `sudo`, install a udev rule, and change the user's GNOME keybindings.
- Don't delete or rewrite `~/.local/share/clipboard-linux/history.db`; it is the user's real
  clipboard history. Tests use temporary databases.
- Don't store clipboard content marked secret, and don't log clipboard content.
- Don't commit `build/`, `doc/` or `compile_commands.json` (all gitignored).
