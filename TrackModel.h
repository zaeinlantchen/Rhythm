// TrackModel.h
#ifndef TRACKMODEL_H
#define TRACKMODEL_H

#include <QAbstractTableModel>
#include <QSortFilterProxyModel>
#include "Song.h"

class TrackModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column { ColTitle = 0, ColArtist, ColAlbum, ColDuration, ColumnCount };

    explicit TrackModel(QObject *parent = nullptr);

    void setSongs(const QList<Song> &songs);
    const QList<Song> &songs() const { return m_songs; }
    Song songAt(int row) const;

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &idx, int role) const override;
    QVariant headerData(int section, Qt::Orientation o, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &idx) const override;

private:
    QList<Song> m_songs;
};

// 让搜索框能一次匹配标题/艺术家/专辑
class SongFilterProxy : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;
protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override;
};

#endif // TRACKMODEL_H