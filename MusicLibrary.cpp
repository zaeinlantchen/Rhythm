// MusicLibrary.cpp
#include "MusicLibrary.h"

#include <QDir>
#include <QImage>
#include <QFileInfo>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QMediaMetaData>
#include <QDirIterator>

static const char *kSupportedExt[] = {
    "mp3", "flac", "wav", "m4a", "ogg", "aac", "opus", "wma"
};

MusicLibrary::MusicLibrary(QObject *parent) : QObject(parent)
{
    m_probe = new QMediaPlayer(this);
    connect(m_probe, &QMediaPlayer::mediaStatusChanged,
            this, &MusicLibrary::onProbeStatusChanged);
}

MusicLibrary::~MusicLibrary()
{
    if (m_db.isOpen()) m_db.close();
}

bool MusicLibrary::open(const QString &dbPath)
{
    QString path = dbPath;
    if (path.isEmpty()) {
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(dir);
        path = dir + "/library.db";
    }

    m_db = QSqlDatabase::addDatabase("QSQLITE", "rhythm_library");
    m_db.setDatabaseName(path);
    if (!m_db.open()) {
        qWarning() << "DB open failed:" << m_db.lastError().text();
        return false;
    }

    QSqlQuery q(m_db);
    q.exec("PRAGMA journal_mode=WAL");
    q.exec("PRAGMA synchronous=NORMAL");
    return initSchema();
}

bool MusicLibrary::updateCover(qint64 id, const QString &coverPath)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE songs SET cover_path = ? WHERE id = ?");
    q.addBindValue(coverPath);
    q.addBindValue(id);
    if (!q.exec()) return false;
    emit libraryChanged();
    return true;
}

bool MusicLibrary::updateLyrics(qint64 id, const QString &lyricsPath)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE songs SET lyrics_path = ? WHERE id = ?");
    q.addBindValue(lyricsPath);
    q.addBindValue(id);
    if (!q.exec()) return false;
    emit libraryChanged();
    return true;
}

bool MusicLibrary::initSchema()
{
    QSqlQuery q(m_db);
    q.exec(R"(
        CREATE TABLE IF NOT EXISTS songs (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            file_path   TEXT UNIQUE NOT NULL,
            title       TEXT,
            artist      TEXT,
            album       TEXT,
            genre       TEXT,
            duration_ms INTEGER DEFAULT 0,
            cover_path  TEXT,
            lyrics_path TEXT,
            added_at    INTEGER
        )
    )");
    return initConfigSchema() && initPlaylistSchema();
}

bool MusicLibrary::initConfigSchema()
{
    QSqlQuery q(m_db);
    return q.exec(R"(
        CREATE TABLE IF NOT EXISTS site_configs (
            key   TEXT PRIMARY KEY,
            value TEXT NOT NULL
        )
    )");
}

