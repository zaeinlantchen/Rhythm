#ifndef UNLOCKBUTTON_H
#define UNLOCKBUTTON_H

#include <QWidget>
#include <QColor>

class UnlockButton : public QWidget
{
    Q_OBJECT
public:
    explicit UnlockButton(QWidget *parent = nullptr);
    ~UnlockButton() override;

    void setIconColor(const QColor &c);

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void enterEvent(QEnterEvent *e) override;
    void leaveEvent(QEvent *e) override;

private:
    QColor m_iconColor {0xFF, 0xFF, 0xFF};
    bool   m_hovered = false;
    bool   m_pressed = false;

    static constexpr int kSize = 28;
};

#endif // UNLOCKBUTTON_H