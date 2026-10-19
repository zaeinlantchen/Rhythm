// TrackModel.cpp
#include "TrackModel.h"
#include <QRegularExpression>

static QString formatMs(int ms)
{
    if (ms <= 0) return QStringLiteral("--:--");
    const int total = ms / 1000;
    return QString("%1:%2")
        .arg(total / 60, 2, 10, QChar('0'))
        .arg(total % 60, 2, 10, QChar('0'));
}

TrackModel::TrackModel(QObject *parent) : QAbstractTableModel(parent) {}

void TrackModel::setSongs(const QList<Song> &songs)
{
    beginResetModel();
    m_songs = songs;
    endResetModel();
}

Song TrackModel::songAt(int row) const
{
    if (row < 0 || row >= m_songs.size()) return {};
    return m_songs.at(row);
}

int TrackModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_songs.size();
}

int TrackModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant TrackModel::data(const QModelIndex &idx, int role) const
{
    if (!idx.isValid() || idx.row() >= m_songs.size()) return {};
    const Song &s = m_songs.at(idx.row());

    switch (role) {
    case Qt::DisplayRole:
        switch (idx.column()) {
        case ColTitle:    return s.title;
        case ColArtist:   return s.artist;
        case ColAlbum:    return s.album;
        case ColDuration: return formatMs(s.durationMs);
        }
        break;
    case Qt::TextAlignmentRole:
        if (idx.column() == ColDuration)
            return int(Qt::AlignRight | Qt::AlignVCenter);
        break;
    case Qt::ToolTipRole:
        return s.filePath;
    case Qt::UserRole:
        return QVariant::fromValue(s);
    }
    return {};
}

QVariant TrackModel::headerData(int section, Qt::Orientation o, int role) const
{
    if (o != Qt::Horizontal || role != Qt::DisplayRole) return {};
    switch (section) {
    case ColTitle:    return QStringLiteral("标题");
    case ColArtist:   return QStringLiteral("艺术家");
    case ColAlbum:    return QStringLiteral("专辑");
    case ColDuration: return QStringLiteral("时长");
    }
    return {};
}

Qt::ItemFlags TrackModel::flags(const QModelIndex &idx) const
{
    if (!idx.isValid()) return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

bool SongFilterProxy::filterAcceptsRow(int row, const QModelIndex &parent) const
{
    const QString pattern = filterRegularExpression().pattern();
    if (pattern.isEmpty()) return true;

    const QAbstractItemModel *m = sourceModel();
    for (int col = 0; col < m->columnCount(); ++col) {
        const QString text = m->index(row, col, parent).data(Qt::DisplayRole).toString();
        if (text.contains(filterRegularExpression())) return true;
    }
    return false;
}