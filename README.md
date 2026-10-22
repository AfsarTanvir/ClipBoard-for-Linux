# ClipBoard for Linux

A clipboard history manager for Linux (GNOME on Wayland), modelled on the Windows **Win + V** clipboard.

## Features

- Clipboard history that survives reboots (SQLite)
- **Super + V** opens the history popup
- Navigate with the mouse wheel or arrow keys; paste with **Enter** or a click
- **1–9** in the popup pastes that item immediately
- **Ctrl + Alt + 1 … 9** pastes item 1–9 without opening the popup
- Pin items so they are never removed
- Delete a single item, or clear all unpinned items
- Duplicate copies are merged and moved to the top

## Tech stack

C++20 · Qt 6 (Widgets, SQL, D-Bus) · SQLite · Linux uinput

## Status

Work in progress.
