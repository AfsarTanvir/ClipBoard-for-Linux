#include "ClipboardWatcher.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QMimeData>

#include <utility>

namespace {

// KDE/KeePassXC/Bitwarden mark secrets with this MIME type; never store them.
const QString PasswordHintMime = QStringLiteral("x-kde-passwordManagerHint");

// GNOME Files adds this format when files themselves are copied, as opposed to
// a path copied as text. (KDE's application/x-kde4-urilist is not used: KIO
// adds it to copied web links as well.)
const QString GnomeCopiedFilesMime = QStringLiteral("x-special/gnome-copied-files");

// True when files were copied in GNOME Files (Ctrl+C), local or not (smb://,
// sftp://, trash:///).
bool isFileCopy(const QMimeData &mime)
{
    return mime.hasUrls() && mime.hasFormat(GnomeCopiedFilesMime);
}

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

    // The marker set by setText() applies only to the next text change, even
    // if a filter below skips it, so take it now.
    const QString ownText = std::exchange(m_ownText, QString());

    if (mime->hasFormat(PasswordHintMime)
        && mime->data(PasswordHintMime).trimmed() == QByteArrayLiteral("secret"))
        return;

    // Copying files in a file manager also offers their paths as text.
    // Like Windows, keep only real text: skip file copies.
    if (isFileCopy(*mime))
        return;

    const QString text = mime->text();

    // Skip the change we caused ourselves in setText().
    if (!ownText.isNull() && text == ownText)
        return;

    emit textCopied(text);
}
