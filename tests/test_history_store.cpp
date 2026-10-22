#include "HistoryStore.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>

class TestHistoryStore : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        QVERIFY(m_dir.isValid());
        m_dbPath = m_dir.filePath(QStringLiteral("history-%1.db").arg(++m_counter));
    }

    void addsNewestFirst()
    {
        HistoryStore store(m_dbPath);
        QVERIFY(store.open());
        store.addText(QStringLiteral("first"));
        QThread::msleep(2);
        store.addText(QStringLiteral("second"));

        const auto items = store.items();
        QCOMPARE(items.size(), 2);
        QCOMPARE(items[0].text, QStringLiteral("second"));
        QCOMPARE(items[1].text, QStringLiteral("first"));
    }

    void ignoresBlankText()
    {
        HistoryStore store(m_dbPath);
        QVERIFY(store.open());
        store.addText(QString());
        store.addText(QStringLiteral("   \n\t"));
        QVERIFY(store.items().isEmpty());
    }

    void duplicateMovesToTop()
    {
        HistoryStore store(m_dbPath);
        QVERIFY(store.open());
        store.addText(QStringLiteral("a"));
        QThread::msleep(2);
        store.addText(QStringLiteral("b"));
        QThread::msleep(2);
        store.addText(QStringLiteral("a"));

        const auto items = store.items();
        QCOMPARE(items.size(), 2);
        QCOMPARE(items[0].text, QStringLiteral("a"));
        QCOMPARE(items[1].text, QStringLiteral("b"));
    }

    void capsUnpinnedButKeepsPinned()
    {
        HistoryStore store(m_dbPath, 3);
        QVERIFY(store.open());
        store.addText(QStringLiteral("keep me"));
        store.setPinned(store.items().first().id, true);

        for (int i = 0; i < 5; ++i) {
            QThread::msleep(2);
            store.addText(QStringLiteral("item %1").arg(i));
        }

        const auto items = store.items();
        QCOMPARE(items.size(), 4); // 3 unpinned + 1 pinned
        QCOMPARE(items[0].text, QStringLiteral("item 4"));
        QCOMPARE(items.last().text, QStringLiteral("keep me"));
        QVERIFY(items.last().pinned);
    }

    void clearUnpinnedKeepsPinned()
    {
        HistoryStore store(m_dbPath);
        QVERIFY(store.open());
        store.addText(QStringLiteral("pinned"));
        store.setPinned(store.items().first().id, true);
        store.addText(QStringLiteral("loose"));

        store.clearUnpinned();

        const auto items = store.items();
        QCOMPARE(items.size(), 1);
        QCOMPARE(items[0].text, QStringLiteral("pinned"));
    }

    void removeDeletesOneItem()
    {
        HistoryStore store(m_dbPath);
        QVERIFY(store.open());
        store.addText(QStringLiteral("x"));
        store.addText(QStringLiteral("y"));
        const qint64 id = store.items().first().id;

        store.remove(id);

        const auto items = store.items();
        QCOMPARE(items.size(), 1);
        QVERIFY(items[0].id != id);
    }

    void survivesReopen()
    {
        {
            HistoryStore store(m_dbPath);
            QVERIFY(store.open());
            store.addText(QStringLiteral("persisted"));
            store.setPinned(store.items().first().id, true);
        }
        HistoryStore reopened(m_dbPath);
        QVERIFY(reopened.open());
        const auto items = reopened.items();
        QCOMPARE(items.size(), 1);
        QCOMPARE(items[0].text, QStringLiteral("persisted"));
        QVERIFY(items[0].pinned);
    }

    void emitsChanged()
    {
        HistoryStore store(m_dbPath);
        QVERIFY(store.open());
        QSignalSpy spy(&store, &HistoryStore::changed);
        store.addText(QStringLiteral("hello"));
        store.clearUnpinned();
        QCOMPARE(spy.count(), 2);
    }

private:
    QTemporaryDir m_dir;
    QString m_dbPath;
    int m_counter = 0;
};

QTEST_GUILESS_MAIN(TestHistoryStore)
#include "test_history_store.moc"
