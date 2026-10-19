#include "AudioSpectrumWidget.h"
#include <QPainter>
#include <QLinearGradient>

AudioSpectrumWidget::AudioSpectrumWidget(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    ensureSize();
}

void AudioSpectrumWidget::setBarCount(int n)
{
    m_barCount = qMax(4, n);
    ensureSize();
    update();
}

void AudioSpectrumWidget::setBarGap(int px)
{
    m_barGap = qMax(0, px);
    update();
}

void AudioSpectrumWidget::setTopColor(const QColor &c)    { m_topColor = c; update(); }
void AudioSpectrumWidget::setBottomColor(const QColor &c) { m_bottomColor = c; update(); }

void AudioSpectrumWidget::ensureSize()
{
    if (m_display.size() != m_barCount) {
        m_display = QVector<float>(m_barCount, 0.0f);
        m_target  = QVector<float>(m_barCount, 0.0f);
    }
}

void AudioSpectrumWidget::reset()
{
    m_target.fill(0.0f);
    m_display.fill(0.0f);
    update();
}

void AudioSpectrumWidget::pushBars(const QVector<float> &bars)
{
    ensureSize();
    const int n = qMin(m_barCount, bars.size());
    for (int i = 0; i < n; ++i)
        m_target[i] = qBound(0.0f, bars[i], 1.0f);

    // 上升快、下降慢
    for (int i = 0; i < m_barCount; ++i) {
        const float t = m_target[i];
        if (t > m_display[i])
            m_display[i] = m_display[i] * 0.35f + t * 0.65f;
        else
            m_display[i] = m_display[i] * 0.82f + t * 0.18f;
    }
    update();
}

void AudioSpectrumWidget::paintEvent(QPaintEvent *)
{
    if (m_display.isEmpty()) return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int w  = width();
    const int h  = height();
    const int midY = h / 2;

    const qreal cellW = qreal(w) / m_barCount;
    const int barW = qMax(1, int(cellW) - m_barGap);
    const qreal radius = barW / 2.0;

    // 柱状条颜色：从中心向外渐变
    QLinearGradient grad(0, 0, 0, h);
    grad.setColorAt(0.0,  m_bottomColor);
    grad.setColorAt(0.5,  m_topColor);
    grad.setColorAt(1.0,  m_bottomColor);

    p.setPen(Qt::NoPen);
    p.setBrush(grad);

    for (int i = 0; i < m_barCount; ++i) {
        const qreal x = i * cellW + m_barGap / 2.0;
        const float v = m_display[i];
        const int   bh = qMax(1, int(v * (h / 2 - 4)));

        // 上下对称绘制
        // const QRect r(int(x), midY - bh, barW, bh * 2);
        const QRect r(int(x), h - bh, barW, bh);
        p.drawRoundedRect(r, radius, radius);
    }
}