#include "DesktopLyrics.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QApplication>
#include <QScreen>
#include <QCursor>
#include <QGuiApplication>

DesktopLyrics::DesktopLyrics(QWidget *parent) : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint
                   | Qt::WindowStaysOnTopHint
                   | Qt::Tool
                   | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setMouseTracking(true);
    setMouseTracking(true);

    setMinimumSize(200, 60);
    resize(800, 100);
    setMask(rect());

    rebuildFont();
}

DesktopLyrics::~DesktopLyrics()
{
    stopHoverPolling();
    if (m_unlockButton) {
        m_unlockButton->hide();
        delete m_unlockButton;
        m_unlockButton = nullptr;
    }
}

void DesktopLyrics::rebuildFont()
{
    int px;
    if (m_userSetFontPx) {
        // 用户手动调过字号：按"当前高度 / 设定字号时的基准高度"等比缩放
        const qreal scale = qreal(height()) / qreal(m_baseHeight);
        px = qRound(m_baseFontPx * scale);
    } else {
        // 从未手动调过：默认字号 = 窗口高度的 36%
        px = qRound(height() * 0.36);
    }

    m_fontPx = qBound(14, px, 200);

    m_font = font();
    m_font.setPixelSize(m_fontPx);
    m_font.setBold(true);
    update();
}

// ==================== 数据 ====================

void DesktopLyrics::setLyrics(const Lyrics &lyrics)
{
    m_lyrics = lyrics;
    m_currentIndex = -1;
    update();
}

void DesktopLyrics::setCurrentTime(qint64 ms)
{
    m_currentTime = ms;
    if (m_lyrics.isEmpty()) return;
    const int idx = m_lyrics.indexAt(ms);
    if (idx != m_currentIndex) {
        m_currentIndex = idx;
        update();
    }
}

int DesktopLyrics::currentLineIndex() const
{
    if (m_lyrics.isEmpty()) return -1;
    if (m_currentIndex >= 0) return m_currentIndex;
    return m_lyrics.indexAt(m_currentTime);
}

QString DesktopLyrics::currentLineText() const
{
    const int idx = currentLineIndex();
    if (idx < 0) return QString();
    const QString t = m_lyrics.at(idx).text;
    return t.isEmpty() ? QStringLiteral("♪") : t;
}

// ==================== 样式 ====================

void DesktopLyrics::setTextColor(const QColor &c)
{
    m_textColor = c;
    if (m_unlockButton) m_unlockButton->setIconColor(c);
    update();
    emit styleChanged();
}

void DesktopLyrics::setOutlineColor(const QColor &c)
{
    m_outlineColor = c;
    update();
    emit styleChanged();
}

void DesktopLyrics::setFontPixelSize(int px)
{
    px = qBound(14, px, 200);

    m_userSetFontPx = true;
    m_baseHeight    = height();
    m_baseFontPx    = px;

    rebuildFont();      // 会按当前高度重算（此时 scale=1，结果就是 px）
    emit styleChanged();
}

// ==================== 锁定 ====================

void DesktopLyrics::setLocked(bool locked)
{
    if (m_locked == locked) return;
    m_locked = locked;
    applyLockInputTransparency();

    if (locked) {
        startHoverPolling();
        m_hovered = false;
        m_lockedHovered = false;
    } else {
        stopHoverPolling();
        m_lockedHovered = false;
        hideUnlockButton();
        setCursor(Qt::SizeAllCursor);
    }
    update();
    emit lockedChanged(locked);
}

void DesktopLyrics::applyLockInputTransparency()
{
    // 锁定 → 鼠标穿透；解锁 → 正常接收
    setAttribute(Qt::WA_TransparentForMouseEvents, m_locked);
}

// ==================== 悬停轮询（仅锁定状态） ====================

void DesktopLyrics::startHoverPolling()
{
    if (!m_hoverTimer) {
        m_hoverTimer = new QTimer(this);
        m_hoverTimer->setInterval(80);
        connect(m_hoverTimer, &QTimer::timeout, this, &DesktopLyrics::pollCursor);
    }
    m_hoverTimer->start();
}

void DesktopLyrics::stopHoverPolling()
{
    if (m_hoverTimer) m_hoverTimer->stop();
}

