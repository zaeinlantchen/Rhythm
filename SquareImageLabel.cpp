#include "SquareImageLabel.h"

SquareImageLabel::SquareImageLabel(QWidget *parent) : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setCursor(Qt::PointingHandCursor);
    reload();
}

SquareImageLabel::~SquareImageLabel() = default;

int SquareImageLabel::squareSide() const
{
    return qMin(width(), height());
}

void SquareImageLabel::setSource(const QString &path)
{
    if (m_sourcePath == path && !path.isEmpty()) return;
    m_sourcePath = path;
    reload();
}

void SquareImageLabel::setDefaultSource(const QString &path)
{
    m_defaultPath = path;
}

void SquareImageLabel::setBorderRadius(int r)
{
    m_radius = r;
    update();
}

void SquareImageLabel::reload()
{
    if (m_movie) {
        m_movie->stop();
        m_movie->deleteLater();
        m_movie = nullptr;
    }
    m_sourcePixmap = QPixmap();
    m_frame        = QPixmap();

    QString actual = m_sourcePath;
    if (actual.isEmpty()) actual = m_defaultPath;
    if (actual.isEmpty()) { update(); return; }

    const bool isGif = actual.endsWith(".gif", Qt::CaseInsensitive);

    if (isGif) {
        m_movie = new QMovie(actual, QByteArray(), this);
        if (m_movie->isValid()) {
            m_movie->setCacheMode(QMovie::CacheAll);
            connect(m_movie, &QMovie::frameChanged,
                    this, &SquareImageLabel::onMovieFrameChanged);
            m_movie->start();
            return;
        }
        delete m_movie;
        m_movie = nullptr;
    }

    QPixmap src(actual);
    if (src.isNull() && actual != m_defaultPath)
        src = QPixmap(m_defaultPath);

    if (!src.isNull()) {
        m_sourcePixmap = src;
        m_frame = cropToSquare(m_sourcePixmap, squareSide());
    }
    update();
}

void SquareImageLabel::onMovieFrameChanged(int)
{
    if (!m_movie) return;
    m_sourcePixmap = m_movie->currentPixmap();
    m_frame = cropToSquare(m_sourcePixmap, squareSide());
    update();
}

QPixmap SquareImageLabel::cropToSquare(const QPixmap &src, int targetSide) const
{
    if (src.isNull() || targetSide <= 0) return {};

    // 短边铺满，长边居中裁掉
    const int side = qMin(src.width(), src.height());
    const int x = (src.width()  - side) / 2;
    const int y = (src.height() - side) / 2;
    QPixmap cropped = src.copy(x, y, side, side);
    return cropped.scaled(targetSide, targetSide,
                          Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

void SquareImageLabel::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_movie) return;   // GIF 会自己刷新
    if (!m_sourcePixmap.isNull()) {
        m_frame = cropToSquare(m_sourcePixmap, squareSide());
        update();
    }
}

void SquareImageLabel::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton)
        m_pressPos = e->position().toPoint();
    QWidget::mousePressEvent(e);
}

void SquareImageLabel::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        const QPoint diff = e->position().toPoint() - m_pressPos;
        // 拖动不超过 5px 才算点击，避免误触
        if (diff.manhattanLength() < 5)
            emit clicked();
    }
    QWidget::mouseReleaseEvent(e);
}

void SquareImageLabel::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    const QRect r = rect();
    QPainterPath path;
    path.addRoundedRect(r, m_radius, m_radius);
    p.setClipPath(path);

    if (m_frame.isNull()) {
        p.fillRect(r, QColor(0xF0, 0xF0, 0xF0));
        return;
    }
    p.drawPixmap(r, m_frame);
}