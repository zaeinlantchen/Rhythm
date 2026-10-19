#include "TableList.h"

#include <QMouseEvent>
#include <QPainter>
#include <QHeaderView>
#include <QItemSelectionModel>

TableList::TableList(QWidget *parent)
    : QTableView(parent)
{
    // 让 viewport 也追踪鼠标，否则鼠标在表格内部移动不会派发 MouseMove
    setMouseTracking(true);
    viewport()->setCursor(Qt::PointingHandCursor);
    viewport()->setMouseTracking(true);
    this->setShowGrid(false);
}

void TableList::setHoverColor(const QColor &color)
{
    m_hoverColor = color;
    viewport()->update();
}

void TableList::setSelectedRowColor(const QColor &color)
{
    m_selectedColor = color;
    viewport()->update();
}

bool TableList::viewportEvent(QEvent *event)
{
    switch (event->type()) {
    case QEvent::MouseMove: {
        auto *me = static_cast<QMouseEvent*>(event);
        const QModelIndex idx = indexAt(me->position().toPoint());
        const int row = idx.isValid() ? idx.row() : -1;
        if (row != m_hoveredRow) {
            m_hoveredRow = row;
            viewport()->update();   // 只重绘 viewport，不重绘表头
        }
        break;
    }
    case QEvent::Leave:
        if (m_hoveredRow != -1) {
            m_hoveredRow = -1;
            viewport()->update();
        }
        break;
    default:
        break;
    }
    return QTableView::viewportEvent(event);
}

void TableList::leaveEvent(QEvent *event)
{
    QTableView::leaveEvent(event);
    if (m_hoveredRow != -1) {
        m_hoveredRow = -1;
        viewport()->update();
    }
}

void TableList::paintEvent(QPaintEvent *event)
{
    // 先让基类正常绘制单元格内容
    QTableView::paintEvent(event);

    if (!model())
        return;

    QPainter p(viewport());
    const int vw = viewport()->width();

    // 整行选中背景：铺满整个 viewport 宽度
    if (selectionModel()) {
        const QModelIndexList selectedRows = selectionModel()->selectedRows();
        for (const QModelIndex &idx : selectedRows) {
            const int r = idx.row();
            if (r < 0 || r >= model()->rowCount())
                continue;
            const int y = rowViewportPosition(r);
            const int h = rowHeight(r);
            p.fillRect(0, y, vw, h, m_selectedColor);
        }
    }

    // 整行悬停背景：不叠加在已选中行上
    if (m_hoveredRow >= 0 && m_hoveredRow < model()->rowCount()) {
        const bool isSel = selectionModel()
        && selectionModel()->isRowSelected(m_hoveredRow, QModelIndex());
        if (!isSel) {
            const int y = rowViewportPosition(m_hoveredRow);
            const int h = rowHeight(m_hoveredRow);
            p.fillRect(0, y, vw, h, m_hoverColor);
        }
    }
}