void DesktopLyrics::pollCursor()
{
    if (!isVisible() || !m_locked) return;

    const QPoint gp = QCursor::pos();
    const bool onLyrics = frameGeometry().contains(gp);
    const bool onUnlock = m_unlockButton
                          && m_unlockButton->isVisible()
                          && m_unlockButton->geometry().contains(gp);
    const bool hovered = onLyrics || onUnlock;

    if (hovered != m_lockedHovered) {
        m_lockedHovered = hovered;
        update();
    }

    if (hovered) {
        if (!m_unlockButton || !m_unlockButton->isVisible())
            showUnlockButton();
        else
            updateUnlockButtonPosition();
    } else {
        hideUnlockButton();
    }
}

// ==================== 屏幕 / 边界 ====================

QRect DesktopLyrics::screenRect() const
{
    if (auto *s = QGuiApplication::screenAt(frameGeometry().center()))
        return s->availableGeometry();
    if (auto *s = QGuiApplication::primaryScreen())
        return s->availableGeometry();
    return QRect(0, 0, 1920, 1080);
}

void DesktopLyrics::constrainPosition(QPoint &pos) const
{
    const QRect sr = screenRect();
    const QSize sz = size();
    pos.setX(qBound(sr.left(), pos.x(), sr.right()  - sz.width()  + 1));
    pos.setY(qBound(sr.top(),  pos.y(), sr.bottom() - sz.height() + 1));
}

void DesktopLyrics::snapToEdge()
{
    const QRect sr = screenRect();
    const QRect g  = geometry();

    int x = g.x();
    int y = g.y();

    if (qAbs(x - sr.left()) < kSnapPx)
        x = sr.left();
    else if (qAbs(g.right() - sr.right()) < kSnapPx)
        x = sr.right() - g.width() + 1;

    if (qAbs(y - sr.top()) < kSnapPx)
        y = sr.top();
    else if (qAbs(g.bottom() - sr.bottom()) < kSnapPx)
        y = sr.bottom() - g.height() + 1;

    if (x != g.x() || y != g.y())
        move(x, y);
}

// ==================== 命中区域 ====================

QRect DesktopLyrics::lockIconRect() const
{
    const int totalIcons = 2;                       // lock + close
    const int x = width() - kIconMargin - kIconSize * totalIcons - 4;
    return QRect(x, kIconMargin, kIconSize, kIconSize);
}

QRect DesktopLyrics::closeIconRect() const
{
    const int x = width() - kIconMargin - kIconSize;
    return QRect(x, kIconMargin, kIconSize, kIconSize);
}

DesktopLyrics::Zone DesktopLyrics::hitZone(const QPoint &pos) const
{
    const QRect r = rect();

    // 角落优先
    if (pos.x() >= r.right() - kResizeW && pos.y() >= r.bottom() - kResizeW)
        return ZoneResizeBR;
    if (pos.x() <= r.left() + kResizeW && pos.y() >= r.bottom() - kResizeW)
        return ZoneResizeBL;
    if (pos.x() >= r.right() - kResizeW && pos.y() <= r.top() + kResizeW)
        return ZoneResizeTR;
    if (pos.x() <= r.left() + kResizeW && pos.y() <= r.top() + kResizeW)
        return ZoneResizeTL;

    // 图标
    if (lockIconRect().contains(pos))  return ZoneLock;
    if (closeIconRect().contains(pos)) return ZoneClose;

    return ZoneNone;
}

bool DesktopLyrics::isResizeZone(Zone z) const
{
    return z == ZoneResizeTL || z == ZoneResizeTR ||
           z == ZoneResizeBL || z == ZoneResizeBR;
}

void DesktopLyrics::updateCursorForZone(Zone z)
{
    switch (z) {
    case ZoneLock:
    case ZoneClose:
        setCursor(Qt::PointingHandCursor);
        break;
    case ZoneResizeTL:
    case ZoneResizeBR:
        setCursor(Qt::SizeFDiagCursor);
        break;
    case ZoneResizeTR:
    case ZoneResizeBL:
        setCursor(Qt::SizeBDiagCursor);
        break;
    default:
        setCursor(Qt::SizeAllCursor);
        break;
    }
}

