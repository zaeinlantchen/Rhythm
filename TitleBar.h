#ifndef TITLEBAR_H
#define TITLEBAR_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QColor>
#include <QPoint>

class QMenu;

class TitleBar : public QWidget
{
    Q_OBJECT

public:
    explicit TitleBar(QWidget *parent = nullptr);
    ~TitleBar() override;

    void setTitleIcon(const QString &iconPath, const QSize &size = QSize(20, 20));
    void setTitleContent(const QString &title, int fontSize = 11);
    void setBackgroundColor(const QColor &color);
    void setMaximized(bool maximized);

signals:
    void signalMinClicked();
    void signalMaxRestoreClicked();
    void signalCloseClicked();

    void signalSetting();
    void signalRescan();

    void signalAbout();
    void signalExit();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    void initUi();
    void initConnections();
    void buildHamburgerMenu();

    QLabel      *m_iconLabel  = nullptr;
    QLabel      *m_titleLabel = nullptr;
    QToolButton *m_menuBtn    = nullptr;
    QToolButton *m_minBtn     = nullptr;
    QToolButton *m_maxBtn     = nullptr;
    QToolButton *m_closeBtn   = nullptr;

    QColor m_bgColor;
    bool   m_maximized = false;
    bool   m_dragging  = false;
    QPoint m_dragStartPos;

    QIcon m_maxIcon;        // 未最大化：显示“最大化”图标
    QIcon m_restoreIcon;    // 已最大化：显示“还原”图标
    void  buildMaxIcons();  // 构造两套图标
};

#endif // TITLEBAR_H