#pragma once

#include <QObject>
#include <QString>

// Watches the system clipboard and reports every new text copy.
class ClipboardWatcher : public QObject
{
    Q_OBJECT

public:
    explicit ClipboardWatcher(QObject *parent = nullptr);

    // Puts text on the clipboard without reporting it as a new copy, so
    // pasting an old item does not reorder the history.
    void setText(const QString &text);

signals:
    void textCopied(const QString &text);

private slots:
    void onClipboardChanged();

private:
    QString m_ownText;
};
