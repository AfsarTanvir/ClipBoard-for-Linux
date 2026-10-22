#include "HistoryStore.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>

namespace {

QString hashOf(const QString &text)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha256).toHex());
}

} // namespace

HistoryStore::HistoryStore(const QString &dbPath, int maxUnpinned, QObject *parent)
    : QObject(parent)
    , m_dbPath(dbPath)
    , m_connectionName(QStringLiteral("history-") + QUuid::createUuid().toString(QUuid::WithoutBraces))
    , m_maxUnpinned(qMax(1, maxUnpinned))
{
}

HistoryStore::~HistoryStore()
{
    {
        QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
        if (db.isOpen())
            db.close();
    }
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool HistoryStore::open()
{
    QDir().mkpath(QFileInfo(m_dbPath).absolutePath());

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    db.setDatabaseName(m_dbPath);
    if (!db.open()) {
        qWarning("HistoryStore: cannot open %s: %s", qPrintable(m_dbPath),
                 qPrintable(db.lastError().text()));
        return false;
    }

    return exec(QStringLiteral("PRAGMA journal_mode=WAL"))
        && exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS items ("
            "  id         INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  content    TEXT    NOT NULL,"
            "  hash       TEXT    NOT NULL UNIQUE,"
            "  pinned     INTEGER NOT NULL DEFAULT 0,"
            "  created_at INTEGER NOT NULL"
            ")"))
        && exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS items_created_at ON items(created_at DESC)"));
}

QList<ClipItem> HistoryStore::items() const
{
    QList<ClipItem> result;
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.exec(QStringLiteral(
        "SELECT id, content, pinned, created_at FROM items ORDER BY created_at DESC, id DESC"));
    while (query.next()) {
        ClipItem item;
        item.id = query.value(0).toLongLong();
        item.text = query.value(1).toString();
        item.pinned = query.value(2).toBool();
        item.createdAt = query.value(3).toLongLong();
        result.append(item);
    }
    return result;
}

void HistoryStore::addText(const QString &text)
{
    if (text.trimmed().isEmpty() || text.size() > MaxTextLength)
        return;

    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral(
        "INSERT INTO items (content, hash, pinned, created_at) VALUES (?, ?, 0, ?) "
        "ON CONFLICT(hash) DO UPDATE SET created_at = excluded.created_at"));
    query.addBindValue(text);
    query.addBindValue(hashOf(text));
    query.addBindValue(QDateTime::currentMSecsSinceEpoch());
    if (!query.exec()) {
        qWarning("HistoryStore: insert failed: %s", qPrintable(query.lastError().text()));
        return;
    }

    prune();
    emit changed();
}

void HistoryStore::setPinned(qint64 id, bool pinned)
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral("UPDATE items SET pinned = ? WHERE id = ?"));
    query.addBindValue(pinned ? 1 : 0);
    query.addBindValue(id);
    query.exec();

    // Unpinning can push the unpinned count over the limit.
    prune();
    emit changed();
}

void HistoryStore::remove(qint64 id)
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral("DELETE FROM items WHERE id = ?"));
    query.addBindValue(id);
    query.exec();
    emit changed();
}

void HistoryStore::clearUnpinned()
{
    exec(QStringLiteral("DELETE FROM items WHERE pinned = 0"));
    emit changed();
}

void HistoryStore::setMaxUnpinned(int maxUnpinned)
{
    m_maxUnpinned = qMax(1, maxUnpinned);
    prune();
    emit changed();
}

bool HistoryStore::exec(const QString &sql)
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    if (!query.exec(sql)) {
        qWarning("HistoryStore: '%s' failed: %s", qPrintable(sql),
                 qPrintable(query.lastError().text()));
        return false;
    }
    return true;
}

void HistoryStore::prune()
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral(
        "DELETE FROM items WHERE pinned = 0 AND id NOT IN ("
        "  SELECT id FROM items WHERE pinned = 0 ORDER BY created_at DESC, id DESC LIMIT ?)"));
    query.addBindValue(m_maxUnpinned);
    query.exec();
}