bool MusicLibrary::initPlaylistSchema()
{
    QSqlQuery q(m_db);
    if (!q.exec(R"(
        CREATE TABLE IF NOT EXISTS playlists (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            name       TEXT NOT NULL,
            created_at INTEGER,
            updated_at INTEGER
        )
    )")) return false;

    if (!q.exec(R"(
        CREATE TABLE IF NOT EXISTS playlist_songs (
            playlist_id INTEGER NOT NULL,
            song_id     INTEGER NOT NULL,
            position    INTEGER NOT NULL,
            PRIMARY KEY (playlist_id, song_id)
        )
    )")) return false;

    if (!q.exec(R"(
        CREATE TABLE IF NOT EXISTS play_queue (
            position INTEGER PRIMARY KEY,
            song_id  INTEGER NOT NULL
        )
    )")) return false;

    return true;
}

QList<Song> MusicLibrary::allSongs() const
{
    QList<Song> list;
    QSqlQuery q(m_db);
    if (!q.exec("SELECT id,file_path,title,artist,album,genre,duration_ms,cover_path,lyrics_path,added_at "
                "FROM songs ORDER BY added_at DESC, id DESC"))
        return list;

    while (q.next()) {
        Song s;
        s.id         = q.value(0).toLongLong();
        s.filePath   = q.value(1).toString();
        s.title      = q.value(2).toString();
        s.artist     = q.value(3).toString();
        s.album      = q.value(4).toString();
        s.genre      = q.value(5).toString();
        s.durationMs = q.value(6).toInt();
        s.coverPath  = q.value(7).toString();
        s.lyricsPath = q.value(8).toString();
        s.addedAt    = q.value(9).toLongLong();
        list.append(s);
    }
    return list;
}

QList<Song> MusicLibrary::search(const QString &keyword) const
{
    if (keyword.trimmed().isEmpty()) return allSongs();

    QList<Song> list;
    QSqlQuery q(m_db);
    q.prepare("SELECT id,file_path,title,artist,album,genre,duration_ms,cover_path,lyrics_path,added_at "
              "FROM songs WHERE title LIKE :kw OR artist LIKE :kw OR album LIKE :kw "
              "ORDER BY added_at DESC");
    q.bindValue(":kw", "%" + keyword + "%");
    if (!q.exec()) return list;

    while (q.next()) {
        Song s;
        s.id         = q.value(0).toLongLong();
        s.filePath   = q.value(1).toString();
        s.title      = q.value(2).toString();
        s.artist     = q.value(3).toString();
        s.album      = q.value(4).toString();
        s.genre      = q.value(5).toString();
        s.durationMs = q.value(6).toInt();
        s.coverPath  = q.value(7).toString();
        s.lyricsPath = q.value(8).toString();
        s.addedAt    = q.value(9).toLongLong();
        list.append(s);
    }
    return list;
}

int MusicLibrary::songCount() const
{
    QSqlQuery q(m_db);
    if (q.exec("SELECT COUNT(*) FROM songs") && q.next())
        return q.value(0).toInt();
    return 0;
}

bool MusicLibrary::isSupported(const QString &path)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    for (auto e : kSupportedExt)
        if (ext == QLatin1String(e)) return true;
    return false;
}

bool MusicLibrary::pathInSongFolders(const QString &path) const
{
    const QString songDir = pathsConfig().songFolder;
    if (songDir.isEmpty()) return false;
    return path.startsWith(songDir + "/") || path.startsWith(songDir + "\\");
}

bool MusicLibrary::updateSongMetaByPath(const QString &path,
                                        const QString &lyricsPath,
                                        const QString &coverPath)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE songs SET lyrics_path = ?, cover_path = ? WHERE file_path = ?");
    q.addBindValue(lyricsPath);
    q.addBindValue(coverPath);
    q.addBindValue(path);
    return q.exec();
}

bool MusicLibrary::existsPath(const QString &path) const
{
    QSqlQuery q(m_db);
    q.prepare("SELECT 1 FROM songs WHERE file_path = ? LIMIT 1");
    q.addBindValue(path);
    return q.exec() && q.next();
}

void MusicLibrary::collectFiles(const QString &dir, bool recursive, QStringList &out) const
{
    QDir d(dir);
    const QFileInfoList entries = d.entryInfoList(
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    for (const QFileInfo &fi : entries) {
        if (fi.isDir()) {
            if (recursive) collectFiles(fi.absoluteFilePath(), true, out);
        } else if (isSupported(fi.absoluteFilePath())) {
            out.append(fi.absoluteFilePath());
        }
    }
}

void MusicLibrary::importFiles(const QStringList &paths)
{
    m_pending.clear();
    for (const QString &p : paths) {
        QFileInfo fi(p);
        if (fi.isDir())          collectFiles(fi.absoluteFilePath(), true, m_pending);
        else if (isSupported(p)) m_pending.append(fi.absoluteFilePath());
    }
    m_pending.removeDuplicates();
    m_total   = m_pending.size();
    m_scanned = 0;
    m_added   = 0;
    if (m_total == 0) { emit scanFinished(0); return; }
    emit scanProgress(0, m_total);
    processNext();
}

void MusicLibrary::processNext()
{
    if (m_pending.isEmpty()) {
        if (m_added > 0) emit libraryChanged();
        emit scanFinished(m_added);
        emit scanStateChanged(false);
        return;
    }

    m_currentPath = m_pending.takeFirst();
    m_probe->setSource(QUrl::fromLocalFile(m_currentPath));
}

void MusicLibrary::onProbeStatusChanged(QMediaPlayer::MediaStatus status)
{
    if (status != QMediaPlayer::LoadedMedia &&
        status != QMediaPlayer::InvalidMedia)
        return;

    if (status == QMediaPlayer::LoadedMedia) {
        Song s = buildSongFromProbe(m_currentPath);  // 里面不再设置 lyricsPath / coverPath
        s.lyricsPath = findLyrics(s.filePath);
        s.coverPath  = extractCoverToCache(s.filePath, m_probe->metaData());
        // —— 元信息去重 ——
        const QString key = songKey(s);
        if (m_keyToPath.contains(key)) {
            const QString existing = m_keyToPath[key];
            const bool existingInSongs = pathInSongFolders(existing);
            const bool newInSongs      = pathInSongFolders(s.filePath);

            if (newInSongs && existingInSongs) {
                // 用新文件替换旧文件
                updateSongMetaByPath(existing, s.lyricsPath, s.coverPath);
            } else if (newInSongs && !existingInSongs) {
                removeSongByPath(existing);
                if (insertSong(s)) {
                    ++m_added;
                    m_keyToPath[key] = s.filePath;
                }
            }
            // 其它情况：跳过
        } else {
            if (insertSong(s)) {
                ++m_added;
                m_keyToPath[key] = s.filePath;
            }
        }
    }

    ++m_scanned;
    emit scanProgress(m_scanned, m_total);
    processNext();
}

bool MusicLibrary::removeSongByPath(const QString &path)
{
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM songs WHERE file_path = ?");
    q.addBindValue(path);
    return q.exec();
}

static QString autoDetectLrc(const QString &audioPath)
{
    const QFileInfo fi(audioPath);
    const QString dir  = fi.absolutePath();
    const QString base = fi.completeBaseName();

    const QStringList candidates = {
        dir + "/" + base + ".lrc",
        dir + "/" + base + ".LRC",
        dir + "/" + fi.fileName() + ".lrc",
    };
    for (const QString &p : candidates)
        if (QFile::exists(p)) return p;
    return {};
}

Song MusicLibrary::buildSongFromProbe(const QString &filePath)
{
    Song s;
    s.filePath   = filePath;
    s.addedAt    = QDateTime::currentSecsSinceEpoch();
    s.durationMs = int(m_probe->duration());

    const QMediaMetaData meta = m_probe->metaData();

    // ---- 基本文本信息 ----
    s.title = meta.value(QMediaMetaData::Title).toString();
    s.album = meta.value(QMediaMetaData::AlbumTitle).toString();
    s.genre = meta.value(QMediaMetaData::Genre).toString();

    const QVariant artistVar = meta.value(QMediaMetaData::ContributingArtist);
    if (artistVar.canConvert<QStringList>())
        s.artist = artistVar.toStringList().join(QStringLiteral(" / "));
    else
        s.artist = artistVar.toString();

    if (s.title.isEmpty())  s.title  = QFileInfo(filePath).completeBaseName();
    if (s.artist.isEmpty()) s.artist = QStringLiteral("未知艺术家");
    if (s.album.isEmpty())  s.album  = QStringLiteral("未知专辑");

    // ---- 专辑封面：成功 -> 缓存路径；失败 -> 空字符串（UI 回落到默认图） ----
    // s.coverPath = extractCoverToCache(filePath, meta);

    // s.lyricsPath = autoDetectLrc(filePath);
    return s;
}

QString MusicLibrary::extractCoverToCache(const QString &filePath,
                                          const QMediaMetaData &meta) const
{
    QImage cover;

    // Qt 6 首选：CoverArtImage（类型为 QImage）
    const QVariant coverVar = meta.value(QMediaMetaData::CoverArtImage);
    if (coverVar.canConvert<QImage>()) {
        cover = coverVar.value<QImage>();
    }

    // 备选：ThumbnailImage
    if (cover.isNull()) {
        const QVariant thumbVar = meta.value(QMediaMetaData::ThumbnailImage);
        if (thumbVar.canConvert<QImage>()) {
            cover = thumbVar.value<QImage>();
        }
    }

    // 提取失败 -> 返回空，UI 使用默认图
    if (cover.isNull())
        return QString();

    return cacheCover(cover, filePath);
}

QString MusicLibrary::cacheCover(const QImage &img, const QString &srcPath) const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
    + "/covers";
    QDir().mkpath(dir);

    // 以音频文件路径做哈希，保证同一首歌重复导入不会重复写盘
    const QByteArray hash = QCryptographicHash::hash(srcPath.toUtf8(),
                                                     QCryptographicHash::Md5).toHex();
    const QString outPath = dir + "/" + QString::fromLatin1(hash) + ".png";

    if (!QFile::exists(outPath)) {
        // 限制缓存尺寸，避免几十 MB 的原始封面拖慢加载
        img.scaled(500, 500, Qt::KeepAspectRatio, Qt::SmoothTransformation)
            .save(outPath, "PNG");
    }
    return outPath;
}

QString MusicLibrary::songKey(const Song &s) const
{
    return (s.title.toLower() + "|" + s.artist.toLower() + "|" + s.album.toLower());
}

QString MusicLibrary::findLyrics(const QString &audioPath) const
{
    const QFileInfo fi(audioPath);
    const QString base = fi.completeBaseName();

    auto tryPath = [](const QString &p) {
        return QFile::exists(p) ? p : QString();
    };

    // 音频同级目录，多个候选扩展名
    const QString dir = fi.absolutePath();
    const QStringList exts = {".lrc", ".LRC", ".Lrc"};
    for (const auto &e : exts) {
        if (QString r = tryPath(dir + "/" + base + e); !r.isEmpty())
            return r;
    }
    // 也试一下带音频扩展名的（如 Take Me Hand.mp3.lrc）
    if (QString r = tryPath(dir + "/" + fi.fileName() + ".lrc"); !r.isEmpty())
        return r;

    // 配置里的歌词文件夹
    const QString lrcDir = pathsConfig().lyricsFolder;
    if (lrcDir.isEmpty()) return {};

    for (const auto &e : exts) {
        if (QString r = tryPath(lrcDir + "/" + base + e); !r.isEmpty()){
            return r;
        }
    }

    // 递归查找，忽略扩展名大小写
    QDirIterator it(lrcDir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString p = it.next();
        const QFileInfo pf(p);
        if (pf.suffix().compare("lrc", Qt::CaseInsensitive) != 0) continue;
        if (pf.completeBaseName() == base)
            return p;
    }
    return {};
}

bool MusicLibrary::insertSong(const Song &s)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT OR IGNORE INTO songs "
              "(file_path,title,artist,album,genre,duration_ms,cover_path,lyrics_path,added_at) "
              "VALUES (?,?,?,?,?,?,?,?,?)");
    q.addBindValue(s.filePath);
    q.addBindValue(s.title);
    q.addBindValue(s.artist);
    q.addBindValue(s.album);
    q.addBindValue(s.genre);
    q.addBindValue(s.durationMs);
    q.addBindValue(s.coverPath);
    q.addBindValue(s.lyricsPath);
    q.addBindValue(s.addedAt);
    return q.exec();
}

