#ifndef HOVERICONBUTTON_H
#define HOVERICONBUTTON_H

#include <QPushButton>
#include <QIcon>
#include <QSize>
#include <QColor>
#include <QPropertyAnimation>

class HoverIconButton : public QPushButton
{
    Q_OBJECT
    Q_PROPERTY(qreal iconScale READ iconScale WRITE setIconScale)

public:
    explicit HoverIconButton(QWidget *parent = nullptr);

    void setNormalIconSize(const QSize &s);
    void setHoverScale(qreal scale);                 // 0.5 ~ 1.0
    void setHoverIconSize(const QSize &s);           // 便捷：按比例换算
    void setAnimationDuration(int ms);
    void setEasingCurve(const QEasingCurve &c);

    qreal iconScale() const { return m_scale; }
    void  setIconScale(qreal s);                     // 动画驱动

protected:
    void enterEvent(QEnterEvent *e) override;
    void leaveEvent(QEvent *e)      override;
    void mousePressEvent(QMouseEvent *e)   override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void paintEvent(QPaintEvent *e) override;

private:
    void animateTo(qreal target);

    QSize m_normalIconSize {24, 24};
    qreal m_hoverScale = 0.8;
    qreal m_scale      = 1.0;

    bool m_hovered = false;
    bool m_pressed = false;

    QPropertyAnimation *m_anim = nullptr;
};

#endif