#ifndef RHYTHM_H
#define RHYTHM_H

#include "Lyrics.h"
#include <QSlider>
#include <QMainWindow>
#include <QPropertyAnimation>
#include <QListWidgetItem>
#include <QSettings>
#include <QGroupBox>
#include <QPushButton>
#include <QLabel>
#include <QTableView>
#include <QHeaderView>
#include <QPixmap>
#include <QStandardPaths>
#include <QStatusBar>
#include <AudioSpectrumWidget.h>

#include <QMouseEvent>
#include <QWindow>
#include <QMenu>
#include <QWidgetAction>
#include <QVBoxLayout>
#include <QSlider>
#include <QLabel>
#include <QToolButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QCryptographicHash>
#include <AudioAnalyzer.h>
#include <QAudioBufferOutput>

#include <QInputDialog>
#include <QCursor>
#include <QSplitter>

#include <QCloseEvent>

#include <DesktopLyrics.h>
#include <QGraphicsOpacityEffect>

class QSortFilterProxyModel;

class AudioAnalyzer;
class AudioSpectrumWidget;

class PlayerController;
class MusicLibrary;
class TrackModel;
class Song;


QT_BEGIN_NAMESPACE
namespace Ui { class Rhythm; }
QT_END_NAMESPACE

class QSlider;

class Rhythm : public QMainWindow
{
    Q_OBJECT

public:
    explicit Rhythm(QWidget *parent = nullptr);
    ~Rhythm() override;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *e) override;

private slots:
    void onPlayPauseClicked();
    void onPrevClicked();
    void onNextClicked();
    void onProgressPressed();
    void onProgressReleased();
    void onProgressValueChanged(int v);
    void onNavChanged(int row);
    void onSearchTextChanged(const QString &text);

    void toggleQueuePanel();

    void cyclePlayMode();
    void setupProgressSliderOverlay();
    void repositionProgressSlider();

    void onPlaylistSelectionChanged();
    void onPlaylistSongDoubleClicked(QListWidgetItem *item);
    void onCreatePlaylist();
    void onRenamePlaylist();
    void onDeletePlaylist();
    void persistPlayQueue();

private:
    enum EdgeFlag {
        EdgeNone   = 0,
        EdgeLeft   = 1,
        EdgeRight  = 2,
        EdgeTop    = 4,
        EdgeBottom = 8
    };

    enum Volume {
        No = 0,
        Middle,
        Large
    };

    int  hitTest(const QPoint &pos) const;
    void setupTitleBar();
    void setupVolumePopup();
    void setupPlayer();
    void setupLibrary();
    void setupModelView();
    void setupQueueToggle();
    void updateNowPlayingUi(const Song &s);
    void updateResizeCursor(const QPoint &pos);
    void reloadSongsList();
    void buildvolumeIcon();
    void updatePlayModeIcon();
    static QString formatTime(qint64 ms);

    void setupSpectrum();

    void setPreviewWindow();
    void repositionSpectrum();

    void loadLyricsFor(const Song &song);
    void updateLyricsHighlight(qint64 pos);

    void showLyricsPage();
    void hideLyricsPage();

    void setLeftPanelVisible(bool visible);
    void setQueuePanelVisible(bool visible, bool animated = true);
    void updateLyricsCoverSize();

    void buildSettingsPage();
    void loadPathsToUi();
    void savePathsFromUi();

    void setupToast();
    void showToast(const QString &msg, int durationMs = 2500);
    void repositionToast();

    void saveAllSettings();

    // 各设置项的子函数（读/写）
    QJsonObject collectPathSettings() const;
    void        applyPathSettings(const QJsonObject &o);

    void buildPlaylistPage();
    void reloadPlaylists();
    void reloadPlaylistSongs(qint64 playlistId);

    void setupDesktopLyrics();
    void applyDesktopLyricsConfig();

    void updateWordBtnIcon();
    void showDesktopLyrics();

    void paintEvent(QPaintEvent *e) override;
    void applyBackgroundSettings();

    Ui::Rhythm *ui;
    QSlider    *m_volumeSlider = nullptr;

    PlayerController *m_player  = nullptr;
    MusicLibrary     *m_library = nullptr;
    TrackModel       *m_model   = nullptr;
    QSortFilterProxyModel *m_proxy = nullptr;

    QPropertyAnimation *m_queueAnim = nullptr;

    AudioAnalyzer       *m_analyzer = nullptr;
    AudioSpectrumWidget *m_spectrum = nullptr;

    QLineEdit *m_songFolderEdit   = nullptr;
    QLineEdit *m_lyricsFolderEdit = nullptr;
    QLabel      *m_scanStatusLabel  = nullptr;

    QLabel *m_toast = nullptr;
    QTimer *m_toastTimer = nullptr;

    QListWidget *m_playlistList        = nullptr;
    QListWidget *m_playlistSongsList   = nullptr;
    QLabel      *m_playlistHeader      = nullptr;
    QTimer      *m_queueSaveTimer      = nullptr;

    DesktopLyrics *m_desktopLyrics = nullptr;
    QAction *m_desktopLyricsAction = nullptr;

    QLineEdit *m_bgPathEdit       = nullptr;
    QSlider   *m_bgOpacitySlider  = nullptr;
    QLabel    *m_bgOpacityLabel   = nullptr;

    int m_queuePanelWidth = 300;

    bool m_seeking = false;

    int m_level = Volume::No;

    QWidget *m_volumePopup = nullptr;

    int m_leftPanelWidth = 200;
    int m_panelOpacity = 220;
    qint64       m_currentPlaylistId   = -1;

    QPixmap m_bgPixmap;

    QString m_searchKeyword;
};

#endif // RHYTHM_H