void MusicLibrary::removeSong(qint64 id)
{
    qDebug() << "Attempting to remove song, id =" << id;

    // 先查出封面路径，便于删除缓存
    QString coverPath, lyricsPath;
    {
        QSqlQuery q(m_db);
        q.prepare("SELECT cover_path, lyrics_path FROM songs WHERE id = ?");
        q.addBindValue(id);
        if (q.exec() && q.next()) {
            coverPath = q.value(0).toString();
            lyricsPath = q.value(1).toString();
        }
    }

    QSqlQuery q(m_db);
    q.prepare("DELETE FROM songs WHERE id = ?");
    q.addBindValue(id);

    // 只要能执行成功，就刷新 UI（不完全依赖 numRowsAffected）
    if (q.exec()) {
        qDebug() << "Delete executed. Rows affected:" << q.numRowsAffected();

        // 删除封面缓存文件（只删我们缓存目录下的，避免误删用户手动指定的图）
        const QString appData = QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation);

        if (!coverPath.isEmpty() && coverPath.startsWith(appData + "/covers/"))
            QFile::remove(coverPath);

        if (!lyricsPath.isEmpty() && lyricsPath.startsWith(appData + "/lyrics/"))
            QFile::remove(lyricsPath);

        emit libraryChanged(); // 强制刷新
    } else {
        qDebug() << "Delete failed:" << q.lastError().text();
    }
}

