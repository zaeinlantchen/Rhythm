// PlayerController.h
#ifndef PLAYERCONTROLLER_H
#define PLAYERCONTROLLER_H

#include <QObject>
#include <QList>
#include <QMediaPlayer>
#include <QAudioOutput>
#include "Song.h"

class PlayerController : public QObject
{
    Q_OBJECT
public:
    enum class PlayMode { Sequential, LoopAll, LoopOne, Shuffle };
    Q_ENUM(PlayMode)

    explicit PlayerController(QObject *parent = nullptr);
    void buildplaymodeIcon();
    void setQueue(const QList<Song> &songs, int startIndex = 0);
    void appendToQueue(const QList<Song> &songs);

    void playAt(int index);
    void play();
    void pause();
    void togglePlayPause();
    void stop();
    void next();
    void previous();

    void setVolume(int v);              // 0–100
    int  volume() const;
    void setPosition(qint64 ms);

    QMediaPlayer::PlaybackState state() const;
    bool isPlaying() const;
    qint64 position() const;
    qint64 duration() const;
    int  currentIndex() const { return m_currentIndex; }
    Song currentSong() const;
    const QList<Song> &queue() const { return m_queue; }

    void setPlayMode(PlayMode m) { m_playMode = m; }
    PlayMode playMode() const { return m_playMode; }

    void removeSongsFromQueue(const QList<qint64> &ids);

    void enqueueAndPlay(const Song &song);

    QAudioBufferOutput *audioBufferOutput() const { return m_bufferOutput; }

    void restoreQueue(const QList<Song> &songs, int currentIndex);

signals:
    void stateChanged(QMediaPlayer::PlaybackState state);
    void positionChanged(qint64 ms);
    void durationChanged(qint64 ms);
    void currentSongChanged(const Song &song);
    void queueChanged(const QList<Song> &queue);
    void errorOccurred(const QString &msg);

private slots:
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void onErrorOccurred(QMediaPlayer::Error err, const QString &msg);

private:
    void handleAutoNext();

    QMediaPlayer *m_player = nullptr;
    QAudioOutput *m_audio  = nullptr;

    QList<Song> m_queue;
    int m_currentIndex = -1;
    PlayMode m_playMode = PlayMode::Sequential;
    bool m_loopOnePending = false;

    QAudioBufferOutput *m_bufferOutput = nullptr;
};

#endif // PLAYERCONTROLLER_H