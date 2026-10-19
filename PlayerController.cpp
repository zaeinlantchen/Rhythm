// PlayerController.cpp
#include "PlayerController.h"
#include <QAudioBufferOutput>
#include <QRandomGenerator>

PlayerController::PlayerController(QObject *parent) : QObject(parent)
{
    m_audio = new QAudioOutput(this);
    m_audio->setVolume(0.7f);

    m_player = new QMediaPlayer(this);
    m_player->setAudioOutput(m_audio);

    m_bufferOutput = new QAudioBufferOutput(this);
    m_player->setAudioBufferOutput(m_bufferOutput);

    connect(m_player, &QMediaPlayer::playbackStateChanged,
            this, &PlayerController::stateChanged);
    connect(m_player, &QMediaPlayer::positionChanged,
            this, &PlayerController::positionChanged);
    connect(m_player, &QMediaPlayer::durationChanged,
            this, &PlayerController::durationChanged);
    connect(m_player, &QMediaPlayer::mediaStatusChanged,
            this, &PlayerController::onMediaStatusChanged);
    connect(m_player, &QMediaPlayer::errorOccurred,
            this, &PlayerController::onErrorOccurred);
}

void PlayerController::setQueue(const QList<Song> &songs, int startIndex)
{
    m_queue = songs;
    emit queueChanged(m_queue);

    if (m_queue.isEmpty()) {
        m_player->stop();
        m_player->setSource(QUrl());
        m_currentIndex = -1;
        return;
    }
    if (startIndex < 0 || startIndex >= m_queue.size())
        startIndex = 0;
    playAt(startIndex);
}

void PlayerController::appendToQueue(const QList<Song> &songs)
{
    if (songs.isEmpty()) return;
    m_queue.append(songs);
    emit queueChanged(m_queue);
}

void PlayerController::playAt(int index)
{
    if (index < 0 || index >= m_queue.size()) return;

    m_currentIndex = index;
    const Song &s = m_queue.at(index);

    m_loopOnePending = false;
    m_player->setSource(s.url());
    m_player->play();
    emit currentSongChanged(s);
}

void PlayerController::play()
{
    if (m_currentIndex < 0 && !m_queue.isEmpty())
        playAt(0);
    else
        m_player->play();
}

void PlayerController::pause()      { m_player->pause(); }
void PlayerController::stop()       { m_player->stop();  }

void PlayerController::togglePlayPause()
{
    if (m_player->playbackState() == QMediaPlayer::PlayingState)
        pause();
    else
        play();
}

void PlayerController::next()
{
    if (m_queue.isEmpty()) return;

    if (m_queue.size() == 1) {
        // 只有一首，重头播
        m_player->setPosition(0);
        m_player->play();
        return;
    }

    int i;
    if (m_playMode == PlayMode::Shuffle && m_queue.size() > 1) {
        do { i = QRandomGenerator::global()->bounded(int(m_queue.size())); }
        while (i == m_currentIndex);
    } else {
        i = (m_currentIndex + 1) % int(m_queue.size());
    }
    playAt(i);
}

void PlayerController::previous()
{
    if (m_queue.isEmpty()) return;
    if (m_queue.size() == 1) {
        m_player->setPosition(0);
        m_player->play();
        return;
    }

    // 循环切换：首曲 -> 末尾
    const int n = int(m_queue.size());
    int i = (m_currentIndex - 1 + n) % n;
    playAt(i);
}

void PlayerController::setVolume(int v)
{
    v = qBound(0, v, 100);
    m_audio->setVolume(v / 100.0f);
}

int PlayerController::volume() const
{
    return qRound(m_audio->volume() * 100.0f);
}

void PlayerController::setPosition(qint64 ms)
{
    m_player->setPosition(ms);
}

QMediaPlayer::PlaybackState PlayerController::state() const
{
    return m_player->playbackState();
}

bool PlayerController::isPlaying() const
{
    return m_player->playbackState() == QMediaPlayer::PlayingState;
}

qint64 PlayerController::position() const { return m_player->position(); }
qint64 PlayerController::duration() const { return m_player->duration(); }

Song PlayerController::currentSong() const
{
    if (m_currentIndex < 0 || m_currentIndex >= m_queue.size()) return {};
    return m_queue.at(m_currentIndex);
}

// PlayerController.cpp
void PlayerController::removeSongsFromQueue(const QList<qint64> &ids)
{
    if (ids.isEmpty() || m_queue.isEmpty()) return;

    const Song cur = currentSong();
    const bool curWillBeRemoved = cur.isValid() && ids.contains(cur.id);

    QList<Song> kept;
    kept.reserve(m_queue.size());
    for (const Song &s : m_queue)
        if (!ids.contains(s.id))
            kept.append(s);

    m_queue = kept;

    if (curWillBeRemoved) {
        m_player->stop();
        m_player->setSource(QUrl());
        m_currentIndex = -1;

        emit currentSongChanged(Song{}); // <--- 补上这一行，通知 UI 变成“未播放”
        emit queueChanged(m_queue);
        return;
    }

    // 保持当前播放位置（如果当前歌仍在队列里）
    if (m_currentIndex >= 0) {
        int newIdx = -1;
        for (int i = 0; i < m_queue.size(); ++i) {
            if (m_queue[i].id == cur.id) { newIdx = i; break; }
        }
        m_currentIndex = newIdx;
    }

    emit queueChanged(m_queue);
}

void PlayerController::enqueueAndPlay(const Song &song)
{
    if (!song.isValid()) return;

    int idx = -1;
    for (int i = 0; i < m_queue.size(); ++i) {
        if (m_queue[i].id == song.id) { idx = i; break; }
    }

    if (idx < 0) {
        m_queue.append(song);
        emit queueChanged(m_queue);
        idx = m_queue.size() - 1;
    }

    playAt(idx);
}

void PlayerController::restoreQueue(const QList<Song> &songs, int currentIndex)
{
    m_queue = songs;

    if (currentIndex >= 0 && currentIndex < m_queue.size()) {
        m_currentIndex = currentIndex;
        const Song &s = m_queue.at(currentIndex);
        m_player->setSource(s.url());     // 设置 source，但不 play()
        emit currentSongChanged(s);
    } else {
        m_currentIndex = -1;
    }
    emit queueChanged(m_queue);
}

void PlayerController::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    if (status != QMediaPlayer::EndOfMedia) return;
    handleAutoNext();
}

void PlayerController::handleAutoNext()
{
    if (m_playMode == PlayMode::LoopOne) {
        m_player->setPosition(0);
        m_player->play();
        return;
    }
    if (m_playMode == PlayMode::Sequential &&
        m_currentIndex == int(m_queue.size()) - 1) {
        return;   // 顺序播放结束，停住
    }
    next();
}

void PlayerController::onErrorOccurred(QMediaPlayer::Error err, const QString &msg)
{
    if (err == QMediaPlayer::NoError) return;
    emit errorOccurred(msg.isEmpty() ? QStringLiteral("播放失败") : msg);
    // 出错自动跳过
    if (!m_queue.isEmpty()) next();
}