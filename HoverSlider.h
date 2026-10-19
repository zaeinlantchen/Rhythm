#ifndef HOVERSLIDER_H
#define HOVERSLIDER_H

#include <QSlider>
#include <QColor>

class HoverSlider : public QSlider
{
    Q_OBJECT
public:
    explicit HoverSlider(QWidget *parent = nullptr);

    void setGrooveColor(const QColor &c)       { m_grooveColor = c; update(); }
    void setPlayedColor(const QColor &c)       { m_playedColor = c; update(); }
    void setHandleColor(const QColor &c)       { m_handleColor = c; update(); }
    void setHandleBorderColor(const QColor &c) { m_handleBorder = c; update(); }

    // overlay 场景下 groove 相对 widget 顶部的偏移
    void setGrooveOffset(int px) { m_grooveOffset = px; update(); }

protected:
    void enterEvent(QEnterEvent *e) override;
    void leaveEvent(QEvent *e) override;
    void paintEvent(QPaintEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;

private:
    qreal ratio() const;
    int   grooveHeight() const;   // hover 时 4px，普通 2px

    bool   m_hovered = false;
    int    m_grooveOffset = 6;    // 默认 6：widget 顶部空 6px，groove 画在 y=6

    QColor m_grooveColor  {0xE0, 0xE0, 0xE0};
    QColor m_playedColor  {0x33, 0xDF, 0xCB};
    QColor m_handleColor  {0x0C, 0xB0, 0x9D};
    QColor m_handleBorder {0xFF, 0xFF, 0xFF};
};

#endif