#include "HistoryModel.h"

HistoryModel::HistoryModel(HistoryStore *store, QObject *parent)
    : QAbstractListModel(parent)
    , m_store(store)
{
    // The store may not be open yet; the owner calls reload() once it is.
    connect(m_store, &HistoryStore::changed, this, &HistoryModel::reload);
}

int HistoryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_items.size());
}

QVariant HistoryModel::data(const QModelIndex &index, int role) const
{
    const ClipItem *item = itemAt(index.row());
    if (!index.isValid() || !item)
        return {};

    switch (role) {
    case Qt::DisplayRole:
    case TextRole:
        return item->text;
    case IdRole:
        return item->id;
    case PinnedRole:
        return item->pinned;
    case CreatedAtRole:
        return item->createdAt;
    default:
        return {};
    }
}

QHash<int, QByteArray> HistoryModel::roleNames() const
{
    return {
        {IdRole, "id"},
        {TextRole, "text"},
        {PinnedRole, "pinned"},
        {CreatedAtRole, "createdAt"},
    };
}

const ClipItem *HistoryModel::itemAt(int row) const
{
    if (row < 0 || row >= m_items.size())
        return nullptr;
    return &m_items[row];
}

void HistoryModel::reload()
{
    beginResetModel();
    m_items = m_store->items();
    endResetModel();
}
