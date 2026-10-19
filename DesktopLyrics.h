#ifndef DESKTOPLYRICS_H
#define DESKTOPLYRICS_H

#include <QWidget>
#include <QColor>
#include <QPoint>
#include <QRect>
#include <QFont>
#include <QTimer>
#include <UnlockButton.h>
#include "Lyrics.h"

class DesktopLyrics : public QWidget
{
    Q_OBJECT
public:
    explicit DesktopLyrics(QWidget *parent = nullptr);
    ~DesktopLyrics() override;

    void setLyrics(const Lyrics &lyrics);
    void setCurrentTime(qint64 ms);

    void setLocked(bool locked);
    bool isLocked() const { return m_locked; }

    void setTextColor(const QColor &c);
    void setOutlineColor(const QColor &c);
    void setFontPixelSize(int px);

    QColor textColor()     const { return m_textColor; }
    QColor outlineColor()  const { return m_outlineColor; }
    int    fontPixelSize() const { return m_fontPx; }

signals:
    void lockedChanged(bool locked);
    void geometryChanged(const QPoint &pos, const QSize &size);
    void styleChanged();
    void requestClose();

protected:
    void paintEvent(QPaintEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void contextMenuEvent(QContextMenuEvent *e) override;
    void enterEvent(QEnterEvent *e) override;
    void leaveEvent(QEvent *e) override;
    void moveEvent(QMoveEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;

private:
    enum Zone {
        ZoneNone,
        ZoneLock,
        ZoneClose,
        ZoneResizeTL, ZoneResizeTR,
        ZoneResizeBL, ZoneResizeBR,
    };

    int     currentLineIndex() const;
    QString currentLineText() const;
    void    rebuildFont();

    QRect lockIconRect()  const;
    QRect closeIconRect() const;

    Zone  hitZone(const QPoint &pos) const;
    bool  isResizeZone(Zone z) const;
    void  updateCursorForZone(Zone z);

    QRect screenRect() const;
    void  constrainPosition(QPoint &pos) const;
    void  snapToEdge();

    void applyLockInputTransparency();
    void startHoverPolling();
    void stopHoverPolling();
    void pollCursor();

    void drawHoverOverlay(QPainter &p);
    void drawLockGlyph(QPainter &p, const QRect &r, bool locked, bool hover);
    void drawCloseGlyph(QPainter &p, const QRect &r, bool hover);
    void drawRangeBorder(QPainter &p);

    void showUnlockButton();
    void hideUnlockButton();
    void updateUnlockButtonPosition();

    UnlockButton *m_unlockButton = nullptr;

    Lyrics m_lyrics;
    qint64 m_currentTime  = 0;
    int    m_currentIndex = -1;

    bool    m_locked = false;
    QColor  m_textColor    {0x33, 0xDF, 0xCB};
    QColor  m_outlineColor {0x00, 0x00, 0x00};
    int     m_fontPx = 36;

    QFont   m_font;

    bool   m_dragging = false;
    QPoint m_dragOffset;

    bool   m_resizing = false;
    Zone   m_resizeZone = ZoneNone;
    QPoint m_resizeStartGlobal;
    QRect  m_resizeStartGeom;

    bool m_hovered   = false;
    Zone m_hoverZone = ZoneNone;

    QTimer *m_hoverTimer = nullptr;

    static constexpr int kIconSize   = 22;
    static constexpr int kIconMargin = 8;
    static constexpr int kResizeW    = 10;
    static constexpr int kSnapPx     = 18;

    bool m_lockedHovered = false;
    int m_baseHeight = 100;   // 基准窗口高度
    int m_baseFontPx = 36;    // 基准字号
    bool m_userSetFontPx = false;   // 用户是否手动调过字号
};

#endif // DESKTOPLYRICS_H