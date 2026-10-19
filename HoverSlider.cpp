#include "HoverSlider.h"
#include <QPainter>
#include <QMouseEvent>

HoverSlider::HoverSlider(QWidget *parent) : QSlider(parent)
{
    setOrientation(Qt::Horizontal);
    setAttribute(Qt::WA_Hover, true);
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void HoverSlider::enterEvent(QEnterEvent *e)
{
    QSlider::enterEvent(e);
    m_hovered = true;
    update();
}

void HoverSlider::leaveEvent(QEvent *e)
{
    QSlider::leaveEvent(e);
    m_hovered = false;
    update();
}

qreal HoverSlider::ratio() const
{
    const qreal r = maximum() - minimum();
    if (r <= 0) return 0.0;
    return (value() - minimum()) / r;
}

int HoverSlider::grooveHeight() const
{
    return m_hovered ? 4 : 2;
}

void HoverSlider::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        const qreal r = qreal(e->position().x()) / qMax(1, width());
        setValue(minimum() + qRound(r * (maximum() - minimum())));
    }
    QSlider::mousePressEvent(e);
}

void HoverSlider::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);

    const int w       = width();
    const int gh      = grooveHeight();
    const int centerY = m_grooveOffset;          // groove 中心线（= playerBar 顶边）

    const int grooveY = centerY - gh / 2;        // groove 左上角 y

    // 轨道背景
    p.fillRect(0, grooveY, w, gh, m_grooveColor);

    // 已播放部分
    const int playedW = int(w * ratio());
    if (playedW > 0)
        p.fillRect(0, grooveY, playedW, gh, m_playedColor);

    // hover：完整圆形手柄，圆心在轨道中心线上
    if (m_hovered) {
        const int r = 6;
        p.setPen(QPen(m_handleBorder, 2));
        p.setBrush(m_handleColor);
        p.drawEllipse(QPoint(playedW, centerY), r, r);
    }
}