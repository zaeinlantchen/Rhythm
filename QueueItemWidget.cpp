#include "QueueItemWidget.h"
#include "SquareImageLabel.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <qstyle.h>

static const int kCoverSide = 40;

QueueItemWidget::QueueItemWidget(QWidget *parent) : QWidget(parent)
{
    m_cover = new SquareImageLabel(this);
    m_cover->setFixedSize(kCoverSide, kCoverSide);
    m_cover->setBorderRadius(6);

    m_title = new QLabel(this);
    m_artist = new QLabel(this);
    m_title->setObjectName("queueItemTitle");
    m_artist->setObjectName("queueItemArtist");

    m_title->setTextInteractionFlags(Qt::NoTextInteraction);
    m_artist->setTextInteractionFlags(Qt::NoTextInteraction);
    m_title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_artist->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_title->setWordWrap(false);
    m_artist->setWordWrap(false);

    m_marker = new QLabel(this);
    m_marker->setFixedWidth(6);
    m_marker->setObjectName("queueItemMarker");

    auto *info = new QVBoxLayout();
    info->setContentsMargins(0, 0, 0, 0);
    info->setSpacing(2);
    info->addWidget(m_title);
    info->addWidget(m_artist);

    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(8, 6, 8, 6);
    root->setSpacing(8);
    root->addWidget(m_marker);
    root->addWidget(m_cover);
    root->addLayout(info, 1);
}

void QueueItemWidget::setSong(const Song &song)
{
    m_cover->setSource(song.coverPath);

    m_titleText  = song.title.isEmpty()  ? QStringLiteral("未命名")   : song.title;
    m_artistText = song.artist.isEmpty() ? QStringLiteral("未知艺术家") : song.artist;

    m_title->setToolTip(m_titleText);
    m_artist->setToolTip(m_artistText);

    updateElidedText();
}

void QueueItemWidget::setPlaying(bool playing)
{
    m_title->setProperty("playing", playing);
    m_artist->setProperty("playing", playing);

    // 播放指示器
    m_marker->setProperty("playing", playing);

    // 触发 QSS 重算
    m_title->style()->unpolish(m_title);
    m_title->style()->polish(m_title);
    m_artist->style()->unpolish(m_artist);
    m_artist->style()->polish(m_artist);
    m_marker->style()->unpolish(m_marker);
    m_marker->style()->polish(m_marker);
}

void QueueItemWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateElidedText();
}

void QueueItemWidget::updateElidedText()
{
    if (m_titleText.isEmpty() && m_artistText.isEmpty())
        return;

    const int w = m_title->width();
    if (w <= 0) return;

    m_title->setText(m_title->fontMetrics().elidedText(
        m_titleText, Qt::ElideRight, w));
    m_artist->setText(m_artist->fontMetrics().elidedText(
        m_artistText, Qt::ElideRight, w));
}