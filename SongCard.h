#ifndef SONGCARD_H
#define SONGCARD_H

#include <QWidget>
#include "Song.h"

class SquareImageLabel;
class MarqueeLabel;

class SongCard : public QWidget
{
    Q_OBJECT
public:
    explicit SongCard(QWidget *parent = nullptr);

    void setSong(const Song &song);
    const Song &song() const { return m_song; }

signals:
    void activated(const Song &song);
    void contextMenuRequested(const Song &song, const QPoint &globalPos);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    SquareImageLabel *m_image  = nullptr;
    MarqueeLabel     *m_title  = nullptr;
    MarqueeLabel     *m_artist = nullptr;
    Song              m_song;
    bool              m_hovered = false;
};

#endif