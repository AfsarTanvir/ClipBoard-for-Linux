#pragma once

class QWidget;

namespace X11Activation {

// Asks the window manager to focus window. Qt's activateWindow() sends the
// timestamp of the last X event we saw, which is usually older than the
// user's last click elsewhere, so GNOME's focus-stealing prevention refuses
// and only flashes the window. A request marked as coming from a pager
// (taskbar) with no timestamp is honoured. Does nothing outside X11/XWayland.
void activate(QWidget *window);

} // namespace X11Activation
