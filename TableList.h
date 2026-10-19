#ifndef TABLELIST_H
#define TABLELIST_H

#include <QTableView>

class TableList : public QTableView
{
    Q_OBJECT
public:
    explicit TableList(QWidget *parent = nullptr);

    void setHoverColor(const QColor &color);
    void setSelectedRowColor(const QColor &color);

protected:
    bool viewportEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    int    m_hoveredRow = -1;
    QColor m_hoverColor = QColor(0, 0, 0, 18);
    QColor m_selectedColor = QColor(29, 185, 84, 55);
};

#endif // TableList_H
