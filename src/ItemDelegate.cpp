#include "ItemDelegate.h"

#include "HistoryModel.h"
#include "Theme.h"

#include <QAbstractItemView>
#include <QCursor>
#include <QPainter>
#include <QPainterPath>

namespace {

constexpr int CardMargin = 4;   // gap between cards
constexpr int CardPadding = 10; // inside a card
constexpr int BadgeSize = 20;
constexpr int ButtonSize = 26;

QRect cardRect(const QRect &itemRect)
{
    return itemRect.adjusted(CardMargin, CardMargin, -CardMargin, -CardMargin);
}

// First lines of the text, elided to the available width.
QStringList previewLines(const QString &text, const QFontMetrics &fm, int width)
{
    QStringList lines = text.split(QLatin1Char('\n'));
    while (!lines.isEmpty() && lines.first().trimmed().isEmpty())
        lines.removeFirst();

    const bool truncated = lines.size() > ItemDelegate::PreviewLines;
    lines = lines.mid(0, ItemDelegate::PreviewLines);

    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i];
        line.replace(QLatin1Char('\t'), QStringLiteral("    "));
        line.remove(QLatin1Char('\r'));
        if (truncated && i == lines.size() - 1)
            line += QStringLiteral(" …");
        lines[i] = fm.elidedText(line, Qt::ElideRight, width);
    }
    return lines;
}

void drawPin(QPainter *p, const QRect &r, bool pinned, bool hovered)
{
    const QColor color = pinned ? Theme::Accent : (hovered ? Theme::Text : Theme::TextDim);
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    p->setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap));

    const QPointF c = QRectF(r).center();
    // Head of the pin, a short body and the needle.
    const QRectF head(c.x() - 4, c.y() - 7, 8, 7);
    QPainterPath path;
    path.addRoundedRect(head, 2, 2);
    if (pinned)
        p->fillPath(path, color);
    p->drawPath(path);
    p->drawLine(QPointF(c.x() - 6, c.y()), QPointF(c.x() + 6, c.y()));
    p->drawLine(QPointF(c.x(), c.y()), QPointF(c.x(), c.y() + 7));
    p->restore();
}

void drawCross(QPainter *p, const QRect &r, bool hovered)
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    p->setPen(QPen(hovered ? Theme::Danger : Theme::TextDim, 1.6, Qt::SolidLine, Qt::RoundCap));
    const QPointF c = QRectF(r).center();
    p->drawLine(c + QPointF(-4.5, -4.5), c + QPointF(4.5, 4.5));
    p->drawLine(c + QPointF(-4.5, 4.5), c + QPointF(4.5, -4.5));
    p->restore();
}

} // namespace

QRect ItemDelegate::pinButtonRect(const QRect &itemRect)
{
    const QRect card = cardRect(itemRect);
    return {card.right() - CardPadding / 2 - 2 * ButtonSize, card.top() + CardPadding / 2,
            ButtonSize, ButtonSize};
}

QRect ItemDelegate::deleteButtonRect(const QRect &itemRect)
{
    const QRect card = cardRect(itemRect);
    return {card.right() - CardPadding / 2 - ButtonSize, card.top() + CardPadding / 2,
            ButtonSize, ButtonSize};
}

QSize ItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &) const
{
    const int textHeight = option.fontMetrics.lineSpacing() * PreviewLines;
    return {option.rect.width(), textHeight + 2 * CardPadding + 2 * CardMargin};
}

void ItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                         const QModelIndex &index) const
{
    const bool selected = option.state & QStyle::State_Selected;
    const bool hovered = option.state & QStyle::State_MouseOver;
    const bool pinned = index.data(HistoryModel::PinnedRole).toBool();
    const QString text = index.data(HistoryModel::TextRole).toString();
    const QRect card = cardRect(option.rect);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    // Card background, with an accent bar for the keyboard selection.
    QPainterPath cardPath;
    cardPath.addRoundedRect(QRectF(card), 6, 6);
    painter->fillPath(cardPath, selected ? Theme::CardSelected
                                         : (hovered ? Theme::CardHover : Theme::Card));
    if (selected) {
        painter->setPen(QPen(Theme::Accent, 1));
        painter->drawPath(cardPath);
    }

    int textLeft = card.left() + CardPadding;

    // Number badge for the first nine items: matches the 1-9 / Ctrl+Alt+N keys.
    if (index.row() < 9) {
        const QRect badge(textLeft, card.top() + CardPadding, BadgeSize, BadgeSize);
        QPainterPath badgePath;
        badgePath.addRoundedRect(QRectF(badge), 4, 4);
        painter->fillPath(badgePath, Theme::Badge);
        QFont badgeFont = option.font;
        badgeFont.setBold(true);
        badgeFont.setPointSizeF(badgeFont.pointSizeF() * 0.85);
        painter->setFont(badgeFont);
        painter->setPen(Theme::TextDim);
        painter->drawText(badge, Qt::AlignCenter, QString::number(index.row() + 1));
        textLeft = badge.right() + CardPadding;
    }

    // Buttons are only shown on hover/selection, except a pinned pin which always shows.
    const QRect pinRect = pinButtonRect(option.rect);
    const QRect deleteRect = deleteButtonRect(option.rect);
    QPoint cursor(-1, -1);
    if (const auto *view = qobject_cast<const QAbstractItemView *>(option.widget))
        cursor = view->viewport()->mapFromGlobal(QCursor::pos());
    if (hovered || selected || pinned)
        drawPin(painter, pinRect, pinned, hovered && pinRect.contains(cursor));
    if (hovered || selected)
        drawCross(painter, deleteRect, hovered && deleteRect.contains(cursor));

    // Text preview.
    const QRect textRect(textLeft, card.top() + CardPadding,
                         pinRect.left() - CardPadding / 2 - textLeft,
                         card.height() - 2 * CardPadding);
    painter->setFont(option.font);
    painter->setPen(Theme::Text);
    const QFontMetrics fm(option.font);
    const QStringList lines = previewLines(text, fm, textRect.width());
    int y = textRect.top() + fm.ascent();
    for (const QString &line : lines) {
        painter->drawText(textRect.left(), y, line);
        y += fm.lineSpacing();
    }

    painter->restore();
}
