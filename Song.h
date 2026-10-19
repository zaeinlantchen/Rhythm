// Song.h
#ifndef SONG_H
#define SONG_H

#include <QString>
#include <QUrl>
#include <QMetaType>

struct Song {
    qint64  id = -1;
    QString filePath;
    QString title;
    QString artist;
    QString album;
    QString genre;
    int     durationMs = 0;
    QString coverPath;          // 缓存后的封面文件路径
    QString lyricsPath;
    qint64  addedAt = 0;

    QUrl url() const { return QUrl::fromLocalFile(filePath); }
    bool isValid() const { return !filePath.isEmpty(); }
};

Q_DECLARE_METATYPE(Song)

#endif // SONG_H