// ==================== 事件 ====================

void DesktopLyrics::enterEvent(QEnterEvent *e)
{
    QWidget::enterEvent(e);
    if (m_locked) return;
    m_hovered = true;
    update();
}

void DesktopLyrics::leaveEvent(QEvent *e)
{
    QWidget::leaveEvent(e);
    m_hovered = false;
    m_hoverZone = ZoneNone;
    update();
}

void DesktopLyrics::moveEvent(QMoveEvent *e)
{
    QWidget::moveEvent(e);
    updateUnlockButtonPosition();
}

void DesktopLyrics::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    setMask(rect());            // 之前加过的输入区域修正
    updateUnlockButtonPosition();
    rebuildFont();              // ← 新增：窗口缩放 → 字号同步
}

void DesktopLyrics::mousePressEvent(QMouseEvent *e)
{
    if (m_locked) { e->ignore(); return; }
    if (e->button() != Qt::LeftButton) { QWidget::mousePressEvent(e); return; }

    const Zone zone = hitZone(e->position().toPoint());

    if (isResizeZone(zone)) {
        m_resizing = true;
        m_resizeZone = zone;
        m_resizeStartGlobal = e->globalPosition().toPoint();
        m_resizeStartGeom   = geometry();
        e->accept();
        return;
    }

    if (zone == ZoneLock) {
        setLocked(true);
        e->accept();
        return;
    }
    if (zone == ZoneClose) {
        emit requestClose();
        e->accept();
        return;
    }

    // 拖动
    m_dragging = true;
    m_dragOffset = e->globalPosition().toPoint() - frameGeometry().topLeft();
    e->accept();
}

void DesktopLyrics::mouseMoveEvent(QMouseEvent *e)
{
    if (m_locked) { e->ignore(); return; }

    // 缩放
    if (m_resizing) {
        const QPoint delta = e->globalPosition().toPoint() - m_resizeStartGlobal;
        QRect g = m_resizeStartGeom;

        switch (m_resizeZone) {
        case ZoneResizeBR:
            g.setWidth (qMax(minimumWidth(),  m_resizeStartGeom.width()  + delta.x()));
            g.setHeight(qMax(minimumHeight(), m_resizeStartGeom.height() + delta.y()));
            break;
        case ZoneResizeBL: {
            const int newW = qMax(minimumWidth(), m_resizeStartGeom.width() - delta.x());
            g.setLeft(m_resizeStartGeom.right() - newW + 1);
            g.setWidth(newW);
            g.setHeight(qMax(minimumHeight(), m_resizeStartGeom.height() + delta.y()));
            break;
        }
        case ZoneResizeTR: {
            const int newH = qMax(minimumHeight(), m_resizeStartGeom.height() - delta.y());
            g.setTop(m_resizeStartGeom.bottom() - newH + 1);
            g.setHeight(newH);
            g.setWidth(qMax(minimumWidth(), m_resizeStartGeom.width() + delta.x()));
            break;
        }
        case ZoneResizeTL: {
            const int newW = qMax(minimumWidth(), m_resizeStartGeom.width() - delta.x());
            const int newH = qMax(minimumHeight(), m_resizeStartGeom.height() - delta.y());
            g.setLeft(m_resizeStartGeom.right() - newW + 1);
            g.setTop(m_resizeStartGeom.bottom() - newH + 1);
            g.setWidth(newW);
            g.setHeight(newH);
            break;
        }
        default: break;
        }

        // 限制在屏幕内
        const QRect sr = screenRect();
        if (g.right()  > sr.right())  g.setRight(sr.right());
        if (g.bottom() > sr.bottom()) g.setBottom(sr.bottom());
        if (g.left()   < sr.left())   { g.setLeft(sr.left()); g.setWidth(qMax(minimumWidth(), g.width())); }
        if (g.top()    < sr.top())    { g.setTop(sr.top());   g.setHeight(qMax(minimumHeight(), g.height())); }

        setGeometry(g);
        update();
        e->accept();
        return;
    }

    // 拖动
    if (m_dragging) {
        QPoint newPos = e->globalPosition().toPoint() - m_dragOffset;
        constrainPosition(newPos);
        move(newPos);
        update();
        e->accept();
        return;
    }

    // 更新悬停区域
    const Zone zone = hitZone(e->position().toPoint());
    if (zone != m_hoverZone) {
        m_hoverZone = zone;
        updateCursorForZone(zone);
        update();
    }

    QWidget::mouseMoveEvent(e);
}

