#pragma once

#include "HistoryStore.h"

#include <QAbstractListModel>

// Read-only list model over HistoryStore, in the same order the popup shows.
class HistoryModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        TextRole,
        PinnedRole,
        CreatedAtRole,
    };

    explicit HistoryModel(HistoryStore *store, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Returns nullptr when row is out of range.
    const ClipItem *itemAt(int row) const;

public slots:
    void reload();

private:
    HistoryStore *m_store;
    QList<ClipItem> m_items;
};
