#ifndef SQUAREIMAGELABEL_H
#define SQUAREIMAGELABEL_H

#include <QWidget>
#include <QPixmap>
#include <QMovie>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>

class SquareImageLabel : public QWidget
{
    Q_OBJECT
public:
    explicit SquareImageLabel(QWidget *parent = nullptr);
    ~SquareImageLabel() override;

    void setSource(const QString &path);        // 图片或 GIF；空则用默认图
    void setDefaultSource(const QString &path); // 默认 :/image/record.png
    void setBorderRadius(int r);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;

private slots:
    void onMovieFrameChanged(int frame);
signals:
    void clicked();


private:
    void    reload();
    QPixmap cropToSquare(const QPixmap &src, int targetSide) const;
    int     squareSide() const;

    QString m_sourcePath;
    QString m_defaultPath = QStringLiteral(":/image/record.png");

    QPixmap  m_sourcePixmap;   // 原始图（未裁切）
    QPixmap  m_frame;          // 已裁切到当前尺寸
    QPoint m_pressPos;
    QMovie  *m_movie = nullptr;
    int      m_radius = 8;
};

#endif