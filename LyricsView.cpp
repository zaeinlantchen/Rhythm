#include "LyricsView.h"

#include <QPainter>
#include <QPointF>
#include <QMouseEvent>
#include <QLinearGradient>
#include <QDateTime>

LyricsView::LyricsView(QWidget *parent) : QWidget(parent)
{
    setMouseTracking(true);

    m_scrollAnim = new QPropertyAnimation(this, "scrollOffset", this);
    m_scrollAnim->setDuration(320);
    m_scrollAnim->setEasingCurve(QEasingCurve::OutCubic);
}

void LyricsView::clear()
{
    m_lyrics = Lyrics();
    m_currentIndex = -1;
    m_currentTime = 0;
    m_scrollOffset = 0;
    update();
}

void LyricsView::setLyrics(const Lyrics &lyrics)
{
    m_lyrics = lyrics;
    m_currentIndex = -1;
    m_currentTime = 0;
    m_scrollOffset = 0;
    updateScrollTarget(false);
    update();
}

void LyricsView::setCurrentTime(qint64 ms)
{
    m_currentTime = ms;
    if (m_lyrics.isEmpty()) return;

    const int idx = m_lyrics.indexAt(ms);
    if (idx != m_currentIndex) {
        const bool farJump = (m_currentIndex < 0) || (qAbs(idx - m_currentIndex) > 5);
        m_currentIndex = idx;
        updateScrollTarget(!farJump);
    }
    update();   // 逐字高亮需要每帧重绘
}

void LyricsView::setCurrentIndex(int index)
{
    if (index == m_currentIndex) return;
    const bool farJump = (m_currentIndex < 0) || (qAbs(index - m_currentIndex) > 5);
    m_currentIndex = index;
    updateScrollTarget(!farJump);
    update();
}

void LyricsView::setScrollOffset(qreal v)
{
    m_scrollOffset = v;
    update();
}

void LyricsView::wheelEvent(QWheelEvent *e)
{
    const int delta = e->angleDelta().y();
    if (delta == 0) { e->ignore(); return; }

    // 记录“用户正在手动滚动”，暂时不自动跟随
    m_userScrollUntil = QDateTime::currentMSecsSinceEpoch() + m_userScrollHoldMs;

    // 一次滚 2 行
    const qreal step = -delta / 120.0 * kLineHeight * 2;

    m_scrollAnim->stop();

    const int n = m_lyrics.count();
    if (n == 0) { e->accept(); return; }
    const qreal contentH = n * kLineHeight;
    const qreal viewH    = height();
    const qreal maxOff   = qMax<qreal>(0.0, contentH - viewH);

    const qreal target = qBound(0.0, m_scrollOffset + step, maxOff);
    setScrollOffset(target);

    e->accept();
}

void LyricsView::updateScrollTarget(bool animated)
{
    const int n = m_lyrics.count();
    if (n == 0) return;

    // 用户刚手动滚动过 → 不要抢他的视图
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_userScrollUntil > now) return;

    const qreal viewH    = height();

    const int focusIndex = (m_currentIndex >= 0) ? m_currentIndex : 0;
    qreal target = focusIndex * kLineHeight
             + kLineHeight / 2.0
             - viewH / 2.0;


    if (animated) {
        m_scrollAnim->stop();
        m_scrollAnim->setStartValue(m_scrollOffset);
        m_scrollAnim->setEndValue(target);
        m_scrollAnim->start();
    } else {
        m_scrollAnim->stop();
        setScrollOffset(target);
    }
}

void LyricsView::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    updateScrollTarget(false);
}

// -------------------- 视觉参数 --------------------

qreal LyricsView::lineAlpha(int index) const
{
    const int hl = centeredLine();
    if (hl < 0) return 180.0;
    const int dist = qAbs(index - hl);
    switch (dist) {
    case 0: return 255.0;
    case 1: return 200.0;
    case 2: return 140.0;
    case 3: return 90.0;
    case 4: return 60.0;
    default: return 35.0;
    }
}

int LyricsView::lineFontSize(int index) const
{
    const int hl = centeredLine();
    if (index == hl) return kCurFontSize;
    const int dist = qAbs(index - hl);
    if (dist == 1) return 14;
    return kMinFontSize;
}

int LyricsView::centeredLine() const
{
    if (m_lyrics.isEmpty()) return -1;
    const qreal contentY = m_scrollOffset + height() / 2.0;
    const int idx = int(contentY / kLineHeight);
    if (idx < 0 || idx >= m_lyrics.count()) return -1;
    return idx;
}

QColor LyricsView::lineColor(int index) const
{
    const int hl = centeredLine();
    if (index == hl) return QColor(0x0C, 0xB0, 0x9D);   // 品牌青
    return QColor(0x55, 0x55, 0x55);
}

