#include <QApplication>

int main(int argc, char *argv[])
{
    // GNOME on Wayland only lets the focused window read the clipboard.
    // Running through XWayland lets us see every copy, so default to xcb.
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "xcb");

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("clipboard-linux"));
    QApplication::setApplicationVersion(QStringLiteral(APP_VERSION));

    // The app lives in the background; hiding the popup must not quit it.
    QApplication::setQuitOnLastWindowClosed(false);

    return QApplication::exec();
}
