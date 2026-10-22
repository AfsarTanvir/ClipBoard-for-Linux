#include "X11Activation.h"

#include <QGuiApplication>
#include <QWidget>

#include <xcb/xcb.h>

#include <cstring>
#include <memory>

namespace X11Activation {

void activate(QWidget *window)
{
    const auto *x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
    if (!x11 || !window)
        return;
    xcb_connection_t *connection = x11->connection();

    static const xcb_atom_t activeWindowAtom = [connection] {
        constexpr char name[] = "_NET_ACTIVE_WINDOW";
        const std::unique_ptr<xcb_intern_atom_reply_t, decltype(&std::free)> reply(
            xcb_intern_atom_reply(connection,
                                  xcb_intern_atom(connection, false, std::strlen(name), name),
                                  nullptr),
            &std::free);
        return reply ? reply->atom : xcb_atom_t(XCB_ATOM_NONE);
    }();
    if (activeWindowAtom == XCB_ATOM_NONE)
        return;

    const xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(connection)).data;

    xcb_client_message_event_t event{};
    event.response_type = XCB_CLIENT_MESSAGE;
    event.format = 32;
    event.window = xcb_window_t(window->winId());
    event.type = activeWindowAtom;
    event.data.data32[0] = 2;                 // source indication: pager
    event.data.data32[1] = XCB_CURRENT_TIME;  // no timestamp: skip the age check
    event.data.data32[2] = 0;                 // we have no currently active window

    xcb_send_event(connection, false, screen->root,
                   XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY,
                   reinterpret_cast<const char *>(&event));
    xcb_flush(connection);
}

} // namespace X11Activation
