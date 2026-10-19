// MusicLibrary.h
#ifndef MUSICLIBRARY_H
#define MUSICLIBRARY_H

#include <QObject>
#include <QSqlDatabase>
#include <QList>
#include <QMediaPlayer>
#include <QJsonObject>
#include <QJsonDocument>
#include "Song.h"

static const char *kPathKey = "path";

class MusicLibrary : public QObject
{
    Q_OBJECT
public:
    explicit MusicLibrary(QObject *parent = nullptr);
    ~MusicLibrary();

    bool open(const QString &dbPath = QString());
    bool updateCover(qint64 id, const QString &coverPath);

    bool updateLyrics(qint64 id, const QString &lyricsPath);
    QList<Song> allSongs() const;
    QList<Song> search(const QString &keyword) const;
    int  songCount() const;

    void importFiles(const QStringList &paths);
    void removeSong(qint64 id);
    void clearAll();

    void removeSongs(const QList<qint64> &ids);

    QJsonObject configValue(const QString &key) const;
    bool        setConfigValue(const QString &key, const QJsonObject &value);
    bool        removeConfig(const QString &key);

    QString findLyrics(const QString &audioPath) const;

    struct PathsConfig {
        QString songFolder;    // 空表示未设置
        QString lyricsFolder;  // 空表示未设置
    };

    struct Playlist {
        qint64  id = -1;
        QString name;
        qint64  createdAt = 0;
        qint64  updatedAt = 0;
        int     songCount = 0;
    };

    // ---- 歌单 ----
    QList<Playlist> playlists() const;
    Playlist        playlistById(qint64 id) const;
    qint64          createPlaylist(const QString &name);
    bool            renamePlaylist(qint64 id, const QString &newName);
    bool            removePlaylist(qint64 id);

    // ---- 歌单歌曲 ----
    QList<Song> playlistSongs(qint64 playlistId) const;
    bool        addSongToPlaylist(qint64 playlistId, qint64 songId);
    bool        removeSongFromPlaylist(qint64 playlistId, qint64 songId);
    bool        setPlaylistSongs(qint64 playlistId, const QList<qint64> &songIds);

    // ---- 播放队列持久化 ----
    void          savePlayQueue(const QList<qint64> &songIds, int currentIndex);
    QList<Song>   loadPlayQueue(int &currentIndex) const;

    PathsConfig pathsConfig() const;
    bool        setPathsConfig(const PathsConfig &cfg);

    void rescan();      // 后台重扫所有歌曲文件夹

signals:
    void scanProgress(int current, int total);
    void scanFinished(int added);
    void libraryChanged();

    void scanStateChanged(bool running);   // 供 UI 显示忙碌状态

    void playlistsChanged();
    void playlistSongsChanged(qint64 playlistId);

private:
    bool initSchema();
    bool initConfigSchema();
    bool initPlaylistSchema();
    bool existsPath(const QString &path) const;
    bool insertSong(const Song &song);
    void collectFiles(const QString &dir, bool recursive, QStringList &out) const;

    void processNext();
    void onProbeStatusChanged(QMediaPlayer::MediaStatus status);

    bool removeSongByPath(const QString &path);

    Song buildSongFromProbe(const QString &filePath);
    QString extractCoverToCache(const QString &filePath,
                                const QMediaMetaData &meta) const;
    QString cacheCover(const QImage &img, const QString &srcPath) const;

    QString songKey(const Song &s) const;

    static bool isSupported(const QString &path);

    bool    pathInSongFolders(const QString &path) const;
    void    enqueueFromFolders();

    QHash<QString, QString> m_keyToPath;

    bool updateSongMetaByPath(const QString &path,
                              const QString &lyricsPath,
                              const QString &coverPath);

    QSqlDatabase  m_db;
    QMediaPlayer *m_probe = nullptr;

    QStringList m_pending;

    QStringList m_songFolders;
    QStringList m_lyricsFolders;

    QString     m_currentPath;
    int m_total = 0;
    int m_scanned = 0;
    int m_added = 0;
};

#endif // MUSICLIBRARY_H