#pragma once

#include <QWidget>

class HistoryModel;
class QLabel;
class QListView;
class QPushButton;
class QStackedWidget;

// The Win+V style popup: a frameless window listing the clipboard history.
//
// Keys:   Up/Down/PgUp/PgDn/Home/End move, Enter pastes, 1-9 paste that item,
//         P pins, Delete removes, Ctrl+Delete clears all, Esc closes.
// Mouse:  wheel scrolls, click pastes, the pin / cross buttons pin / remove.
class PopupWindow : public QWidget
{
    Q_OBJECT

public:
    explicit PopupWindow(HistoryModel *model, QWidget *parent = nullptr);

    void showPopup();
    void toggle();

signals:
    void pasteRequested(qint64 id);
    void pinToggleRequested(qint64 id);
    void removeRequested(qint64 id);
    void clearAllRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void changeEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    bool handleKey(QKeyEvent *event);
    bool handleClick(QMouseEvent *event);
    void activateRow(int row);
    void restoreSelection();
    void updateEmptyState();
    qint64 idAt(int row) const;

    HistoryModel *m_model;
    QListView *m_list;
    QLabel *m_emptyLabel;
    QStackedWidget *m_stack;
    QPushButton *m_clearAllButton;
    int m_lastRow = 0;
};
