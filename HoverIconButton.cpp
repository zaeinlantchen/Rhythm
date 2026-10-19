#include "HoverIconButton.h"

#include <QPainter>
#include <QMouseEvent>

HoverIconButton::HoverIconButton(QWidget *parent) : QPushButton(parent)
{
    setAttribute(Qt::WA_Hover, true);
    setFocusPolicy(Qt::NoFocus);
    setFlat(true);
    setText(QString());

    m_anim = new QPropertyAnimation(this, "iconScale", this);
    m_anim->setDuration(150);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);
}

void HoverIconButton::setNormalIconSize(const QSize &s)
{
    m_normalIconSize = s;
    update();
}

void HoverIconButton::setHoverScale(qreal scale)
{
    m_hoverScale = qBound(0.3, scale, 1.0);
}

void HoverIconButton::setHoverIconSize(const QSize &s)
{
    if (m_normalIconSize.width() > 0)
        m_hoverScale = qBound(0.3,
                              qreal(s.width()) / m_normalIconSize.width(),
                              1.0);
}

void HoverIconButton::setAnimationDuration(int ms)
{
    m_anim->setDuration(qMax(0, ms));
}

void HoverIconButton::setEasingCurve(const QEasingCurve &c)
{
    m_anim->setEasingCurve(c);
}

void HoverIconButton::setIconScale(qreal s)
{
    m_scale = s;
    update();
}

void HoverIconButton::animateTo(qreal target)
{
    if (qFuzzyCompare(m_scale, target) && m_anim->state() != QAbstractAnimation::Running)
        return;

    m_anim->stop();
    m_anim->setStartValue(m_scale);
    m_anim->setEndValue(target);
    m_anim->start();
}

void HoverIconButton::enterEvent(QEnterEvent *e)
{
    QPushButton::enterEvent(e);
    m_hovered = true;
    animateTo(m_hoverScale);
    update();                 // 背景切换需要立即重绘
}

void HoverIconButton::leaveEvent(QEvent *e)
{
    QPushButton::leaveEvent(e);
    m_hovered = false;
    m_pressed = false;
    animateTo(1.0);
    update();
}

void HoverIconButton::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_pressed = true;
        update();
    }
    QPushButton::mousePressEvent(e);
}

void HoverIconButton::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_pressed = false;
        update();
    }
    QPushButton::mouseReleaseEvent(e);
}

void HoverIconButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    const QRect r = rect();

    // ② 图标
    const QIcon ic = icon();
    if (ic.isNull()) return;

    // 目标尺寸 = 常态尺寸 * 当前缩放
    const QSize target(
        qMax(1, qRound(m_normalIconSize.width()  * m_scale)),
        qMax(1, qRound(m_normalIconSize.height() * m_scale)));

    // 按 DPR 请求位图，保证高分屏清晰
    const qreal dpr = devicePixelRatioF();
    QPixmap px = ic.pixmap(target * dpr, dpr);
    px.setDevicePixelRatio(dpr);

    // 居中绘制
    const int ix = r.x() + (r.width()  - target.width())  / 2;
    const int iy = r.y() + (r.height() - target.height()) / 2;
    p.drawPixmap(QRect(ix, iy, target.width(), target.height()), px);
}