void DesktopLyrics::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        const bool wasActive = m_dragging || m_resizing;
        m_dragging = false;
        m_resizing = false;
        m_resizeZone = ZoneNone;

        if (wasActive) {
            snapToEdge();
            emit geometryChanged(pos(), size());
        }
    }
    QWidget::mouseReleaseEvent(e);
}

void DesktopLyrics::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (m_locked) { e->ignore(); return; }
    if (e->button() == Qt::LeftButton) {
        setLocked(true);
        e->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(e);
}

void DesktopLyrics::contextMenuEvent(QContextMenuEvent *e)
{
    QMenu menu;

    QAction *lockAct = menu.addAction(QStringLiteral("锁定"));
    connect(lockAct, &QAction::triggered, this, [this]{ setLocked(true); });

    menu.addSeparator();

    QMenu *sizeMenu = menu.addMenu(QStringLiteral("字号"));
    for (int sz : {20, 24, 28, 32, 36, 40, 48, 56}) {
        QAction *a = sizeMenu->addAction(QString::number(sz));
        a->setCheckable(true);
        a->setChecked(sz == m_fontPx);
        connect(a, &QAction::triggered, this, [this, sz]{ setFontPixelSize(sz); });
    }

    QMenu *colorMenu = menu.addMenu(QStringLiteral("颜色"));
    const QList<QPair<QString, QColor>> colors = {
                                                   {QStringLiteral("青绿"), QColor(0x33, 0xDF, 0xCB)},
                                                   {QStringLiteral("白色"), QColor(0xFF, 0xFF, 0xFF)},
                                                   {QStringLiteral("黄色"), QColor(0xFF, 0xD5, 0x4F)},
                                                   {QStringLiteral("粉红"), QColor(0xFF, 0x88, 0xB0)},
                                                   {QStringLiteral("天蓝"), QColor(0x55, 0xB8, 0xFF)},
                                                   };
    for (const auto &pair : colors) {
        QAction *a = colorMenu->addAction(pair.first);
        QPixmap pm(16, 16); pm.fill(pair.second);
        a->setIcon(QIcon(pm));
        connect(a, &QAction::triggered, this, [this, c = pair.second]{ setTextColor(c); });
    }

    menu.addSeparator();
    menu.addAction(QStringLiteral("关闭桌面歌词"), this, [this]{ emit requestClose(); });

    menu.exec(e->globalPos());
}

// ==================== 绘制 ====================

void DesktopLyrics::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    p.fillRect(rect(), QColor(0, 0, 0, 1));

    // 判断是否处于悬停/拖动/缩放状态
    const bool hovered = m_locked ? m_lockedHovered : m_hovered;
    const bool active  = m_dragging || m_resizing;

    // ① 悬停 / 拖动背景
    if (hovered || active) {
        QPainterPath bgPath;
        bgPath.addRoundedRect(rect().adjusted(0, 0, -1, -1), 0, 0);

        QColor bg;
        if (active)
            bg = QColor(0x33, 0xDF, 0xCB, 40);   // 拖动/缩放：淡青
        else if (m_locked)
            bg = QColor(255, 255, 255, 0);   // 锁定时悬停：极淡白
        else
            bg = QColor(0xFF, 0xFF, 0xFF, 25);   // 未锁定时悬停：淡白

        p.fillPath(bgPath, bg);
    }

    // ① 范围边框（悬停 / 拖动 / 缩放时显示）
    if (!m_locked && (m_hovered || m_dragging || m_resizing))
        drawRangeBorder(p);

    // ② 歌词文字
    const QString text = currentLineText();
    if (!text.isEmpty()) {
        p.setFont(m_font);

        const QFontMetrics fm(m_font);
        const int textW = fm.horizontalAdvance(text);
        const int textH = fm.height();
        const int x = (width()  - textW) / 2;
        const int y = (height() - textH) / 2 + fm.ascent();

        QPainterPath path;
        path.addText(x, y, m_font, text);

        QPen outline(m_outlineColor);
        outline.setWidth(4);
        outline.setJoinStyle(Qt::RoundJoin);
        p.setPen(outline);
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);

        p.setPen(Qt::NoPen);
        p.setBrush(m_textColor);
        p.drawPath(path);
    }

    // ③ 悬停图标覆盖层
    drawHoverOverlay(p);
}

