#ifndef QUEUEITEMWIDGET_H
#define QUEUEITEMWIDGET_H

#include <QWidget>
#include "Song.h"

class SquareImageLabel;
class QLabel;

class QueueItemWidget : public QWidget
{
    Q_OBJECT
public:
    explicit QueueItemWidget(QWidget *parent = nullptr);

    void setSong(const Song &song);
    void setPlaying(bool playing);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    SquareImageLabel *m_cover  = nullptr;
    QLabel           *m_title  = nullptr;
    QLabel           *m_artist = nullptr;
    QLabel           *m_marker = nullptr;   // 播放指示器（可选）

    void updateElidedText();

    QString m_titleText;
    QString m_artistText;
};

#endif