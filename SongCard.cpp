#include "SongCard.h"
#include "SquareImageLabel.h"
#include "MarqueeLabel.h"

#include <QVBoxLayout>
#include <QPainter>
#include <QContextMenuEvent>
#include <QMouseEvent>

static const int kCardWidth = 160;
static const int kImageSide = kCardWidth - 16;   // 左右各 8 padding

SongCard::SongCard(QWidget *parent) : QWidget(parent)
{
    setFixedWidth(kCardWidth);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    setMouseTracking(true);

    m_image = new SquareImageLabel(this);
    m_image->setFixedSize(kImageSide, kImageSide);
    m_image->setBorderRadius(8);

    m_title = new MarqueeLabel(this);
    m_title->setAlignment(Qt::AlignCenter);
    m_title->setPauseOnHover(true);
    m_title->setSpeed(40);
    m_title->setGap(60);

    m_artist = new MarqueeLabel(this);
    m_artist->setAlignment(Qt::AlignCenter);
    m_artist->setPauseOnHover(true);
    m_artist->setSpeed(30);
    m_artist->setGap(50);

    QFont tf = m_title->font();
    tf.setPointSize(10);
    tf.setBold(true);
    m_title->setFont(tf);

    QFont af = m_artist->font();
    af.setPointSize(9);
    m_artist->setFont(af);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    layout->addWidget(m_image, 0, Qt::AlignHCenter);
    layout->addWidget(m_title);
    layout->addWidget(m_artist);
}

void SongCard::setSong(const Song &song)
{
    m_song = song;
    m_image->setSource(song.coverPath);          // 空 => 默认 record.png
    m_title->setText(song.title.isEmpty() ? QStringLiteral("未命名") : song.title);
    m_artist->setText(song.artist.isEmpty() ? QStringLiteral("未知艺术家") : song.artist);
}

void SongCard::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit activated(m_song);
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void SongCard::contextMenuEvent(QContextMenuEvent *event)
{
    emit contextMenuRequested(m_song, event->globalPos());
    event->accept();
}

void SongCard::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(event);
}

void SongCard::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
    QWidget::leaveEvent(event);
}

void SongCard::paintEvent(QPaintEvent *)
{
    if (!m_hovered) return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x33, 0xdf, 0xcb, 0x22));
    p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 10, 10);
}