void DesktopLyrics::drawRangeBorder(QPainter &p)
{
    QPen pen(QColor(0x33, 0xDF, 0xCB, 130));
    pen.setWidth(1);
    pen.setStyle(Qt::SolidLine);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawRect(rect().adjusted(0, 0, -1, -1));
}

void DesktopLyrics::showUnlockButton()
{
    if (!m_unlockButton) {
        m_unlockButton = new UnlockButton(nullptr);
        m_unlockButton->setIconColor(m_textColor);
        connect(m_unlockButton, &UnlockButton::clicked, this, [this]{
            setLocked(false);   // 解锁 → hideUnlockButton 由 setLocked 内触发
        });
    }
    updateUnlockButtonPosition();
    m_unlockButton->show();
    m_unlockButton->raise();
}

void DesktopLyrics::hideUnlockButton()
{
    if (m_unlockButton) m_unlockButton->hide();
}

void DesktopLyrics::updateUnlockButtonPosition()
{
    if (!m_unlockButton || !m_unlockButton->isVisible()) return;

    const QRect g = frameGeometry();
    const QRect sr = screenRect();

    int x = g.right() - m_unlockButton->width() - 6;
    if (x < sr.left()) x = g.left() + 6;

    int y = g.top() + 6;
    if (y < sr.top()) y = sr.top() + 2;

    m_unlockButton->move(x, y);
}

void DesktopLyrics::drawHoverOverlay(QPainter &p)
{
    // 锁定状态：解锁入口由独立顶层控件 UnlockButton 承担，
    // 桌面歌词本身不再绘制任何悬停图标
    if (m_locked) return;
    if (!m_hovered) return;

    const QRect lr = lockIconRect();
    const QRect cr = closeIconRect();
    const QRect panelRect = lr.united(cr).adjusted(-8, -4, 8, 4);

    QPainterPath panelPath;
    panelPath.addRoundedRect(panelRect, 10, 10);
    p.fillPath(panelPath, QColor(0, 0, 0, 140));

    drawLockGlyph(p, lr, /*locked=*/false, m_hoverZone == ZoneLock);
    drawCloseGlyph(p, cr, m_hoverZone == ZoneClose);
}

void DesktopLyrics::drawLockGlyph(QPainter &p, const QRect &r, bool locked, bool hover)
{
    p.save();
    p.translate(r.center());

    const QColor color = hover ? QColor(0x33, 0xDF, 0xCB) : QColor(0xFF, 0xFF, 0xFF);
    QPen pen(color);
    pen.setWidth(2);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    // 锁体
    p.drawRoundedRect(QRectF(-7, 0, 14, 11), 2, 2);

    // 锁环
    if (locked) {
        // 闭合
        QPainterPath shackle;
        shackle.moveTo(-4, 0);
        shackle.lineTo(-4, -4);
        shackle.arcTo(QRectF(-4, -8, 8, 8), 180, -180);
        shackle.lineTo(4, 0);
        p.drawPath(shackle);
    } else {
        // 打开：向右上翻开
        QPainterPath shackle;
        shackle.moveTo(-3, 0);
        shackle.lineTo(-3, -5);
        shackle.arcTo(QRectF(-3, -9, 8, 8), 180, -180);
        p.drawPath(shackle);
    }

    p.restore();
}

void DesktopLyrics::drawCloseGlyph(QPainter &p, const QRect &r, bool hover)
{
    p.save();
    p.translate(r.center());

    const QColor color = hover ? QColor(0xFF, 0x6B, 0x6B) : QColor(0xFF, 0xFF, 0xFF);
    QPen pen(color);
    pen.setWidth(2);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);

    constexpr int s = 5;
    p.drawLine(-s, -s,  s,  s);
    p.drawLine( s, -s, -s,  s);

    p.restore();
}