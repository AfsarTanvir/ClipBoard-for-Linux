#include "ClipboardWatcher.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QMimeData>

namespace {

// KDE/KeePassXC/Bitwarden mark secrets with this MIME type; never store them.
const QString PasswordHintMime = QStringLiteral("x-kde-passwordManagerHint");

} // namespace

ClipboardWatcher::ClipboardWatcher(QObject *parent)
    : QObject(parent)
{
    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged,
            this, &ClipboardWatcher::onClipboardChanged);
}

void ClipboardWatcher::setText(const QString &text)
{
    m_ownText = text;
    QGuiApplication::clipboard()->setText(text, QClipboard::Clipboard);
}

void ClipboardWatcher::onClipboardChanged()
{
    const QMimeData *mime = QGuiApplication::clipboard()->mimeData(QClipboard::Clipboard);
    if (!mime || !mime->hasText())
        return;

    if (mime->hasFormat(PasswordHintMime)
        && mime->data(PasswordHintMime).trimmed() == QByteArrayLiteral("secret"))
        return;

    const QString text = mime->text();

    // Skip the change we caused ourselves in setText().
    const bool isOwn = !m_ownText.isNull() && text == m_ownText;
    m_ownText.clear();
    if (isOwn)
        return;

    emit textCopied(text);
}
