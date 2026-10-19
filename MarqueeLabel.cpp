#include "MarqueeLabel.h"
#include <QPainter>
#include <QFontMetrics>
#include <QEvent>

MarqueeLabel::MarqueeLabel(QWidget *parent)
    : QLabel(parent)
{
    // 不被内容撑开
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    setMinimumWidth(0);

    m_timer = new QTimer(this);
    m_timer->setInterval(16);   // ~60fps
    connect(m_timer, &QTimer::timeout, this, &MarqueeLabel::tick);
}

void MarqueeLabel::setSpeed(int pxPerSecond)
{
    m_speed = qMax(1, pxPerSecond);
}

void MarqueeLabel::setGap(int gapPx)
{
    m_gap = qMax(0, gapPx);
    updateScrolling();
}

void MarqueeLabel::setPauseOnHover(bool on)
{
    m_pauseOnHover = on;
    if (!on && m_scrolling) m_timer->start();
}

void MarqueeLabel::setText(const QString &text)
{
    QLabel::setText(text);
    m_offsetF = 0;
    m_offset  = 0;
    m_textWidth = 0;            // 强制重算
    updateScrolling();
    update();
}

QSize MarqueeLabel::sizeHint() const
{
    // 关键：只按字高，不按文本宽度
    const QFontMetrics fm(font());
    return QSize(0, fm.height());
}

QSize MarqueeLabel::minimumSizeHint() const
{
    return sizeHint();
}

void MarqueeLabel::resizeEvent(QResizeEvent *event)
{
    QLabel::resizeEvent(event);
    updateScrolling();
}

void MarqueeLabel::changeEvent(QEvent *event)
{
    QLabel::changeEvent(event);
    // 字体变化时文本宽度会变，需要重新判断
    if (event->type() == QEvent::FontChange ||
        event->type() == QEvent::StyleChange)
    {
        m_textWidth = 0;
        updateScrolling();
    }
}

void MarqueeLabel::enterEvent(QEnterEvent *event)
{
    QLabel::enterEvent(event);
    m_hovered = true;
    if (m_pauseOnHover && m_scrolling) m_timer->stop();
}

void MarqueeLabel::leaveEvent(QEvent *event)
{
    QLabel::leaveEvent(event);
    m_hovered = false;
    if (m_pauseOnHover && m_scrolling && !m_timer->isActive())
        m_timer->start();
}

void MarqueeLabel::updateScrolling()
{
    if (m_textWidth == 0) {
        const QFontMetrics fm(font());
        m_textWidth = fm.horizontalAdvance(text());
    }

    const bool needScroll = (m_textWidth > width());

    if (needScroll && !m_scrolling) {
        m_scrolling = true;
        if (!(m_pauseOnHover && m_hovered))
            m_timer->start();
    } else if (!needScroll && m_scrolling) {
        m_scrolling = false;
        m_timer->stop();
        m_offsetF = 0;
        m_offset  = 0;
    }
    update();
}

void MarqueeLabel::tick()
{
    if (!m_scrolling) return;

    const qreal dt = m_timer->interval() / 1000.0;  // 秒
    m_offsetF += m_speed * dt;

    const int loopLen = m_textWidth + m_gap;
    if (loopLen > 0 && m_offsetF >= loopLen)
        m_offsetF -= loopLen;

    m_offset = int(m_offsetF);
    update();
}

void MarqueeLabel::paintEvent(QPaintEvent *event)
{
    if (!m_scrolling) {
        // 不溢出：交给 QLabel 默认绘制（保留对齐、样式、颜色）
        QLabel::paintEvent(event);
        return;
    }

    QPainter p(this);
    p.setClipRect(rect());

    // 使用标签本身的文字颜色
    QPalette pal = palette();
    p.setPen(pal.color(QPalette::WindowText));
    p.setFont(font());

    const QFontMetrics fm(font());
    const int y = (height() - fm.height()) / 2 + fm.ascent();
    const int loopLen = m_textWidth + m_gap;

    // 循环画：从 -offset 开始，每隔 loopLen 画一次，直到超出宽度
    for (int x = -m_offset; x < width(); x += loopLen) {
        p.drawText(x, y, text());
    }
}