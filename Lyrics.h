#ifndef LYRICS_H
#define LYRICS_H

#include <QString>
#include <QList>

struct LyricLine {
    qint64  timeMs = 0;
    QString text;
};

class Lyrics
{
public:
    static Lyrics fromPath(const QString &audioPath);   // 找同名 .lrc

    bool isEmpty() const { return m_lines.isEmpty(); }
    int  count()   const { return m_lines.size(); }
    const QList<LyricLine> &lines() const { return m_lines; }
    const LyricLine &at(int i) const { return m_lines.at(i); }

    // 返回 timeMs 对应的行索引；还没到第一行返回 -1
    int indexAt(qint64 timeMs) const;

private:
    QList<LyricLine> m_lines;
    friend class LyricsParser;
};

#endif