void MusicLibrary::removeSongs(const QList<qint64> &ids)
{
    if (ids.isEmpty()) return;
    for (qint64 id : ids) removeSong(id);
    // removeSong 内部已 emit libraryChanged，批量时可选合并，这里简单起见逐个触发
}

QJsonObject MusicLibrary::configValue(const QString &key) const
{
    QSqlQuery q(m_db);
    q.prepare("SELECT value FROM site_configs WHERE key = ?");
    q.addBindValue(key);
    if (!q.exec() || !q.next())
        return {};

    const QByteArray raw = q.value(0).toString().toUtf8();
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    return doc.isObject() ? doc.object() : QJsonObject();
}

bool MusicLibrary::setConfigValue(const QString &key, const QJsonObject &value)
{
    const QByteArray raw = QJsonDocument(value).toJson(QJsonDocument::Compact);

    QSqlQuery q(m_db);
    q.prepare("INSERT INTO site_configs(key, value) VALUES(?, ?) "
              "ON CONFLICT(key) DO UPDATE SET value = excluded.value");
    q.addBindValue(key);
    q.addBindValue(QString::fromUtf8(raw));
    return q.exec();
}

bool MusicLibrary::removeConfig(const QString &key)
{
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM site_configs WHERE key = ?");
    q.addBindValue(key);
    return q.exec();
}

