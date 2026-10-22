#include "PopupWindow.h"

#include "HistoryModel.h"
#include "ItemDelegate.h"
#include "Theme.h"
#include "X11Activation.h"

#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListView>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScreen>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace {

constexpr int PopupWidth = 380;
constexpr int PopupHeight = 480;
constexpr int CornerRadius = 10;

QString popupStyleSheet()
{
    return QStringLiteral(R"(
        QLabel#title { color: %1; font-weight: 600; font-size: 11pt; }
        QLabel#hint, QLabel#empty { color: %2; }
        QPushButton#clearAll {
            color: %1; background: %3; border: none; border-radius: 4px; padding: 4px 10px;
        }
        QPushButton#clearAll:hover { background: %4; }
        QPushButton#clearAll:disabled { color: %2; }
        QListView { background: transparent; border: none; outline: none; }
        QScrollBar:vertical { background: transparent; width: 8px; margin: 2px; }
        QScrollBar::handle:vertical { background: %4; border-radius: 3px; min-height: 30px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }
    )")
        .arg(Theme::Text.name(), Theme::TextDim.name(), Theme::Card.name(),
             Theme::CardSelected.name());
}

} // namespace

PopupWindow::PopupWindow(HistoryModel *model, QWidget *parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    , m_model(model)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowTitle(QStringLiteral("Clipboard"));
    setFixedSize(PopupWidth, PopupHeight);
    setStyleSheet(popupStyleSheet());

    auto *title = new QLabel(QStringLiteral("Clipboard"));
    title->setObjectName(QStringLiteral("title"));

    m_clearAllButton = new QPushButton(QStringLiteral("Clear all"));
    m_clearAllButton->setObjectName(QStringLiteral("clearAll"));
    m_clearAllButton->setFocusPolicy(Qt::NoFocus);
    m_clearAllButton->setToolTip(QStringLiteral("Remove everything except pinned items (Ctrl+Delete)"));
    connect(m_clearAllButton, &QPushButton::clicked, this, &PopupWindow::clearAllRequested);

    auto *header = new QHBoxLayout;
    header->addWidget(title);
    header->addStretch();
    header->addWidget(m_clearAllButton);

    m_list = new QListView;
    m_list->setModel(m_model);
    m_list->setItemDelegate(new ItemDelegate(m_list));
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setUniformItemSizes(true);
    m_list->setMouseTracking(true);
    m_list->viewport()->setAttribute(Qt::WA_Hover);
    m_list->installEventFilter(this);
    m_list->viewport()->installEventFilter(this);

    m_emptyLabel = new QLabel(QStringLiteral("Nothing here yet.\nCopy some text and it will show up here."));
    m_emptyLabel->setObjectName(QStringLiteral("empty"));
    m_emptyLabel->setAlignment(Qt::AlignCenter);

    m_stack = new QStackedWidget;
    m_stack->addWidget(m_list);
    m_stack->addWidget(m_emptyLabel);

    auto *hint = new QLabel(QStringLiteral("Enter paste · P pin · Del remove · Esc close"));
    hint->setObjectName(QStringLiteral("hint"));
    hint->setAlignment(Qt::AlignCenter);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 8, 10);
    layout->setSpacing(8);
    layout->addLayout(header);
    layout->addWidget(m_stack, 1);
    layout->addWidget(hint);

    // Remember the selected row so it survives the model reloading after a
    // pin or delete.
    connect(m_list->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex &current) {
                if (current.isValid())
                    m_lastRow = current.row();
            });
    connect(m_model, &QAbstractItemModel::modelReset, this, [this] {
        restoreSelection();
        updateEmptyState();
    });
    updateEmptyState();
}

