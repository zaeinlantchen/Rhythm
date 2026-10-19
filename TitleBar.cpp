#include "TitleBar.h"

#include <QHBoxLayout>
#include <QPainter>
#include <QMouseEvent>
#include <QMenu>
#include <QFile>

TitleBar::TitleBar(QWidget *parent)
    : QWidget(parent)
    , m_bgColor(40, 40, 40)
{
    setObjectName("TitleBar");
    setAttribute(Qt::WA_StyledBackground, true);

    initUi();
    initConnections();
}

TitleBar::~TitleBar() = default;

void TitleBar::initUi()
{
    setFixedHeight(100);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_iconLabel = new QLabel(this);
    m_iconLabel->setFixedSize(20, 20);
    m_iconLabel->setAlignment(Qt::AlignCenter);

    m_titleLabel = new QLabel("Rhythm", this);
    {
        QFont f = m_titleLabel->font();
        f.setPointSize(11);
        m_titleLabel->setFont(f);
    }
    m_titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_menuBtn  = new QToolButton(this);
    m_minBtn   = new QToolButton(this);
    m_maxBtn   = new QToolButton(this);
    m_closeBtn = new QToolButton(this);

    // 汉堡按钮：点开弹菜单
    m_menuBtn->setPopupMode(QToolButton::InstantPopup);

    // 三个窗口按钮
    for (auto *b : {m_menuBtn, m_minBtn, m_maxBtn, m_closeBtn}) {
        b->setFixedSize(36, 30);
        b->setAutoRaise(true);
        b->setFocusPolicy(Qt::NoFocus);
    }

    QIcon menuIcon;
    menuIcon.addPixmap(QPixmap(":/icons/setting.svg"),       QIcon::Normal);
    menuIcon.addPixmap(QPixmap(":/icons/setting-hover.svg"), QIcon::Active);

    m_menuBtn->setIcon(menuIcon);

    QIcon minIcon;
    minIcon.addPixmap(QPixmap(":/icons/minimize.svg"),       QIcon::Normal);
    minIcon.addPixmap(QPixmap(":/icons/minimize-hover.svg"), QIcon::Active);

    m_minBtn->setIcon(minIcon);   // ─

    buildMaxIcons();

    QIcon closeIcon;
    closeIcon.addPixmap(QPixmap(":/icons/close.svg"),       QIcon::Normal);
    closeIcon.addPixmap(QPixmap(":/icons/close-hover.svg"), QIcon::Active);
    m_closeBtn->setIcon(closeIcon); // ✕

    m_menuBtn->setToolTip(QStringLiteral("菜单"));
    m_minBtn->setToolTip(QStringLiteral("最小化"));
    m_maxBtn->setToolTip(QStringLiteral("最大化"));
    m_closeBtn->setToolTip(QStringLiteral("关闭"));

    m_minBtn->setObjectName("minBtn");
    m_closeBtn->setObjectName("closeBtn");
    m_menuBtn->setObjectName("menuBtn");

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 0, 24, 36);
    layout->setSpacing(4);
    layout->addWidget(m_iconLabel);
    layout->addWidget(m_titleLabel);

    layout->addStretch();

    layout->addStretch();

    layout->addWidget(m_menuBtn);
    layout->addWidget(m_minBtn);
    layout->addWidget(m_maxBtn);
    layout->addWidget(m_closeBtn);

    buildHamburgerMenu();

    QFile f(":/style/Titlebar.qss");
    if (f.open(QIODevice::ReadOnly | QIODevice::Text))
        setStyleSheet(QString::fromUtf8(f.readAll()));
}

void TitleBar::initConnections()
{
    connect(m_minBtn,   &QPushButton::clicked, this, &TitleBar::signalMinClicked);
    connect(m_maxBtn,   &QPushButton::clicked, this, &TitleBar::signalMaxRestoreClicked);
    connect(m_closeBtn, &QPushButton::clicked, this, &TitleBar::signalCloseClicked);
}

void TitleBar::buildHamburgerMenu()
{
    auto *menu = new QMenu(this);
    menu->addAction(QStringLiteral("设置"), this, [this]{ emit signalSetting(); });
    menu->addSeparator();
    menu->addAction(QStringLiteral("关于 Rhythm"),  this, [this]{ emit signalAbout(); });
    menu->addSeparator();
    menu->addAction(QStringLiteral("退出"),         this, [this]{ emit signalExit(); });
    m_menuBtn->setMenu(menu);
}

void TitleBar::setTitleIcon(const QString &iconPath, const QSize &size)
{
    QPixmap p(iconPath);
    if (!p.isNull()) {
        m_iconLabel->setPixmap(p.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

void TitleBar::setTitleContent(const QString &title, int fontSize)
{
    QFont f = m_titleLabel->font();
    f.setPointSize(fontSize);
    m_titleLabel->setFont(f);
    m_titleLabel->setText(title);
}

void TitleBar::setBackgroundColor(const QColor &color)
{
    m_bgColor = color;
    update();
}

void TitleBar::setMaximized(bool maximized)
{
    m_maximized = maximized;
    m_maxBtn->setIcon(maximized
                          ? m_restoreIcon : m_maxIcon);
    m_maxBtn->setToolTip(maximized ? QStringLiteral("还原") : QStringLiteral("最大化"));
}

// void TitleBar::paintEvent(QPaintEvent *event)
// {
//     QPainter p(this);
//     p.fillRect(rect(), m_bgColor);
//     QWidget::paintEvent(event);
// }

void TitleBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        QWidget *top = window();
        m_dragStartPos = event->globalPosition().toPoint() - top->frameGeometry().topLeft();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void TitleBar::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        QWidget *top = window();
        if (top->isMaximized()) {
            // 从最大化拖出：还原后让鼠标保持相对位置
            const qreal ratio = qreal(event->position().x()) / qMax(1, width());
            top->showNormal();
            const int newX = event->globalPosition().toPoint().x() - int(top->width() * ratio);
            const int newY = event->globalPosition().toPoint().y() - 18;
            top->move(newX, newY);
            m_dragStartPos = event->globalPosition().toPoint() - top->frameGeometry().topLeft();
        } else {
            top->move(event->globalPosition().toPoint() - m_dragStartPos);
        }
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void TitleBar::mouseReleaseEvent(QMouseEvent *event)
{
    m_dragging = false;
    QWidget::mouseReleaseEvent(event);
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit signalMaxRestoreClicked();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void TitleBar::buildMaxIcons()
{
    m_maxIcon = QIcon();
    m_maxIcon.addPixmap(QPixmap(":/icons/maximize.svg"), QIcon::Normal);
    m_maxIcon.addPixmap(QPixmap(":/icons/maximize-hover.svg"), QIcon::Active);

    m_restoreIcon = QIcon();
    m_restoreIcon.addPixmap(QPixmap(":/icons/restore.svg"), QIcon::Normal);
    m_restoreIcon.addPixmap(QPixmap(":/icons/restore-hover.svg"), QIcon::Active);

    m_maxBtn->setIcon(m_maxIcon);
    m_maxBtn->setObjectName("maxBtn");
}