QList<MusicLibrary::Playlist> MusicLibrary::playlists() const
{
    QList<Playlist> out;
    QSqlQuery q(m_db);
    if (q.exec("SELECT p.id, p.name, p.created_at, p.updated_at, "
               "(SELECT COUNT(*) FROM playlist_songs ps WHERE ps.playlist_id = p.id) "
               "FROM playlists p ORDER BY p.updated_at DESC")) {
        while (q.next()) {
            Playlist p;
            p.id        = q.value(0).toLongLong();
            p.name      = q.value(1).toString();
            p.createdAt = q.value(2).toLongLong();
            p.updatedAt = q.value(3).toLongLong();
            p.songCount = q.value(4).toInt();
            out.append(p);
        }
    }
    return out;
}

MusicLibrary::Playlist MusicLibrary::playlistById(qint64 id) const
{
    Playlist p;
    QSqlQuery q(m_db);
    q.prepare("SELECT id, name, created_at, updated_at FROM playlists WHERE id = ?");
    q.addBindValue(id);
    if (q.exec() && q.next()) {
        p.id        = q.value(0).toLongLong();
        p.name      = q.value(1).toString();
        p.createdAt = q.value(2).toLongLong();
        p.updatedAt = q.value(3).toLongLong();
    }
    return p;
}

qint64 MusicLibrary::createPlaylist(const QString &name)
{
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO playlists(name, created_at, updated_at) VALUES(?,?,?)");
    q.addBindValue(name);
    q.addBindValue(now);
    q.addBindValue(now);
    if (!q.exec()) return -1;
    const qint64 id = q.lastInsertId().toLongLong();
    emit playlistsChanged();
    return id;
}

bool MusicLibrary::renamePlaylist(qint64 id, const QString &newName)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE playlists SET name = ?, updated_at = ? WHERE id = ?");
    q.addBindValue(newName);
    q.addBindValue(QDateTime::currentSecsSinceEpoch());
    q.addBindValue(id);
    if (!q.exec()) return false;
    emit playlistsChanged();
    return true;
}

