#pragma once

#include <QList>
#include <QObject>
#include <QString>

struct ClipItem
{
    qint64 id = 0;
    QString text;
    bool pinned = false;
    qint64 createdAt = 0; // ms since epoch; refreshed when the same text is copied again
};

// Persistent clipboard history backed by SQLite.
//
// Items are kept newest first. Unpinned items are capped at maxUnpinned;
// pinned items are never removed automatically.
class HistoryStore : public QObject
{
    Q_OBJECT

public:
    explicit HistoryStore(const QString &dbPath, int maxUnpinned = 25, QObject *parent = nullptr);
    ~HistoryStore() override;

    bool open();

    QList<ClipItem> items() const;

    // Adds text to the top of the history. Copying text that already exists
    // moves it back to the top instead of storing a duplicate.
    void addText(const QString &text);

    void setPinned(qint64 id, bool pinned);
    void remove(qint64 id);
    void clearUnpinned();

    void setMaxUnpinned(int maxUnpinned);
    int maxUnpinned() const { return m_maxUnpinned; }

    // Texts larger than this are ignored to keep the database small.
    static constexpr int MaxTextLength = 1'000'000;

signals:
    void changed();

private:
    bool exec(const QString &sql);
    void prune();

    QString m_dbPath;
    QString m_connectionName;
    int m_maxUnpinned;
};
