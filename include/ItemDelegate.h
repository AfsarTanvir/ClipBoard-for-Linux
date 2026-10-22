#pragma once

#include <QStyledItemDelegate>

// Paints one history entry as a card: number badge (1-9), a text preview of
// up to three lines, and pin / delete buttons on the right.
class ItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    // Hit areas inside an item's visual rect, used by the popup for clicks.
    static QRect pinButtonRect(const QRect &itemRect);
    static QRect deleteButtonRect(const QRect &itemRect);

    static constexpr int PreviewLines = 3;
};
