#include "Lyrics.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <algorithm>

static QString findLrcFile(const QString &audioPath)
{
    QFileInfo fi(audioPath);
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

Lyrics Lyrics::fromPath(const QString &lrcPath)
{
    Lyrics out;
    if (lrcPath.isEmpty()) return out;

    QFile f(lrcPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return out;

    const QByteArray raw = f.readAll();

    if (f.size() > 2 * 1024 * 1024) return out;
    // 尝试 UTF-8，失败则试 GBK（中文歌词常见编码）
    QString text = QString::fromUtf8(raw);
    if (text.contains(QChar::ReplacementCharacter)) {
        // 简单的“是不是乱码”判断：出现替换字符就改试本地编码
        text = QString::fromLocal8Bit(raw);
    }

    // ---- 逐行解析 ----
    static const QRegularExpression reTime(
        R"(\[(\d{1,2}):(\d{2})(?:[.:](\d{1,3}))?\])");
    static const QRegularExpression reMeta(
        R"(\[(ti|ar|al|by|offset):([^\]]*)\])",
        QRegularExpression::CaseInsensitiveOption);

    int offsetMs = 0;

    const QStringList rawLines = text.split(QRegularExpression("[\r\n]+"),
                                            Qt::SkipEmptyParts);
    for (const QString &raw : rawLines) {
        // 元信息
        bool hasMeta = false;
        auto metaIt = reMeta.globalMatch(raw);
        while (metaIt.hasNext()) {
            auto m = metaIt.next();
            if (m.captured(1).toLower() == "offset")
                offsetMs = m.captured(2).trimmed().toInt();
            hasMeta = true;
        }
        if (hasMeta) continue;

        // 收集所有 [mm:ss.xx] 时间戳
        QList<qint64> times;
        int textStart = 0;
        auto it = reTime.globalMatch(raw);
        while (it.hasNext()) {
            auto m = it.next();
            const int mm = m.captured(1).toInt();
            const int ss = m.captured(2).toInt();
            int ms = 0;
            const QString frac = m.captured(3);
            if (frac.size() == 1)      ms = frac.toInt() * 100;
            else if (frac.size() == 2) ms = frac.toInt() * 10;
            else if (frac.size() == 3) ms = frac.toInt();

            times.append((mm * 60 + ss) * 1000 + ms);
            textStart = m.capturedEnd();
        }
        if (times.isEmpty()) continue;

        const QString lineText = raw.mid(textStart).trimmed();
        for (qint64 t : times) {
            LyricLine line;
            line.timeMs = t;
            line.text   = lineText;
            out.m_lines.append(line);
        }
    }

    // 应用 offset 并按时间排序
    for (auto &l : out.m_lines) l.timeMs += offsetMs;
    std::sort(out.m_lines.begin(), out.m_lines.end(),
              [](const LyricLine &a, const LyricLine &b){
                  return a.timeMs < b.timeMs;
              });

    return out;
}

int Lyrics::indexAt(qint64 timeMs) const
{
    if (m_lines.isEmpty()) return -1;
    if (timeMs < m_lines.first().timeMs) return -1;

    int lo = 0, hi = m_lines.size() - 1, best = -1;
    while (lo <= hi) {
        const int mid = (lo + hi) / 2;
        if (m_lines[mid].timeMs <= timeMs) {
            best = mid;
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return best;
}