qreal LyricsView::lineProgress(int index) const
{
    if (m_lyrics.isEmpty()) return 0.0;
    const int n = m_lyrics.count();
    if (index < 0 || index >= n) return 0.0;

    // 早于当前播放行 → 已全部唱过
    if (index < m_currentIndex) return 1.0;
    // 晚于当前播放行 → 还没开始唱
    if (index > m_currentIndex) return 0.0;

    // 就是当前播放行：按时间推进
    const qint64 start = m_lyrics.at(index).timeMs;
    const qint64 end   = (index + 1 < n) ? m_lyrics.at(index + 1).timeMs
                                       : start + 3000;
    if (end <= start) return 1.0;
    return qBound(0.0, qreal(m_currentTime - start) / qreal(end - start), 1.0);
}

void LyricsView::scrollBy(qreal deltaY, bool animated)
{
    Q_UNUSED(deltaY);
    Q_UNUSED(animated);
}
// -------------------- 绘制 --------------------

void LyricsView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    if (m_lyrics.isEmpty()) {
        p.setPen(QColor(0xAA, 0xAA, 0xAA));
        QFont f = font();
        f.setPixelSize(14);
        p.setFont(f);
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("暂无歌词"));
        return;
    }

    const int   n     = m_lyrics.count();
    const qreal viewH = height();
    const qreal viewW = width();

    const int firstVisible = qMax(0, int(m_scrollOffset / kLineHeight) - 1);
    const int lastVisible  = qMin(n - 1,
                                 int((m_scrollOffset + viewH) / kLineHeight) + 1);

    const int highlightIdx = centeredLine();

    for (int i = firstVisible; i <= lastVisible; ++i) {
        const qreal y = i * kLineHeight - m_scrollOffset;
        const QRectF lineRect(0, y, viewW, kLineHeight);

        const QString text = m_lyrics.at(i).text.isEmpty()
                                 ? QStringLiteral("♪")
                                 : m_lyrics.at(i).text;

        const bool isHighlight = (i == highlightIdx);

        QFont f = font();
        f.setPixelSize(lineFontSize(i));   // 内部已按居中行计算
        f.setBold(isHighlight);
        p.setFont(f);

        // ---- 非高亮行：整体绘制，按到居中行的距离分档透明度 ----
        if (!isHighlight) {
            QColor c = lineColor(i);
            c.setAlphaF(lineAlpha(i) / 255.0);
            p.setPen(c);
            p.drawText(lineRect, Qt::AlignCenter, text);
            continue;
        }

        // ---- 高亮行（居中行） ----
        const QFontMetricsF fm(f);
        const qreal textW = fm.horizontalAdvance(text);
        const qreal textX = (viewW - textW) / 2.0;

        QColor base = lineColor(i);

        // 底：淡色打底
        QColor dim = base;
        dim.setAlphaF(0.30);
        p.setPen(dim);
        p.drawText(lineRect, Qt::AlignCenter, text);

        // 逐字进度：从左向右叠加亮色
        const qreal prog = lineProgress(i);
        if (prog > 0.001) {
            const qreal clipW = textW * prog;
            p.save();
            p.setClipRect(QRectF(textX, y, clipW, kLineHeight));

            QColor glow = base;
            glow.setAlphaF(0.55);
            p.setPen(glow);
            p.drawText(lineRect, Qt::AlignCenter, text);

            p.setPen(base);
            p.drawText(lineRect, Qt::AlignCenter, text);
            p.restore();
        }
        // hover 提示线
        if (i == m_hoveredIndex) {
            QPen pen(QColor(base.red(), base.green(), base.blue(), 60));
            pen.setWidth(1);
            p.setPen(pen);
            p.drawLine(QPointF(textX, y + kLineHeight - 4),
                       QPointF(textX + textW, y + kLineHeight - 4));
        }
    }


    // 上下渐隐遮罩
    if (viewH > 120) {
        QLinearGradient topGrad(0, 0, 0, 60);
        topGrad.setColorAt(0.0, QColor(255, 255, 255, 255));
        topGrad.setColorAt(1.0, QColor(255, 255, 255, 0));
        p.fillRect(QRectF(0, 0, viewW, 60), topGrad);

        QLinearGradient botGrad(0, viewH - 60, 0, viewH);
        botGrad.setColorAt(0.0, QColor(255, 255, 255, 0));
        botGrad.setColorAt(1.0, QColor(255, 255, 255, 255));
        p.fillRect(QRectF(0, viewH - 60, viewW, 60), botGrad);
    }
}

// -------------------- 交互 --------------------

int LyricsView::lineAtY(int y) const
{
    const qreal contentY = y + m_scrollOffset;
    const int idx = int(std::floor(contentY / kLineHeight));
    if (idx < 0 || idx >= m_lyrics.count()) return -1;
    return idx;
}

void LyricsView::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(e);
        return;
    }
    const int idx = lineAtY(int(e->position().y()));
    if (idx >= 0) emit lineClicked(m_lyrics.at(idx).timeMs);
}

void LyricsView::mouseMoveEvent(QMouseEvent *e)
{
    const int idx = lineAtY(int(e->position().y()));
    if (idx != m_hoveredIndex) {
        m_hoveredIndex = idx;
        setCursor(idx >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
    }
}

void LyricsView::leaveEvent(QEvent *e)
{
    QWidget::leaveEvent(e);
    if (m_hoveredIndex != -1) {
        m_hoveredIndex = -1;
        setCursor(Qt::ArrowCursor);
        update();
    }
}