bool MusicLibrary::removePlaylist(qint64 id)
{
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM playlist_songs WHERE playlist_id = ?");
    q.addBindValue(id);
    q.exec();
    q.prepare("DELETE FROM playlists WHERE id = ?");
    q.addBindValue(id);
    if (!q.exec()) return false;
    emit playlistsChanged();
    return true;
}

QList<Song> MusicLibrary::playlistSongs(qint64 playlistId) const
{
    QList<Song> out;
    QSqlQuery q(m_db);
    q.prepare("SELECT s.id, s.file_path, s.title, s.artist, s.album, s.genre, "
              "       s.duration_ms, s.cover_path, s.lyrics_path, s.added_at "
              "FROM playlist_songs ps JOIN songs s ON s.id = ps.song_id "
              "WHERE ps.playlist_id = ? ORDER BY ps.position");
    q.addBindValue(playlistId);
    if (!q.exec()) return out;
    while (q.next()) {
        Song s;
        s.id         = q.value(0).toLongLong();
        s.filePath   = q.value(1).toString();
        s.title      = q.value(2).toString();
        s.artist     = q.value(3).toString();
        s.album      = q.value(4).toString();
        s.genre      = q.value(5).toString();
        s.durationMs = q.value(6).toInt();
        s.coverPath  = q.value(7).toString();
        s.lyricsPath = q.value(8).toString();
        s.addedAt    = q.value(9).toLongLong();
        out.append(s);
    }
    return out;
}

bool MusicLibrary::addSongToPlaylist(qint64 playlistId, qint64 songId)
{
    QSqlQuery chk(m_db);
    chk.prepare("SELECT 1 FROM playlist_songs WHERE playlist_id = ? AND song_id = ?");
    chk.addBindValue(playlistId);
    chk.addBindValue(songId);
    if (chk.exec() && chk.next()) return true;      // 已存在

    QSqlQuery maxQ(m_db);
    maxQ.prepare("SELECT COALESCE(MAX(position), -1) FROM playlist_songs WHERE playlist_id = ?");
    maxQ.addBindValue(playlistId);
    int pos = 0;
    if (maxQ.exec() && maxQ.next()) pos = maxQ.value(0).toInt() + 1;

    QSqlQuery q(m_db);
    q.prepare("INSERT INTO playlist_songs(playlist_id, song_id, position) VALUES(?,?,?)");
    q.addBindValue(playlistId);
    q.addBindValue(songId);
    q.addBindValue(pos);
    if (!q.exec()) return false;

    QSqlQuery up(m_db);
    up.prepare("UPDATE playlists SET updated_at = ? WHERE id = ?");
    up.addBindValue(QDateTime::currentSecsSinceEpoch());
    up.addBindValue(playlistId);
    up.exec();

    emit playlistSongsChanged(playlistId);
    emit playlistsChanged();
    return true;
}

bool MusicLibrary::removeSongFromPlaylist(qint64 playlistId, qint64 songId)
{
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM playlist_songs WHERE playlist_id = ? AND song_id = ?");
    q.addBindValue(playlistId);
    q.addBindValue(songId);
    if (!q.exec()) return false;

    // 重排 position
    QSqlQuery list(m_db);
    list.prepare("SELECT song_id FROM playlist_songs WHERE playlist_id = ? ORDER BY position");
    list.addBindValue(playlistId);
    QList<qint64> ids;
    if (list.exec()) while (list.next()) ids << list.value(0).toLongLong();
    setPlaylistSongs(playlistId, ids);

    emit playlistSongsChanged(playlistId);
    emit playlistsChanged();
    return true;
}

