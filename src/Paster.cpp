#include "Paster.h"

#include <QDir>

#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cstring>

namespace {

constexpr int PollIntervalMs = 10;
constexpr int MaxModifierWaitMs = 1500;
// Time for focus to return to the target app after the popup hides and
// for the compositor to pick up the new clipboard owner.
constexpr int SettleDelayMs = 100;
constexpr int KeyGapUs = 4000;

constexpr int ModifierKeys[] = {
    KEY_LEFTCTRL, KEY_RIGHTCTRL, KEY_LEFTALT, KEY_RIGHTALT,
    KEY_LEFTSHIFT, KEY_RIGHTSHIFT, KEY_LEFTMETA, KEY_RIGHTMETA,
};

constexpr bool testBit(const unsigned char *bits, int bit)
{
    return bits[bit / 8] & (1 << (bit % 8));
}

QList<int> keysFor(Paster::Keys keys)
{
    switch (keys) {
    case Paster::Keys::CtrlShiftV:
        return {KEY_LEFTCTRL, KEY_LEFTSHIFT, KEY_V};
    case Paster::Keys::ShiftInsert:
        return {KEY_LEFTSHIFT, KEY_INSERT};
    case Paster::Keys::CtrlV:
        break;
    }
    return {KEY_LEFTCTRL, KEY_V};
}

} // namespace

Paster::Paster(QObject *parent)
    : QObject(parent)
{
    m_pollTimer.setInterval(PollIntervalMs);
    connect(&m_pollTimer, &QTimer::timeout, this, &Paster::pollModifiers);

    m_uinputFd = ::open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (m_uinputFd < 0) {
        qWarning("Paster: cannot open /dev/uinput (%s). Items will be copied but not pasted "
                 "automatically. See the Setup section of the README.", std::strerror(errno));
        return;
    }

    ::ioctl(m_uinputFd, UI_SET_EVBIT, EV_KEY);
    for (int key : {KEY_LEFTCTRL, KEY_LEFTSHIFT, KEY_V, KEY_INSERT})
        ::ioctl(m_uinputFd, UI_SET_KEYBIT, key);

    uinput_setup setup{};
    setup.id.bustype = BUS_VIRTUAL;
    setup.id.vendor = 0x1d6b;
    setup.id.product = 0x0104;
    std::strncpy(setup.name, "clipboard-linux virtual keyboard", UINPUT_MAX_NAME_SIZE - 1);

    // Create the device once at startup: the compositor needs a moment to
    // notice a new input device, which would otherwise delay the first paste.
    if (::ioctl(m_uinputFd, UI_DEV_SETUP, &setup) < 0 || ::ioctl(m_uinputFd, UI_DEV_CREATE) < 0) {
        qWarning("Paster: cannot create virtual keyboard (%s).", std::strerror(errno));
        ::close(m_uinputFd);
        m_uinputFd = -1;
    }
}

Paster::~Paster()
{
    closeKeyboards();
    if (m_uinputFd >= 0) {
        ::ioctl(m_uinputFd, UI_DEV_DESTROY);
        ::close(m_uinputFd);
    }
}

Paster::Keys Paster::keysFromString(const QString &name)
{
    const QString key = name.trimmed().toLower();
    if (key == QLatin1String("ctrl+shift+v"))
        return Keys::CtrlShiftV;
    if (key == QLatin1String("shift+insert"))
        return Keys::ShiftInsert;
    return Keys::CtrlV;
}

void Paster::paste()
{
    if (!isAvailable())
        return;

    closeKeyboards();
    openKeyboards();
    m_waited.start();
    m_pollTimer.start();
}

void Paster::openKeyboards()
{
    // Needs read access to /dev/input/event* (the "input" group). Without it
    // we cannot see held keys and simply fall back to the maximum wait.
    const QStringList devices = QDir(QStringLiteral("/dev/input"))
                                    .entryList({QStringLiteral("event*")}, QDir::System);
    for (const QString &device : devices) {
        const QByteArray path = QByteArrayLiteral("/dev/input/") + device.toLatin1();
        const int fd = ::open(path.constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0)
            continue;

        // Only real keyboards: they have letter keys. Our own virtual device does not.
        unsigned char keyBits[KEY_MAX / 8 + 1] = {};
        if (::ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keyBits)), keyBits) >= 0
            && testBit(keyBits, KEY_A) && testBit(keyBits, KEY_Z)) {
            m_keyboardFds.append(fd);
        } else {
            ::close(fd);
        }
    }
}

void Paster::closeKeyboards()
{
    for (int fd : std::as_const(m_keyboardFds))
        ::close(fd);
    m_keyboardFds.clear();
}

bool Paster::anyModifierHeld() const
{
    if (m_keyboardFds.isEmpty())
        return true; // unknown: keep waiting until the timeout

    for (int fd : m_keyboardFds) {
        unsigned char keyState[KEY_MAX / 8 + 1] = {};
        if (::ioctl(fd, EVIOCGKEY(sizeof(keyState)), keyState) < 0)
            continue;
        for (int key : ModifierKeys) {
            if (testBit(keyState, key))
                return true;
        }
    }
    return false;
}

void Paster::pollModifiers()
{
    if (anyModifierHeld() && m_waited.elapsed() < MaxModifierWaitMs)
        return;

    m_pollTimer.stop();
    closeKeyboards();
    QTimer::singleShot(SettleDelayMs, this, &Paster::sendPasteKeys);
}

void Paster::sendPasteKeys()
{
    const QList<int> keys = keysFor(m_keys);
    for (int key : keys) {
        emitEvent(EV_KEY, key, 1);
        emitEvent(EV_SYN, SYN_REPORT, 0);
        ::usleep(KeyGapUs);
    }
    for (auto it = keys.crbegin(); it != keys.crend(); ++it) {
        emitEvent(EV_KEY, *it, 0);
        emitEvent(EV_SYN, SYN_REPORT, 0);
        ::usleep(KeyGapUs);
    }
}

void Paster::emitEvent(int type, int code, int value)
{
    input_event event{};
    event.type = type;
    event.code = code;
    event.value = value;
    if (::write(m_uinputFd, &event, sizeof(event)) != sizeof(event))
        qWarning("Paster: write to /dev/uinput failed (%s).", std::strerror(errno));
}
