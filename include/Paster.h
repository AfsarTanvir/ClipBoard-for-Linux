#pragma once

#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QString>
#include <QTimer>

// Presses the paste shortcut in the focused application.
//
// Wayland does not let apps send keystrokes to other windows, so Paster
// creates a virtual keyboard through the kernel's /dev/uinput instead. That
// needs write access to /dev/uinput (see packaging/60-clipboard-linux-uinput.rules).
class Paster : public QObject
{
    Q_OBJECT

public:
    enum class Keys {
        CtrlV,       // most apps
        CtrlShiftV,  // terminals
        ShiftInsert, // works in most apps and terminals
    };

    explicit Paster(QObject *parent = nullptr);
    ~Paster() override;

    bool isAvailable() const { return m_uinputFd >= 0; }

    void setKeys(Keys keys) { m_keys = keys; }
    static Keys keysFromString(const QString &name);

    // Waits until the user has released Ctrl/Alt/Shift/Super (they are
    // usually still held from the shortcut that triggered the paste), then
    // presses the paste keys.
    void paste();

private:
    void openKeyboards();
    void closeKeyboards();
    bool anyModifierHeld() const;
    void pollModifiers();
    void sendPasteKeys();
    void emitEvent(int type, int code, int value);

    int m_uinputFd = -1;
    Keys m_keys = Keys::CtrlV;
    QList<int> m_keyboardFds; // physical keyboards, opened only while waiting
    QTimer m_pollTimer;
    QElapsedTimer m_waited;
};