bool MusicLibrary::setPlaylistSongs(qint64 playlistId, const QList<qint64> &songIds)
{
    QSqlQuery del(m_db);
    del.prepare("DELETE FROM playlist_songs WHERE playlist_id = ?");
    del.addBindValue(playlistId);
    if (!del.exec()) return false;

    QSqlQuery ins(m_db);
    ins.prepare("INSERT INTO playlist_songs(playlist_id, song_id, position) VALUES(?,?,?)");
    for (int i = 0; i < songIds.size(); ++i) {
        ins.addBindValue(playlistId);
        ins.addBindValue(songIds[i]);
        ins.addBindValue(i);
        if (!ins.exec()) return false;
    }
    return true;
}

void MusicLibrary::savePlayQueue(const QList<qint64> &songIds, int currentIndex)
{
    QSqlQuery del(m_db);
    del.exec("DELETE FROM play_queue");

    QSqlQuery ins(m_db);
    ins.prepare("INSERT INTO play_queue(position, song_id) VALUES(?,?)");
    for (int i = 0; i < songIds.size(); ++i) {
        ins.addBindValue(i);
        ins.addBindValue(songIds[i]);
        ins.exec();
    }

    QJsonObject o;
    o.insert("index", currentIndex);
    setConfigValue("play_queue_state", o);
}

QList<Song> MusicLibrary::loadPlayQueue(int &currentIndex) const
{
    currentIndex = -1;
    QList<qint64> ids;
    QSqlQuery q(m_db);
    if (q.exec("SELECT song_id FROM play_queue ORDER BY position"))
        while (q.next()) ids << q.value(0).toLongLong();

    const QJsonObject o = configValue("play_queue_state");
    currentIndex = o.value("index").toInt(-1);

    if (ids.isEmpty()) return {};

    QList<Song> out;
    for (qint64 id : ids) {
        QSqlQuery sq(m_db);
        sq.prepare("SELECT id, file_path, title, artist, album, genre, "
                   "       duration_ms, cover_path, lyrics_path, added_at "
                   "FROM songs WHERE id = ?");
        sq.addBindValue(id);
        if (sq.exec() && sq.next()) {
            Song s;
            s.id         = sq.value(0).toLongLong();
            s.filePath   = sq.value(1).toString();
            s.title      = sq.value(2).toString();
            s.artist     = sq.value(3).toString();
            s.album      = sq.value(4).toString();
            s.genre      = sq.value(5).toString();
            s.durationMs = sq.value(6).toInt();
            s.coverPath  = sq.value(7).toString();
            s.lyricsPath = sq.value(8).toString();
            s.addedAt    = sq.value(9).toLongLong();
            out.append(s);
        }
    }
    return out;
}

MusicLibrary::PathsConfig MusicLibrary::pathsConfig() const
{
    PathsConfig cfg;
    const QJsonObject o = configValue(kPathKey);
    cfg.songFolder   = o.value("song").toString();
    cfg.lyricsFolder = o.value("lyrics").toString();
    return cfg;
}

bool MusicLibrary::setPathsConfig(const PathsConfig &cfg)
{
    QJsonObject o;
    o.insert("song",   QDir::cleanPath(cfg.songFolder));
    if (cfg.lyricsFolder.isEmpty())
        o.insert("lyrics", QString());
    else
        o.insert("lyrics", QDir::cleanPath(cfg.lyricsFolder));

    return setConfigValue(kPathKey, o);
}

void MusicLibrary::rescan()
{
    if (!m_pending.isEmpty()) return;

    const auto cfg = pathsConfig();
    if (cfg.songFolder.isEmpty()) {
        emit scanFinished(0);
        return;
    }

    emit scanStateChanged(true);

    m_keyToPath.clear();
    for (const Song &s : allSongs())
        m_keyToPath[songKey(s)] = s.filePath;

    m_pending.clear();
    collectFiles(cfg.songFolder, /*recursive=*/true, m_pending);
    m_pending.removeDuplicates();

    m_total   = m_pending.size();
    m_scanned = 0;
    m_added   = 0;

    if (m_total == 0) {
        emit scanStateChanged(false);
        emit scanFinished(0);
        return;
    }
    emit scanProgress(0, m_total);
    processNext();
}

void MusicLibrary::clearAll()
{
    QSqlQuery q(m_db);
    if (q.exec("DELETE FROM songs"))
        emit libraryChanged();
}