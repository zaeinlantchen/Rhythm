#include "UnlockButton.h"
#include <QPainter>
#include <QMouseEvent>
#include <QPainterPath>

UnlockButton::UnlockButton(QWidget *parent) : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint
                   | Qt::WindowStaysOnTopHint
                   | Qt::Tool
                   | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setAttribute(Qt::WA_Hover, true);
    setMouseTracking(true);
    setFixedSize(kSize, kSize);
    setCursor(Qt::PointingHandCursor);
    hide();
}

UnlockButton::~UnlockButton() = default;

void UnlockButton::setIconColor(const QColor &c)
{
    m_iconColor = c;
    update();
}

void UnlockButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // 圆形背景
    QColor bg;
    if (m_pressed)
        bg = QColor(0x33, 0xDF, 0xCB, 230);
    else if (m_hovered)
        bg = QColor(0, 0, 0, 180);
    else
        bg = QColor(0, 0, 0, 130);

    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawEllipse(rect().adjusted(2, 2, -2, -2));

    // 解锁图标（打开状态的锁）
    p.translate(rect().center());
    QPen pen(m_iconColor);
    pen.setWidth(2);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    // 锁体
    p.drawRoundedRect(QRectF(-6, 0, 12, 9), 2, 2);

    // 打开的锁环：右侧竖线不对齐，表示打开
    QPainterPath shackle;
    shackle.moveTo(-2, 0);
    shackle.lineTo(-2, -4);
    shackle.arcTo(QRectF(-2, -8, 8, 8), 180, -180);
    p.drawPath(shackle);
}

void UnlockButton::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_pressed = true;
        update();
    }
}

void UnlockButton::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_pressed = false;
        update();
        if (rect().contains(e->position().toPoint()))
            emit clicked();
    }
}

void UnlockButton::enterEvent(QEnterEvent *e)
{
    QWidget::enterEvent(e);
    m_hovered = true;
    update();
}

void UnlockButton::leaveEvent(QEvent *e)
{
    QWidget::leaveEvent(e);
    m_hovered = false;
    update();
}