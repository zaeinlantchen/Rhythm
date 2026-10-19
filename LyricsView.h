#ifndef LYRICSVIEW_H
#define LYRICSVIEW_H

#include <QWidget>
#include <QVector>
#include <QPropertyAnimation>
#include "Lyrics.h"

class LyricsView : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal scrollOffset READ scrollOffset WRITE setScrollOffset)

public:
    explicit LyricsView(QWidget *parent = nullptr);

    void setLyrics(const Lyrics &lyrics);
    void setCurrentTime(qint64 ms);
    void setCurrentIndex(int index);
    int  currentIndex() const { return m_currentIndex; }
    void clear();

    qreal scrollOffset() const { return m_scrollOffset; }
    void  setScrollOffset(qreal v);

signals:
    void lineClicked(qint64 timeMs);

protected:
    void wheelEvent(QWheelEvent *e) override;
    void paintEvent(QPaintEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void leaveEvent(QEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;

private:
    void updateScrollTarget(bool animated);
    int  lineAtY(int y) const;
    qreal lineAlpha(int index) const;
    int   lineFontSize(int index) const;
    int centeredLine() const;
    QColor lineColor(int index) const;
    qreal lineProgress(int index) const;
    void scrollBy(qreal deltaY, bool animated = true);

    static constexpr int kLineHeight  = 56;
    static constexpr int kMinFontSize = 13;
    static constexpr int kCurFontSize = 18;

    int    m_userScrollHoldMs = 4000;
    Lyrics m_lyrics;
    int    m_currentIndex = -1;
    qint64 m_currentTime  = 0;
    int    m_hoveredIndex = -1;

    qreal m_scrollOffset = 0.0;
    qint64 m_userScrollUntil  = 0;
    QPropertyAnimation *m_scrollAnim = nullptr;
};

#endif