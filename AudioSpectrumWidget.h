#ifndef AUDIOSPECTRUMWIDGET_H
#define AUDIOSPECTRUMWIDGET_H

#include <QWidget>
#include <QVector>
#include <QColor>

class AudioSpectrumWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AudioSpectrumWidget(QWidget *parent = nullptr);

    void setBarCount(int n);
    void setBarGap(int px);
    void setTopColor(const QColor &c);
    void setBottomColor(const QColor &c);

public slots:
    void pushBars(const QVector<float> &bars);   // 0..1
    void reset();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void ensureSize();

    int    m_barCount = 64;
    int    m_barGap   = 3;
    QColor m_topColor    {0x33, 0xdf, 0xcb, 0xB0};
    QColor m_bottomColor {0x33, 0xdf, 0xcb, 0x40};

    QVector<float> m_display;   // 平滑后的显示值
    QVector<float> m_target;
};

#endif