void PopupWindow::showPopup()
{
    // Centre on the primary screen. Under XWayland the cursor position is
    // stale while the pointer is over Wayland windows, so it is not used.
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        const QRect area = screen->availableGeometry();
        move(area.center() - rect().center());
    }

    m_lastRow = 0;
    restoreSelection();
    m_list->scrollToTop();

    show();
    raise();
    activateWindow();
    X11Activation::activate(this);
    m_list->setFocus();

    // The window manager may not have finished mapping the window when the
    // first request arrives; ask once more if we still are not focused.
    QTimer::singleShot(150, this, [this] {
        if (isVisible() && !isActiveWindow())
            X11Activation::activate(this);
    });
}

void PopupWindow::toggle()
{
    if (isVisible())
        hide();
    else
        showPopup();
}

bool PopupWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_list && event->type() == QEvent::KeyPress)
        return handleKey(static_cast<QKeyEvent *>(event));

    if (watched == m_list->viewport()) {
        switch (event->type()) {
        case QEvent::MouseButtonRelease:
            return handleClick(static_cast<QMouseEvent *>(event));
        case QEvent::MouseMove:
            // Repaint so the hovered pin / cross button highlights.
            m_list->viewport()->update();
            break;
        default:
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

bool PopupWindow::handleKey(QKeyEvent *event)
{
    const int row = m_list->currentIndex().row();

    switch (event->key()) {
    case Qt::Key_Escape:
        hide();
        return true;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        activateRow(row);
        return true;
    case Qt::Key_Delete:
        if (event->modifiers() & Qt::ControlModifier)
            emit clearAllRequested();
        else if (row >= 0)
            emit removeRequested(idAt(row));
        return true;
    case Qt::Key_P:
        if (row >= 0)
            emit pinToggleRequested(idAt(row));
        return true;
    default:
        break;
    }

    if (event->key() >= Qt::Key_1 && event->key() <= Qt::Key_9
        && !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier))) {
        activateRow(event->key() - Qt::Key_1);
        return true;
    }

    // Let the list handle navigation keys; swallow everything else so typing
    // letters does not jump around the list.
    switch (event->key()) {
    case Qt::Key_Up:
    case Qt::Key_Down:
    case Qt::Key_PageUp:
    case Qt::Key_PageDown:
    case Qt::Key_Home:
    case Qt::Key_End:
        return false;
    default:
        return true;
    }
}

bool PopupWindow::handleClick(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return false;

    const QModelIndex index = m_list->indexAt(event->position().toPoint());
    if (!index.isValid())
        return false;

    const QRect itemRect = m_list->visualRect(index);
    const QPoint pos = event->position().toPoint();
    if (ItemDelegate::pinButtonRect(itemRect).contains(pos))
        emit pinToggleRequested(idAt(index.row()));
    else if (ItemDelegate::deleteButtonRect(itemRect).contains(pos))
        emit removeRequested(idAt(index.row()));
    else
        activateRow(index.row());
    return true;
}

void PopupWindow::activateRow(int row)
{
    const qint64 id = idAt(row);
    if (id == 0)
        return;
    // Hide first so focus goes back to the app we are pasting into.
    hide();
    emit pasteRequested(id);
}

void PopupWindow::restoreSelection()
{
    const int count = m_model->rowCount();
    if (count == 0)
        return;
    const int row = qBound(0, m_lastRow, count - 1);
    m_list->setCurrentIndex(m_model->index(row));
}

void PopupWindow::updateEmptyState()
{
    const bool empty = m_model->rowCount() == 0;
    m_stack->setCurrentWidget(empty ? static_cast<QWidget *>(m_emptyLabel) : m_list);
    m_clearAllButton->setEnabled(!empty);
}

qint64 PopupWindow::idAt(int row) const
{
    const ClipItem *item = m_model->itemAt(row);
    return item ? item->id : 0;
}

void PopupWindow::changeEvent(QEvent *event)
{
    // Like Windows: clicking anywhere else closes the popup.
    if (event->type() == QEvent::ActivationChange && !isActiveWindow())
        hide();
    QWidget::changeEvent(event);
}

void PopupWindow::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), CornerRadius, CornerRadius);
    painter.fillPath(path, Theme::Window);
    painter.setPen(Theme::Border);
    painter.drawPath(path);
}
