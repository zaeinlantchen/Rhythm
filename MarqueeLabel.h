#ifndef MARQUEE_LABEL_H
#define MARQUEE_LABEL_H

#include <QLabel>
#include <QTimer>

class MarqueeLabel : public QLabel
{
    Q_OBJECT
public:
    explicit MarqueeLabel(QWidget *parent = nullptr);

    void setSpeed(int pxPerSecond);       // 滚动速度，默认 40
    void setGap(int gapPx);               // 循环间隔，默认 60
    void setPauseOnHover(bool on);        // 悬停暂停，默认 true

    void setText(const QString &text);    // 覆盖以重置偏移

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void updateScrolling();
    void tick();

    QTimer *m_timer = nullptr;

    qreal m_offsetF  = 0.0;   // 已滚动的像素（浮点避免累积误差）
    int   m_offset   = 0;     // 整数缓存
    int   m_speed    = 40;
    int   m_gap      = 60;
    int   m_textWidth = 0;

    bool m_scrolling = false;
    bool m_hovered   = false;
    bool m_pauseOnHover = true;
};

#endif // MARQUEE_LABEL_H