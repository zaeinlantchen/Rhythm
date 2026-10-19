#include "Rhythm.h"
#include "ui_Rhythm.h"
#include "TitleBar.h"
#include "PlayerController.h"
#include "MusicLibrary.h"
#include "TrackModel.h"
#include "SongCard.h"
#include "QueueItemWidget.h"

#define kCoverSide 52

namespace SettingsKey {
    constexpr auto Path  = "path";
    constexpr auto Theme = "theme";        // 预留
    constexpr auto Lang  = "language";     // 预留
}

Rhythm::Rhythm(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::Rhythm)
{
    ui->setupUi(this);
    // 无系统边框
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setMinimumSize(900, 600);
    setMouseTracking(true);
    setWindowIcon(QIcon(":/site/icon.ico"));
    ui->centralwidget->setMouseTracking(true);

    ui->playmode->setAutoRaise(true);
    ui->volumeBtn->setAutoRaise(true);
    ui->tracklist->setAutoRaise(true);
    ui->wordBtn->setAutoRaise(true);

    ui->prevBtn->setNormalIconSize(QSize(20, 20));
    ui->prevBtn->setHoverScale(0.8);

    ui->playPauseBtn->setNormalIconSize(QSize(24, 24));
    ui->playPauseBtn->setHoverScale(0.8);

    ui->nextBtn->setNormalIconSize(QSize(20, 20));
    ui->nextBtn->setHoverScale(0.8);

    ui->prevBtn->setIcon(QIcon(":/icons/prev.svg"));
    ui->playPauseBtn->setIcon(QIcon(":/icons/play.svg"));
    ui->nextBtn->setIcon(QIcon(":/icons/next.svg"));

    QIcon listIcon;
    listIcon.addPixmap(QPixmap(":/icons/music-list.svg"),       QIcon::Normal);
    listIcon.addPixmap(QPixmap(":/icons/music-list-hover.svg"), QIcon::Active);

    ui->tracklist->setIcon(listIcon);

    ui->volumeBtn->setIconSize(QSize(20, 20));

    ui->wordBtn->setCheckable(true);

    ui->progressSlider->setAttribute(Qt::WA_Hover, true);
    ui->progressSlider->setMouseTracking(true);

    ui->playmode->setFocusPolicy(Qt::NoFocus);
    ui->playmode->setIconSize(QSize(20, 20));

    setupToast();

    setupTitleBar();
    setupVolumePopup();
    setupPlayer();
    setupLibrary();
    setupModelView();

    buildSettingsPage();

    buildPlaylistPage();

    m_leftPanelWidth = ui->leftPanel->width();
    ui->leftPanel->setMinimumWidth(0);

    setupQueueToggle();
    updatePlayModeIcon();
    setupProgressSliderOverlay();

    setupSpectrum();

    setupDesktopLyrics();

    showDesktopLyrics();

    applyBackgroundSettings();
    // 设置页面显示时自动同步到 library（不自动扫描）
    connect(ui->titleBar, &TitleBar::signalSetting, this, [this]{
        ui->navList->setCurrentRow(3);   // 切到设置页
        loadPathsToUi();
    });

    connect(ui->titleBar, &TitleBar::signalRescan, this, [this]{
        m_library->rescan();
    });

    connect(m_desktopLyrics, &DesktopLyrics::requestClose, this, [this]{
        if (ui->wordBtn->isChecked()) {
            ui->wordBtn->setChecked(false);   // 触发 toggled → 隐藏 + 图标恢复 + 持久化
            qDebug() << "icon is change";
            updateWordBtnIcon();
        } else {
            m_desktopLyrics->hide();
        }
    });

    QTimer::singleShot(0, this, [this]{
        const auto cfg = m_library->pathsConfig();
        if (!cfg.songFolder.isEmpty())
            m_library->rescan();
    });

    m_queueSaveTimer = new QTimer(this);
    m_queueSaveTimer->setSingleShot(true);
    m_queueSaveTimer->setInterval(500);
    connect(m_queueSaveTimer, &QTimer::timeout, this, &Rhythm::persistPlayQueue);

    // 监听窗口状态变化，同步 TitleBar 上的最大化/还原图标
    qApp->installEventFilter(this);
}

Rhythm::~Rhythm()
{
    if (m_desktopLyrics) {
        m_desktopLyrics->hide();
        delete m_desktopLyrics;
        m_desktopLyrics = nullptr;
    }
    delete ui;
}

// -------------------- 播放器 --------------------

void Rhythm::setupPlayer()
{
    m_player = new PlayerController(this);

    connect(ui->playPauseBtn, &QPushButton::clicked,
            this, &Rhythm::onPlayPauseClicked);
    connect(ui->prevBtn, &QPushButton::clicked,
            this, &Rhythm::onPrevClicked);
    connect(ui->nextBtn, &QPushButton::clicked,
            this, &Rhythm::onNextClicked);

    connect(ui->playmode, &QToolButton::clicked,
            this, &Rhythm::cyclePlayMode);

    // 进度条：区分“拖动中”和“播放中”，避免互相打架
    connect(ui->progressSlider, &QSlider::sliderPressed,
            this, &Rhythm::onProgressPressed);
    connect(ui->progressSlider, &QSlider::sliderReleased,
            this, &Rhythm::onProgressReleased);
    connect(ui->progressSlider, &QSlider::valueChanged,
            this, &Rhythm::onProgressValueChanged);

    // 播放器 → UI
    connect(m_player, &PlayerController::durationChanged, this, [this](qint64 dur){
        ui->progressSlider->setRange(0, int(dur));
        ui->totalTimeLabel->setText(formatTime(dur));
    });

    connect(m_player, &PlayerController::stateChanged, this,
            [this](QMediaPlayer::PlaybackState st){
                const bool playing = (st == QMediaPlayer::PlayingState);
                ui->playPauseBtn->setIcon(playing
                                              ? QIcon(":/icons/pause.svg")
                                              : QIcon(":/icons/play.svg"));
            });

    connect(m_player, &PlayerController::errorOccurred, this,
            [this](const QString &msg){
                showToast(msg, 3000);
            });

    connect(ui->queueList, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem *item){
                const int row = ui->queueList->row(item);
                if (row >= 0 && row < m_player->queue().size())
                    m_player->playAt(row);
            });

    connect(m_player, &PlayerController::queueChanged, this,
            [this](const QList<Song> &queue){
                ui->queueList->clear();
                for (const Song &s : queue) {
                    auto *item = new QListWidgetItem(ui->queueList);
                    item->setSizeHint(QSize(0, kCoverSide + 12));   // 40 + 上下各 6 margin
                    item->setData(Qt::UserRole, s.id);               // 存 id 便于比对

                    auto *w = new QueueItemWidget(ui->queueList);
                    w->setSong(s);
                    ui->queueList->addItem(item);
                    ui->queueList->setItemWidget(item, w);
                }
                // 队列重建后，同步一次“当前播放高亮”
                // 触发 currentSongChanged 也可，见下
            });

    connect(m_player, &PlayerController::currentSongChanged, this,
            [this](const Song &s){
                updateNowPlayingUi(s);
                loadLyricsFor(s);
                for (int i = 0; i < ui->queueList->count(); ++i) {
                    auto *item = ui->queueList->item(i);
                    auto *w = qobject_cast<QueueItemWidget*>(ui->queueList->itemWidget(item));
                    if (!w) continue;
                    const qint64 id = item->data(Qt::UserRole).toLongLong();
                    w->setPlaying(id == s.id && s.isValid());
                }
            });

    // 播放进度 → 更新歌词高亮
    connect(m_player, &PlayerController::positionChanged, this, [this](qint64 pos){
        if (m_seeking) return;
        ui->progressSlider->setValue(int(pos));
        ui->curTimeLabel->setText(formatTime(pos));
        updateLyricsHighlight(pos);      // ← 新增
    });

    connect(ui->coverLabel, &SquareImageLabel::clicked, this, [this]{
        if (ui->pages->currentIndex() == 1)
            hideLyricsPage();
        else
            showLyricsPage();
    });

    // 队列 / 当前曲变化 → 排期保存
    connect(m_player, &PlayerController::queueChanged, this, [this]{
        if (m_queueSaveTimer) m_queueSaveTimer->start();
    });
    connect(m_player, &PlayerController::currentSongChanged, this, [this]{
        if (m_queueSaveTimer) m_queueSaveTimer->start();
    });

    // 启动时恢复
    QTimer::singleShot(0, this, [this]{
        int idx = -1;
        const QList<Song> songs = m_library->loadPlayQueue(idx);
        if (!songs.isEmpty())
            m_player->restoreQueue(songs, idx);

        const QJsonObject modeObj = m_library->configValue("play_mode");
        if (modeObj.contains("mode")) {
            m_player->setPlayMode(static_cast<PlayerController::PlayMode>(
                modeObj.value("mode").toInt()));
            updatePlayModeIcon();
        }

        const QJsonObject volObj = m_library->configValue("volume");
        if (volObj.contains("value")) {
            const int v = volObj.value("value").toInt();
            m_volumeSlider->setValue(v);
        }
    });

}

void Rhythm::onPlayPauseClicked() { m_player->togglePlayPause(); }
void Rhythm::onPrevClicked()      { m_player->previous(); }
void Rhythm::onNextClicked()      { m_player->next(); }

void Rhythm::onProgressPressed()
{
    m_seeking = true;
}

void Rhythm::onProgressReleased()
{
    m_seeking = false;
    m_player->setPosition(ui->progressSlider->value());
}

void Rhythm::onProgressValueChanged(int v)
{
    // 拖动中实时显示时间
    if (!m_seeking) return;
    ui->curTimeLabel->setText(formatTime(v));
    updateLyricsHighlight(v);
}

// -------------------- 媒体库 --------------------

void Rhythm::setupLibrary()
{
    m_library = new MusicLibrary(this);
    if (!m_library->open())
        showToast("音乐库初始化失败！", 3000);

    connect(m_library, &MusicLibrary::libraryChanged, this, [this]{
        m_model->setSongs(m_library->allSongs());
        reloadSongsList();
    });

    connect(m_library, &MusicLibrary::scanProgress, this, [this](int cur, int total){
        showToast("", 3000);
    });

    connect(m_library, &MusicLibrary::scanFinished, this, [this](int added){
        showToast(QStringLiteral("扫描完成，新增 %1 首").arg(added), 3000);
    });

    // 首次启动就把已有歌曲读出来
    // 注意：TrackModel 已在 setupModelView 之前准备好，这里延后到 setupModelView 结束
}

// -------------------- 模型/视图 --------------------

void Rhythm::setupModelView()
{
    m_model = new TrackModel(this);
    m_proxy = new SongFilterProxy(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setFilterKeyColumn(-1);   // 关键：所有列参与过滤

    // 加载已有歌曲
    m_model->setSongs(m_library->allSongs());

    reloadSongsList();

    // 搜索
    connect(ui->searchInput, &QLineEdit::textChanged,
            this, &Rhythm::onSearchTextChanged);

    // 左侧导航
    connect(ui->navList, &QListWidget::currentRowChanged,
            this, &Rhythm::onNavChanged);

    ui->navList->setCurrentRow(0);
}

void Rhythm::reloadSongsList()
{
    ui->songsList->clear();

    QList<Song> songs = m_library->allSongs();

    // 按关键字过滤（标题 / 艺术家 / 专辑，不区分大小写）
    if (!m_searchKeyword.isEmpty()) {
        const QString kw = m_searchKeyword.toLower();
        QList<Song> filtered;
        filtered.reserve(songs.size());
        for (const Song &s : songs) {
            if (s.title.toLower().contains(kw)
                || s.artist.toLower().contains(kw)
                || s.album.toLower().contains(kw)) {
                filtered.append(s);
            }
        }
        songs = filtered;
    }

    // 没结果时给个提示
    if (songs.isEmpty()) {
        auto *item = new QListWidgetItem(ui->songsList);
        item->setFlags(Qt::NoItemFlags);
        item->setSizeHint(QSize(0, 80));
        auto *tip = new QLabel(QStringLiteral("没有匹配的歌曲"), ui->songsList);
        tip->setAlignment(Qt::AlignCenter);
        tip->setObjectName("emptyTip");
        ui->songsList->addItem(item);
        ui->songsList->setItemWidget(item, tip);
        return;
    }

    for (const Song &s : songs) {
        auto *item = new QListWidgetItem(ui->songsList);
        auto *card = new SongCard(ui->songsList);
        card->setSong(s);

        // 让 item 尺寸与卡片一致
        item->setSizeHint(QSize(card->width(), card->sizeHint().height()));

        ui->songsList->addItem(item);
        ui->songsList->setItemWidget(item, card);

        connect(card, &SongCard::activated, this, [this, songs](const Song &song){
            m_player->enqueueAndPlay(song);
        });

        connect(card, &SongCard::contextMenuRequested, this,
                [this](const Song &song, const QPoint &pos){
                    QMenu menu;
                    menu.addAction(QStringLiteral("播放"), this, [this, song]{
                        const auto all = m_library->allSongs();
                        m_player->enqueueAndPlay(song);
                    });
                    menu.addSeparator();
                    menu.addAction(QStringLiteral("更换封面..."), this, [this, song]{
                        const QString f = QFileDialog::getOpenFileName(
                            this, QStringLiteral("选择封面图片"), QString(),
                            QStringLiteral("图片 (*.png *.jpg *.jpeg *.bmp *.gif)"));
                        if (f.isEmpty()) return;
                        m_library->updateCover(song.id, f);
                    });

                    menu.addSeparator();

                    if (song.lyricsPath.isEmpty()) {
                        menu.addAction(QStringLiteral("导入歌词..."), this, [this, song]{
                            const QString f = QFileDialog::getOpenFileName(
                                this, QStringLiteral("选择歌词文件"), QString(),
                                QStringLiteral("LRC 歌词 (*.lrc *.LRC);;所有文件 (*)"));
                            if (f.isEmpty()) return;

                            // 拷贝到 AppData/lyrics/ 防止原文件被移走
                            const QString dir = QStandardPaths::writableLocation(
                                                    QStandardPaths::AppDataLocation) + "/lyrics";
                            QDir().mkpath(dir);
                            const QByteArray hash = QCryptographicHash::hash(
                                                        song.filePath.toUtf8(), QCryptographicHash::Md5).toHex();
                            const QString dst = dir + "/" + QString::fromLatin1(hash) + ".lrc";

                            if (QFile::exists(dst)) QFile::remove(dst);
                            if (!QFile::copy(f, dst)) {
                                QMessageBox::warning(this, QString(),
                                                     QStringLiteral("歌词文件复制失败"));
                                return;
                            }
                            m_library->updateLyrics(song.id, dst);

                            // 如果当前正在播放这首歌，立即刷新歌词
                            if (m_player->currentSong().id == song.id)
                                loadLyricsFor(m_player->currentSong());
                        });
                    } else {
                        menu.addAction(QStringLiteral("更换歌词..."), this, [this, song]{
                            const QString f = QFileDialog::getOpenFileName(
                                this, QStringLiteral("选择歌词文件"), QString(),
                                QStringLiteral("LRC 歌词 (*.lrc *.LRC);;所有文件 (*)"));
                            if (f.isEmpty()) return;

                            const QString dir = QStandardPaths::writableLocation(
                                                    QStandardPaths::AppDataLocation) + "/lyrics";
                            QDir().mkpath(dir);
                            const QByteArray hash = QCryptographicHash::hash(
                                                        song.filePath.toUtf8(), QCryptographicHash::Md5).toHex();
                            const QString dst = dir + "/" + QString::fromLatin1(hash) + ".lrc";

                            if (QFile::exists(dst)) QFile::remove(dst);
                            QFile::copy(f, dst);
                            m_library->updateLyrics(song.id, dst);

                            if (m_player->currentSong().id == song.id)
                                loadLyricsFor(m_player->currentSong());
                        });

                        menu.addAction(QStringLiteral("移除歌词"), this, [this, song]{
                            const auto ret = QMessageBox::question(
                                this, QStringLiteral("移除歌词"),
                                QStringLiteral("确定要移除该歌曲的歌词吗？"),
                                QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::No),
                                QMessageBox::No);
                            if (ret != QMessageBox::Yes) return;

                            // 如果歌词文件是我们拷贝过来的，也一起删掉
                            if (song.lyricsPath.startsWith(
                                    QStandardPaths::writableLocation(
                                        QStandardPaths::AppDataLocation) + "/lyrics/"))
                                QFile::remove(song.lyricsPath);

                            m_library->updateLyrics(song.id, QString());

                            if (m_player->currentSong().id == song.id)
                                loadLyricsFor(m_player->currentSong());
                        });
                    }

                    menu.addSeparator();
                    menu.addAction(QStringLiteral("从音乐库移除"), this, [this, song]{
                        const auto ret = QMessageBox::question(
                            this,
                            QStringLiteral("确认删除"),
                            QStringLiteral("确定要从音乐库移除《%1》吗？\n"
                                           "（不会删除磁盘上的音频文件）").arg(song.title),
                            QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::No),
                            QMessageBox::No);
                        if (ret != QMessageBox::Yes) return;

                        // 先同步播放器队列，再删库
                        m_player->removeSongsFromQueue({ song.id });
                        m_library->removeSong(song.id);
                    });

                    menu.addSeparator();
                    menu.addAction(QStringLiteral("添加到歌单..."), this, [this, song]{
                        QMenu submenu;
                        const auto lists = m_library->playlists();
                        for (const auto &pl : lists) {
                            submenu.addAction(pl.name, this, [this, pl, song]{
                                m_library->addSongToPlaylist(pl.id, song.id);
                            });
                        }
                        if (!lists.isEmpty()) submenu.addSeparator();
                        submenu.addAction(QStringLiteral("新建歌单..."), this, [this, song]{
                            bool ok = false;
                            const QString name = QInputDialog::getText(
                                this, QStringLiteral("新建歌单"),
                                QStringLiteral("歌单名称："), QLineEdit::Normal,
                                QStringLiteral("新歌单"), &ok);
                            if (!ok || name.trimmed().isEmpty()) return;
                            const qint64 id = m_library->createPlaylist(name.trimmed());
                            if (id > 0) m_library->addSongToPlaylist(id, song.id);
                        });
                        submenu.exec(QCursor::pos());
                    });

                    menu.exec(pos);
                });
    }
}

void Rhythm::onSearchTextChanged(const QString &text)
{
    m_searchKeyword = ui->searchInput->text().trimmed();
    reloadSongsList();                  // 用新关键字重建卡片
    ui->searchInput->clearFocus();
    // 或使用正则：m_proxy->setFilterRegularExpression(QRegularExpression::escape(text));
}

void Rhythm::onNavChanged(int row)
{
    // if (row < 0 || row >= ui->pages->count()) return;
    // ui->pages->setCurrentIndex(row);
    switch (row) {
        case 0:{
            ui->pages->setCurrentIndex(row);
            break;
        }
        case 1:{
            ui->pages->setCurrentIndex(2);
            break;
        }
        default:
            break;
    }
}

// -------------------- UI 刷新 --------------------

void Rhythm::updateNowPlayingUi(const Song &s)
{
    ui->titleLabel->setText(s.title.isEmpty() ? QStringLiteral("未播放") : s.title);
    ui->artistLabel->setText(s.artist.isEmpty() ? QStringLiteral("—") : s.artist);
    ui->coverLabel->setSource(s.coverPath);

    // 歌词页面里的封面同步
    if (ui->lyricsCover)
        ui->lyricsCover->setSource(s.coverPath);
}

void Rhythm::updateResizeCursor(const QPoint &pos)
{
    if (isMaximized()) {
        unsetCursor();
        return;
    }

    const int edge = hitTest(pos);

    const bool l = edge & EdgeLeft;
    const bool r = edge & EdgeRight;
    const bool t = edge & EdgeTop;
    const bool b = edge & EdgeBottom;

    if ((t && l) || (b && r))       setCursor(Qt::SizeFDiagCursor);
    else if ((t && r) || (b && l))  setCursor(Qt::SizeBDiagCursor);
    else if (l || r)                setCursor(Qt::SizeHorCursor);
    else if (t || b)                setCursor(Qt::SizeVerCursor);
    else                            unsetCursor();
}

QString Rhythm::formatTime(qint64 ms)
{
    if (ms < 0) ms = 0;
    const qint64 total = ms / 1000;
    const qint64 h = total / 3600;
    const qint64 m = (total % 3600) / 60;
    const qint64 s = total % 60;
    if (h > 0)
        return QString("%1:%2:%3")
            .arg(h)
            .arg(m, 2, 10, QChar('0'))
            .arg(s, 2, 10, QChar('0'));
    return QString("%1:%2")
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'));
}

void Rhythm::setupSpectrum()
{
    // 分析器
    m_analyzer = new AudioAnalyzer(this);
    m_analyzer->setBarCount(84);
    m_analyzer->setFreqRange(40.0, 16000.0);

    // 频谱 widget，父控件是 playerBar
    m_spectrum = new AudioSpectrumWidget(ui->playerBar);
    m_spectrum->setBarCount(84);
    m_spectrum->setBarGap(1);
    m_spectrum->setTopColor(QColor(0x33, 0xDF, 0xCB, 0xB0));
    m_spectrum->setBottomColor(QColor(0x33, 0xDF, 0xCB, 0x30));
    m_spectrum->lower();              // 沉到 playerBar 内容之下
    m_spectrum->setVisible(false);

    // 接线：QMediaPlayer buffer -> 分析器 -> widget
    connect(m_player->audioBufferOutput(),
            &QAudioBufferOutput::audioBufferReceived,
            m_analyzer, &AudioAnalyzer::processBuffer);

    connect(m_analyzer, &AudioAnalyzer::spectrumReady,
            m_spectrum, &AudioSpectrumWidget::pushBars);

    // 播放时显示，暂停/停止时隐藏
    connect(m_player, &PlayerController::stateChanged, this,
            [this](QMediaPlayer::PlaybackState st){
                const bool playing = (st == QMediaPlayer::PlayingState);
                m_spectrum->setVisible(playing);
                if (!playing) m_spectrum->reset();
            });

    repositionSpectrum();
}

void Rhythm::repositionSpectrum()
{
    if (!ui->playerBar || !m_spectrum) return;

    const int pw = ui->playerBar->width();
    const int ph = ui->playerBar->height();
    if (pw <= 0 || ph <= 0) return;

    const int sw = int(pw * 0.6);            // 宽 60%
    const int sx = (pw - sw) / 2;            // 居中
    const int sy = 6;                        // 上下留 6px
    const int sh = qMax(1, ph - 12);

    m_spectrum->setGeometry(sx, sy, sw, sh);
}

void Rhythm::setLeftPanelVisible(bool visible)
{
    auto *left = ui->leftPanel;
    if (visible) {
        left->setMaximumWidth(m_leftPanelWidth);
    } else {
        const int w = left->width();
        if (w > 0) m_leftPanelWidth = w;
        left->setMaximumWidth(0);
    }
}

void Rhythm::setQueuePanelVisible(bool visible, bool animated)
{
    if (animated) {
        if (m_queueAnim->state() == QAbstractAnimation::Running)
            return;

        if (visible) {
            m_queueAnim->setStartValue(ui->rightPanel->maximumWidth());
            m_queueAnim->setEndValue(m_queuePanelWidth);
        } else {
            const int w = ui->rightPanel->width();
            if (w > 0) m_queuePanelWidth = w;
            m_queueAnim->setStartValue(ui->rightPanel->maximumWidth());
            m_queueAnim->setEndValue(0);
        }
        m_queueAnim->start();
    } else {
        if (visible) {
            ui->rightPanel->setMaximumWidth(m_queuePanelWidth);
        } else {
            const int w = ui->rightPanel->width();
            if (w > 0) m_queuePanelWidth = w;
            ui->rightPanel->setMaximumWidth(0);
        }
    }
}

void Rhythm::showLyricsPage()
{
    setLeftPanelVisible(false);
    if (ui->rightPanel->maximumWidth() > 0)
        setQueuePanelVisible(false, false);

    ui->lyricsCoverArea->setVisible(true);

    const Song cur = m_player->currentSong();
    ui->lyricsCover->setSource(cur.coverPath);

    ui->pages->setCurrentIndex(1);
    QTimer::singleShot(0, this, [this]{ updateLyricsCoverSize(); });
}

void Rhythm::hideLyricsPage()
{
    setLeftPanelVisible(true);
    ui->pages->setCurrentIndex(0);
}

void Rhythm::updateLyricsCoverSize()
{
    if (!ui->lyricsCover || !ui->lyricsCoverArea) return;
    if (!ui->lyricsCoverArea->isVisible()) return;
    const int w = ui->lyricsCoverArea->width();
    const int h = ui->lyricsCoverArea->height();
    if (w <= 0 || h <= 0) return;

    // 取短边并留边距；范围 [120, 400]
    const int side = qBound(200, qMin(w, h) - 40, 400);
    ui->lyricsCover->setFixedSize(side, side);
}

void Rhythm::buildSettingsPage()
{
    auto *root = qobject_cast<QVBoxLayout*>(ui->pageSettings->layout());
    if (!root) return;

    // ① 标题 + 说明
    auto *title = new QLabel(QStringLiteral("音乐库路径"), ui->pageSettings);
    title->setObjectName("settingsTitle");
    root->addWidget(title);

    auto *hint = new QLabel(
        QStringLiteral("设置歌曲文件夹与对应的歌词文件夹。"
                       "保存后会自动在后台扫描：\n"
                       "• 元信息（标题 / 艺术家 / 专辑）相同的歌曲会被去重；\n"
                       "• 歌词按同名 .lrc 匹配，未找到则留空。"),
        ui->pageSettings);
    hint->setWordWrap(true);
    hint->setObjectName("settingsHint");
    root->addWidget(hint);

    // ② 表单
    auto *form = new QGridLayout();
    form->setHorizontalSpacing(8);
    form->setVerticalSpacing(10);

    auto makeRow = [&](int row, const QString &label,
                       QLineEdit *&edit, const QString &placeholder){
        auto *lb = new QLabel(label, ui->pageSettings);
        edit = new QLineEdit(ui->pageSettings);
        edit->setPlaceholderText(placeholder);
        edit->setReadOnly(true);

        auto *browse = new QPushButton(QStringLiteral("浏览..."), ui->pageSettings);

        form->addWidget(lb,     row, 0);
        form->addWidget(edit,   row, 1);
        form->addWidget(browse, row, 2);

        connect(browse, &QPushButton::clicked, this, [this, edit, label]{
            const QString dir = QFileDialog::getExistingDirectory(this, label);
            if (!dir.isEmpty()) edit->setText(QDir::cleanPath(dir));
        });
    };

    makeRow(0, QStringLiteral("歌曲文件夹"),
            m_songFolderEdit,   QStringLiteral("未选择"));
    makeRow(1, QStringLiteral("歌词文件夹"),
            m_lyricsFolderEdit, QStringLiteral("可留空；留空时仅尝试音频同目录的同名 .lrc"));

    // ---- 背景图片 ----
    auto *bgLbl = new QLabel(QStringLiteral("背景图片"), ui->pageSettings);
    m_bgPathEdit = new QLineEdit(ui->pageSettings);
    m_bgPathEdit->setPlaceholderText(QStringLiteral("未选择"));
    m_bgPathEdit->setReadOnly(true);

    auto *bgBrowse = new QPushButton(QStringLiteral("浏览..."), ui->pageSettings);
    auto *bgClear  = new QPushButton(QStringLiteral("清除"), ui->pageSettings);

    connect(bgBrowse, &QPushButton::clicked, this, [this]{
        const QString f = QFileDialog::getOpenFileName(this,
                                                       QStringLiteral("选择背景图片"), QString(),
                                                       QStringLiteral("图片 (*.png *.jpg *.jpeg *.bmp)"));
        if (!f.isEmpty()) m_bgPathEdit->setText(QDir::cleanPath(f));
    });
    connect(bgClear, &QPushButton::clicked, this, [this]{
        m_bgPathEdit->clear();
    });

    form->addWidget(bgLbl,        2, 0);
    form->addWidget(m_bgPathEdit, 2, 1);

    auto *bgBtns = new QHBoxLayout();
    bgBtns->setContentsMargins(0, 0, 0, 0);
    bgBtns->setSpacing(8);
    bgBtns->addWidget(bgBrowse);
    bgBtns->addWidget(bgClear);
    form->addLayout(bgBtns, 2, 2);

    // ---- 控件透明度 ----
    auto *opacityLbl = new QLabel(QStringLiteral("控件透明度"), ui->pageSettings);

    m_bgOpacitySlider = new QSlider(Qt::Horizontal, ui->pageSettings);
    m_bgOpacitySlider->setRange(0, 90);
    m_bgOpacitySlider->setValue(86);

    m_bgOpacityLabel = new QLabel(QStringLiteral("86%"), ui->pageSettings);
    m_bgOpacityLabel->setMinimumWidth(36);
    m_bgOpacityLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto *opacityBox = new QHBoxLayout();
    opacityBox->setContentsMargins(0, 0, 0, 0);
    opacityBox->addWidget(m_bgOpacitySlider);
    opacityBox->addWidget(m_bgOpacityLabel);

    form->addWidget(opacityLbl, 3, 0);
    form->addLayout(opacityBox, 3, 1, 1, 2);

    // 实时预览
    connect(m_bgOpacitySlider, &QSlider::valueChanged, this, [this](int v){
        m_bgOpacityLabel->setText(QString::number(v) + "%");
        m_panelOpacity = qRound(v / 100.0 * 255.0);
        update(); // 实时重绘主窗口，预览遮罩效果
    });

    auto *clearRow = new QHBoxLayout();
    auto *clearBtn = new QPushButton(QStringLiteral("清空"), ui->pageSettings);
    clearRow->addWidget(clearBtn);
    clearRow->addStretch();
    form->addLayout(clearRow, 2, 0, 1, 3);

    root->addLayout(form);

    connect(clearBtn, &QPushButton::clicked, this, [this]{
        m_songFolderEdit->clear();
        m_lyricsFolderEdit->clear();
    });

    // ③ 保存按钮
    auto *btnRow = new QHBoxLayout();
    auto *saveBtn = new QPushButton(QStringLiteral("保存"), ui->pageSettings);
    saveBtn->setObjectName("settingsSaveBtn");
    btnRow->addStretch();
    btnRow->addWidget(saveBtn);
    root->addLayout(btnRow);

    connect(saveBtn, &QPushButton::clicked, this, [this]{
        savePathsFromUi();
        m_library->rescan();          // 保存后自动扫描
    });

    // ④ 扫描状态
    m_scanStatusLabel = new QLabel(ui->pageSettings);
    m_scanStatusLabel->setObjectName("scanStatusLabel");
    root->addWidget(m_scanStatusLabel);
    root->addStretch();

    connect(m_library, &MusicLibrary::scanStateChanged, this,
            [this](bool running){
                if (m_scanStatusLabel)
                    m_scanStatusLabel->setText(running ? QStringLiteral("正在扫描...")
                                                       : QString());
            });
    connect(m_library, &MusicLibrary::scanProgress, this,
            [this](int c, int t){
                if (m_scanStatusLabel && t > 0)
                    m_scanStatusLabel->setText(
                        QStringLiteral("扫描 %1 / %2").arg(c).arg(t));
            });
    connect(m_library, &MusicLibrary::scanFinished, this, [this](int added){
        if (m_scanStatusLabel)
            m_scanStatusLabel->setText(
                QStringLiteral("扫描完成，新增 %1 首").arg(added));
    });

    loadPathsToUi();
}

void Rhythm::loadPathsToUi()
{
    // 读取路径
    const auto cfg = m_library->pathsConfig();
    m_songFolderEdit->setText(cfg.songFolder);
    m_lyricsFolderEdit->setText(cfg.lyricsFolder);

    // 读取背景配置到 UI 控件
    const QJsonObject bg = m_library->configValue("app_background");
    m_bgPathEdit->setText(bg.value("path").toString());

    qreal op = bg.value("opacity").toDouble(0.86);
    int sliderVal = qRound(op * 100.0);
    m_bgOpacitySlider->setValue(sliderVal);
    m_bgOpacityLabel->setText(QString::number(sliderVal) + "%");

    applyBackgroundSettings();
}

void Rhythm::savePathsFromUi()
{
    // 保存路径
    MusicLibrary::PathsConfig cfg;
    cfg.songFolder   = m_songFolderEdit->text().trimmed();
    cfg.lyricsFolder = m_lyricsFolderEdit->text().trimmed();
    m_library->setPathsConfig(cfg);

    // 保存背景配置
    QJsonObject bg;
    bg.insert("path", m_bgPathEdit->text().trimmed());
    bg.insert("opacity", m_bgOpacitySlider->value() / 255.0);
    qreal op = m_bgOpacitySlider->value() / 100.0;
    bg.insert("opacity", op);
    m_library->setConfigValue("app_background", bg);

    applyBackgroundSettings();
}

void Rhythm::setupToast()
{
    m_toast = new QLabel(this);
    m_toast->setObjectName("appToast");
    m_toast->setAlignment(Qt::AlignCenter);
    m_toast->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_toast->setVisible(false);

    m_toastTimer = new QTimer(this);
    m_toastTimer->setSingleShot(true);
    connect(m_toastTimer, &QTimer::timeout, this, [this]{
        if (m_toast) m_toast->hide();
    });
}

void Rhythm::repositionToast()
{
    if (!m_toast || !m_toast->isVisible()) return;
    const int margin = 20;
    const int x = (width() - m_toast->width()) / 2;
    const int y = height() - m_toast->height() - ui->playerBar->height() - margin;
    m_toast->move(qMax(0, x), qMax(0, y));
}

void Rhythm::saveAllSettings()
{
    {
        const QJsonObject o = collectPathSettings();
        m_library->setConfigValue(SettingsKey::Path, o);
    }
    m_library->rescan();
}

QJsonObject Rhythm::collectPathSettings() const
{
    QJsonObject o;
    o.insert("song",   QDir::cleanPath(m_songFolderEdit->text().trimmed()));
    o.insert("lyrics", m_lyricsFolderEdit->text().isEmpty()
                           ? QString()
                           : QDir::cleanPath(m_lyricsFolderEdit->text().trimmed()));
    return o;
}

void Rhythm::applyPathSettings(const QJsonObject &o)
{
    m_songFolderEdit->setText(o.value("song").toString());
    m_lyricsFolderEdit->setText(o.value("lyrics").toString());
}

void Rhythm::buildPlaylistPage()
{
    auto *page = ui->pagePlaylist;
    if (!page) return;

    // 清空旧布局
    if (auto *old = page->layout()) {
        QLayoutItem *it;
        while ((it = old->takeAt(0))) {
            if (auto *w = it->widget()) w->deleteLater();
            delete it;
        }
        delete old;
    }

    auto *root = new QHBoxLayout(page);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    // ---- 左：歌单 ----
    auto *leftBox = new QWidget(page);
    auto *ll = new QVBoxLayout(leftBox);
    ll->setContentsMargins(0, 0, 0, 0);
    ll->setSpacing(6);

    auto *leftTitle = new QLabel(QStringLiteral("我的歌单"), leftBox);
    leftTitle->setObjectName("playlistSectionTitle");
    ll->addWidget(leftTitle);

    m_playlistList = new QListWidget(leftBox);
    m_playlistList->setObjectName("playlistsList");
    m_playlistList->setFrameShape(QFrame::NoFrame);
    m_playlistList->setContextMenuPolicy(Qt::CustomContextMenu);
    ll->addWidget(m_playlistList, 1);

    auto *lbtns = new QHBoxLayout();
    auto *newBtn = new QPushButton(QStringLiteral("新建"),  leftBox);
    auto *renBtn = new QPushButton(QStringLiteral("重命名"), leftBox);
    auto *delBtn = new QPushButton(QStringLiteral("删除"),  leftBox);
    lbtns->addWidget(newBtn);
    lbtns->addWidget(renBtn);
    lbtns->addWidget(delBtn);
    ll->addLayout(lbtns);

    // ---- 右：歌单里的歌 ----
    auto *rightBox = new QWidget(page);
    auto *rl = new QVBoxLayout(rightBox);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(6);

    m_playlistHeader = new QLabel(QStringLiteral("请选择歌单"), rightBox);
    m_playlistHeader->setObjectName("playlistSectionTitle");
    rl->addWidget(m_playlistHeader);

    m_playlistSongsList = new QListWidget(rightBox);
    m_playlistSongsList->setObjectName("playlistSongsList");
    m_playlistSongsList->setFrameShape(QFrame::NoFrame);
    m_playlistSongsList->setContextMenuPolicy(Qt::CustomContextMenu);
    rl->addWidget(m_playlistSongsList, 1);

    auto *rbtns = new QHBoxLayout();
    auto *playAllBtn  = new QPushButton(QStringLiteral("播放歌单"),     rightBox);
    auto *appendBtn   = new QPushButton(QStringLiteral("加入播放队列"), rightBox);
    auto *removeBtn   = new QPushButton(QStringLiteral("从歌单移除"),   rightBox);
    rbtns->addWidget(playAllBtn);
    rbtns->addWidget(appendBtn);
    rbtns->addStretch();
    rbtns->addWidget(removeBtn);
    rl->addLayout(rbtns);

    auto *splitter = new QSplitter(Qt::Horizontal, page);
    splitter->addWidget(leftBox);
    splitter->addWidget(rightBox);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 7);
    splitter->setChildrenCollapsible(false);
    root->addWidget(splitter);

    // ---- 连接 ----
    connect(m_playlistList, &QListWidget::itemSelectionChanged,
            this, &Rhythm::onPlaylistSelectionChanged);
    connect(m_playlistSongsList, &QListWidget::itemDoubleClicked,
            this, &Rhythm::onPlaylistSongDoubleClicked);

    connect(newBtn, &QPushButton::clicked, this, &Rhythm::onCreatePlaylist);
    connect(renBtn, &QPushButton::clicked, this, &Rhythm::onRenamePlaylist);
    connect(delBtn, &QPushButton::clicked, this, &Rhythm::onDeletePlaylist);

    connect(playAllBtn, &QPushButton::clicked, this, [this]{
        if (m_currentPlaylistId < 0) return;
        const auto songs = m_library->playlistSongs(m_currentPlaylistId);
        if (!songs.isEmpty()) m_player->setQueue(songs, 0);
    });
    connect(appendBtn, &QPushButton::clicked, this, [this]{
        if (m_currentPlaylistId < 0) return;
        m_player->appendToQueue(m_library->playlistSongs(m_currentPlaylistId));
    });
    connect(removeBtn, &QPushButton::clicked, this, [this]{
        auto *item = m_playlistSongsList->currentItem();
        if (!item || m_currentPlaylistId < 0) return;
        m_library->removeSongFromPlaylist(
            m_currentPlaylistId, item->data(Qt::UserRole).toLongLong());
    });

    // 歌单右键
    connect(m_playlistList, &QListWidget::customContextMenuRequested, this,
            [this](const QPoint &pos){
                auto *item = m_playlistList->itemAt(pos);
                QMenu menu;
                if (item) {
                    m_playlistList->setCurrentItem(item);
                    menu.addAction(QStringLiteral("重命名"), this, &Rhythm::onRenamePlaylist);
                    menu.addAction(QStringLiteral("删除"),   this, &Rhythm::onDeletePlaylist);
                    menu.addSeparator();
                }
                menu.addAction(QStringLiteral("新建歌单"), this, &Rhythm::onCreatePlaylist);
                menu.exec(m_playlistList->viewport()->mapToGlobal(pos));
            });

    // 歌单内歌曲右键
    connect(m_playlistSongsList, &QListWidget::customContextMenuRequested, this,
            [this](const QPoint &pos){
                auto *item = m_playlistSongsList->itemAt(pos);
                if (!item || m_currentPlaylistId < 0) return;
                m_playlistSongsList->setCurrentItem(item);
                const qint64 sid = item->data(Qt::UserRole).toLongLong();
                const auto songs = m_library->playlistSongs(m_currentPlaylistId);

                QMenu menu;
                menu.addAction(QStringLiteral("播放"), this, [this, sid, songs]{
                    for (int i = 0; i < songs.size(); ++i)
                        if (songs[i].id == sid) { m_player->setQueue(songs, i); return; }
                });
                menu.addAction(QStringLiteral("加入播放队列"), this, [this, sid, songs]{
                    for (const Song &s : songs)
                        if (s.id == sid) { m_player->enqueueAndPlay(s); return; }
                });
                menu.addSeparator();
                menu.addAction(QStringLiteral("从歌单移除"), this, [this, sid]{
                    m_library->removeSongFromPlaylist(m_currentPlaylistId, sid);
                });
                menu.exec(m_playlistSongsList->viewport()->mapToGlobal(pos));
            });

    // 数据变化刷新
    connect(m_library, &MusicLibrary::playlistsChanged,
            this, &Rhythm::reloadPlaylists);
    connect(m_library, &MusicLibrary::playlistSongsChanged, this,
            [this](qint64 id){
                if (id == m_currentPlaylistId) reloadPlaylistSongs(id);
            });

    reloadPlaylists();
}

void Rhythm::reloadPlaylists()
{
    if (!m_playlistList) return;

    const qint64 prev = m_currentPlaylistId;
    m_playlistList->clear();

    const auto lists = m_library->playlists();
    int selectRow = -1;
    for (int i = 0; i < lists.size(); ++i) {
        const auto &pl = lists[i];
        auto *item = new QListWidgetItem(
            QStringLiteral("%1  (%2)").arg(pl.name).arg(pl.songCount),
            m_playlistList);
        item->setData(Qt::UserRole, pl.id);
        if (pl.id == prev) selectRow = i;
    }

    if (selectRow >= 0)          m_playlistList->setCurrentRow(selectRow);
    else if (m_playlistList->count() > 0) m_playlistList->setCurrentRow(0);
    else {
        m_currentPlaylistId = -1;
        if (m_playlistSongsList) m_playlistSongsList->clear();
        if (m_playlistHeader) m_playlistHeader->setText(QStringLiteral("请选择歌单"));
    }
}

void Rhythm::reloadPlaylistSongs(qint64 playlistId)
{
    if (!m_playlistSongsList) return;
    m_playlistSongsList->clear();
    if (playlistId < 0) return;

    const auto pl = m_library->playlistById(playlistId);
    if (m_playlistHeader)
        m_playlistHeader->setText(QStringLiteral("%1 · %2 首").arg(pl.name).arg(pl.songCount));

    for (const Song &s : m_library->playlistSongs(playlistId)) {
        auto *item = new QListWidgetItem(
            QStringLiteral("%1  —  %2").arg(s.title, s.artist),
            m_playlistSongsList);
        item->setData(Qt::UserRole, s.id);
    }
}

void Rhythm::setupDesktopLyrics()
{
    m_desktopLyrics = new DesktopLyrics(nullptr);
    m_desktopLyrics->setObjectName("desktopLyrics");
    // 默认位置：屏幕下方居中偏上
    if (auto *screen = QGuiApplication::primaryScreen()) {
        const QRect r = screen->availableGeometry();
        const QSize sz = m_desktopLyrics->sizeHint().expandedTo(QSize(800, 100));
        m_desktopLyrics->resize(sz);
        m_desktopLyrics->move(r.center().x() - sz.width() / 2,
                              r.bottom() - sz.height() - 100);
    }

    // 播放进度 → 桌面歌词
    connect(m_player, &PlayerController::positionChanged, this, [this](qint64 pos){
        if (m_desktopLyrics && m_desktopLyrics->isVisible())
            m_desktopLyrics->setCurrentTime(pos);
    });

    // 切歌 → 重新加载歌词
    connect(m_player, &PlayerController::currentSongChanged, this, [this](const Song &s){
        if (!m_desktopLyrics) return;
        QString lrcPath = s.lyricsPath;
        if (lrcPath.isEmpty() && !s.filePath.isEmpty() && m_library)
            lrcPath = m_library->findLyrics(s.filePath);
        const Lyrics lyrics = lrcPath.isEmpty() ? Lyrics() : Lyrics::fromPath(lrcPath);
        m_desktopLyrics->setLyrics(lyrics);
    });

    // 状态变化持久化
    connect(m_desktopLyrics, &DesktopLyrics::geometryChanged, this,
        [this](const QPoint &pos, const QSize &sz){
            QJsonObject o;
            o.insert("x", pos.x());
            o.insert("y", pos.y());
            o.insert("w", sz.width());
            o.insert("h", sz.height());
            m_library->setConfigValue("desktop_lyrics_geometry", o);
        });

    connect(m_desktopLyrics, &DesktopLyrics::styleChanged, this, [this]{
        QJsonObject o;
        o.insert("textColor",    m_desktopLyrics->textColor().name());
        o.insert("outlineColor", m_desktopLyrics->outlineColor().name());
        o.insert("fontSize",     m_desktopLyrics->fontPixelSize());
        m_library->setConfigValue("desktop_lyrics_style", o);
    });
    connect(m_desktopLyrics, &DesktopLyrics::lockedChanged, this, [this](bool locked){
        QJsonObject o;
        o.insert("locked", locked);
        m_library->setConfigValue("desktop_lyrics_locked", o);
    });
    connect(m_desktopLyrics, &DesktopLyrics::requestClose, this, [this]{
        m_desktopLyrics->hide();
        QJsonObject o;
        o.insert("visible", false);
        m_library->setConfigValue("desktop_lyrics_state", o);

        // 同步菜单项的选中状态
        if (m_desktopLyricsAction)
            m_desktopLyricsAction->setChecked(false);
    });

    // 从配置恢复
    applyDesktopLyricsConfig();
}

void Rhythm::applyDesktopLyricsConfig()
{
    if (!m_desktopLyrics) return;

    // 位置
    const QJsonObject geom = m_library->configValue("desktop_lyrics_geometry");
    if (geom.contains("x") && geom.contains("y")) {
        m_desktopLyrics->move(geom.value("x").toInt(),
                              geom.value("y").toInt());
    }
    if (geom.contains("w") && geom.contains("h")) {
        m_desktopLyrics->resize(geom.value("w").toInt(),
                                geom.value("h").toInt());
    }

    // 样式
    const QJsonObject style = m_library->configValue("desktop_lyrics_style");
    if (style.contains("textColor"))
        m_desktopLyrics->setTextColor(QColor(style.value("textColor").toString()));
    if (style.contains("outlineColor"))
        m_desktopLyrics->setOutlineColor(QColor(style.value("outlineColor").toString()));
    if (style.contains("fontSize"))
        m_desktopLyrics->setFontPixelSize(style.value("fontSize").toInt());

    // 锁定
    const QJsonObject locked = m_library->configValue("desktop_lyrics_locked");
    m_desktopLyrics->setLocked(locked.value("locked").toBool(false));
}

void Rhythm::updateWordBtnIcon()
{
    const bool active = ui->wordBtn->isChecked();

    QIcon icon;
    if (active) {
        icon.addFile(":/icons/word-active.svg",       QSize(), QIcon::Normal);
        icon.addFile(":/icons/word-hover.svg", QSize(), QIcon::Active);
    } else {
        icon.addFile(":/icons/word.svg",       QSize(), QIcon::Normal);
        icon.addFile(":/icons/word-hover.svg", QSize(), QIcon::Active);
    }
    ui->wordBtn->setIcon(icon);
    ui->wordBtn->setToolTip(active ? QStringLiteral("关闭桌面歌词")
                                   : QStringLiteral("开启桌面歌词"));
}

void Rhythm::showDesktopLyrics()
{
    ui->wordBtn->setCheckable(true);
    updateWordBtnIcon();          // 初始图标（未选中）

    connect(ui->wordBtn, &QToolButton::toggled, this, [this](bool on){
        if (!m_desktopLyrics) return;

        // 1) 显示 / 隐藏
        m_desktopLyrics->setVisible(on);
        if (on) {
            const Song cur = m_player->currentSong();
            QString lrcPath = cur.lyricsPath;
            if (lrcPath.isEmpty() && !cur.filePath.isEmpty())
                lrcPath = m_library->findLyrics(cur.filePath);

            m_desktopLyrics->setLyrics(
                lrcPath.isEmpty() ? Lyrics() : Lyrics::fromPath(lrcPath));
            m_desktopLyrics->setCurrentTime(m_player->position());
            m_desktopLyrics->raise();
        }

        // 2) 持久化
        QJsonObject o;
        o.insert("visible", on);
        m_library->setConfigValue("desktop_lyrics_state", o);

        // 3) 更新图标
        updateWordBtnIcon();
    });

    // 从配置恢复（会触发 toggled，走完整逻辑）
    const QJsonObject state = m_library->configValue("desktop_lyrics_state");
    if (state.value("visible").toBool(false))
        ui->wordBtn->setChecked(true);
}

void Rhythm::paintEvent(QPaintEvent *e)
{
    QMainWindow::paintEvent(e);

    QPainter p(this);

    // 1. 绘制底图（保持比例铺满）
    if (!m_bgPixmap.isNull()) {
        QPixmap scaled = m_bgPixmap.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        int x = (width() - scaled.width()) / 2;
        int y = (height() - scaled.height()) / 2;
        p.drawPixmap(x, y, scaled);
    } else {
        p.fillRect(rect(), Qt::white); // 无背景图时默认白底
    }

    // 2. 为指定面板绘制半透明遮罩
    if (!m_bgPixmap.isNull() && m_panelOpacity < 255) {
        QColor maskColor(255, 255, 255, m_panelOpacity); // 白色遮罩

        auto drawMask = [&](QWidget *w) {
            if (!w || !w->isVisible()) return;
            // 计算 widget 在主窗口中的相对位置
            QPoint tl = w->mapTo(this, QPoint(0, 0));
            p.fillRect(QRect(tl, w->size()), maskColor);
        };

        // 左中右上下栏
        drawMask(ui->leftPanel);
        drawMask(ui->rightPanel);
        drawMask(ui->titleBar);
        drawMask(ui->playerBar);

        // 如果你想让中部面板也有蒙版，取消下面这行的注释：
        drawMask(ui->centerPanel);
    }
}

void Rhythm::applyBackgroundSettings()
{
    const QJsonObject bg = m_library->configValue("app_background");
    const QString path = bg.value("path").toString();
    const qreal opacity = bg.value("opacity").toDouble(0.86); // 默认 86%

    // 1. 加载图片
    if (path.isEmpty() || !QFile::exists(path)) {
        m_bgPixmap = QPixmap();
    } else {
        m_bgPixmap = QPixmap(path);
    }

    // 2. 记录透明度 (0~255)
    m_panelOpacity = qRound(opacity * 255.0);

    // 3. 触发主窗口重绘
    update();
}

void Rhythm::onPlaylistSelectionChanged()
{
    auto *item = m_playlistList->currentItem();
    if (!item) return;
    m_currentPlaylistId = item->data(Qt::UserRole).toLongLong();
    reloadPlaylistSongs(m_currentPlaylistId);
}

void Rhythm::onPlaylistSongDoubleClicked(QListWidgetItem *item)
{
    if (!item || m_currentPlaylistId < 0) return;
    const qint64 sid = item->data(Qt::UserRole).toLongLong();
    const auto songs = m_library->playlistSongs(m_currentPlaylistId);
    for (int i = 0; i < songs.size(); ++i)
        if (songs[i].id == sid) { m_player->setQueue(songs, i); return; }
}

void Rhythm::onCreatePlaylist()
{
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, QStringLiteral("新建歌单"),
        QStringLiteral("歌单名称："), QLineEdit::Normal,
        QStringLiteral("新歌单"), &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    const qint64 id = m_library->createPlaylist(name.trimmed());
    if (id > 0) { m_currentPlaylistId = id; reloadPlaylists(); }
}

void Rhythm::onRenamePlaylist()
{
    if (m_currentPlaylistId < 0) return;
    const auto pl = m_library->playlistById(m_currentPlaylistId);

    bool ok = false;
    const QString name = QInputDialog::getText(
        this, QStringLiteral("重命名歌单"),
        QStringLiteral("新名称："), QLineEdit::Normal, pl.name, &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    m_library->renamePlaylist(m_currentPlaylistId, name.trimmed());
}

void Rhythm::onDeletePlaylist()
{
    if (m_currentPlaylistId < 0) return;
    const auto pl = m_library->playlistById(m_currentPlaylistId);

    const auto ret = QMessageBox::question(
        this, QStringLiteral("删除歌单"),
        QStringLiteral("确定删除歌单「%1」吗？\n（不会删除歌曲本身）").arg(pl.name),
        QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::No),
        QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    m_library->removePlaylist(m_currentPlaylistId);
    m_currentPlaylistId = -1;
    reloadPlaylists();
}

void Rhythm::persistPlayQueue()
{
    const auto &queue = m_player->queue();
    QList<qint64> ids;
    ids.reserve(queue.size());
    for (const Song &s : queue) ids << s.id;
    m_library->savePlayQueue(ids, m_player->currentIndex());
}

void Rhythm::showToast(const QString &msg, int durationMs)
{
    if (!m_toast) return;

    m_toast->setText(msg);
    m_toast->adjustSize();

    // 目标位置：底部、playerBar 之上一点、水平居中
    const int margin = 20;
    const int x = (width() - m_toast->width()) / 2;
    const int y = height() - m_toast->height() - ui->playerBar->height() - margin;

    m_toast->move(qMax(0, x), qMax(0, y));
    m_toast->raise();
    m_toast->show();

    m_toastTimer->start(durationMs);
}

void Rhythm::loadLyricsFor(const Song &song)
{
    QString lrcPath = song.lyricsPath;
    if (lrcPath.isEmpty() && !song.filePath.isEmpty() && m_library)
        lrcPath = m_library->findLyrics(song.filePath);

    Lyrics lyrics = lrcPath.isEmpty() ? Lyrics() : Lyrics::fromPath(lrcPath);
    ui->lyricsView->setLyrics(lyrics);
}

void Rhythm::updateLyricsHighlight(qint64 pos)
{
    ui->lyricsView->setCurrentTime(pos);
}

void Rhythm::buildvolumeIcon()
{
    QIcon volumeIcon;
    if (m_level == Volume::No) {
        volumeIcon.addPixmap(QPixmap(":/icons/volume-mute.svg"),       QIcon::Normal);
        volumeIcon.addPixmap(QPixmap(":/icons/volume-mute-hover.svg"), QIcon::Active);
    } else if (m_level == Volume::Middle) {
        volumeIcon.addPixmap(QPixmap(":/icons/volume-small.svg"),       QIcon::Normal);
        volumeIcon.addPixmap(QPixmap(":/icons/volume-small-hover.svg"), QIcon::Active);
    } else {
        volumeIcon.addPixmap(QPixmap(":/icons/volume-notice.svg"),       QIcon::Normal);
        volumeIcon.addPixmap(QPixmap(":/icons/volume-notice-hover.svg"), QIcon::Active);
    }
    ui->volumeBtn->setIcon(volumeIcon);
}

void Rhythm::updatePlayModeIcon()
{
    using PM = PlayerController::PlayMode;
    const QString base = [this]{
        switch (m_player->playMode()) {
        case PM::Sequential: return QStringLiteral("switch");
        case PM::LoopAll:    return QStringLiteral("cycle-all");
        case PM::LoopOne:    return QStringLiteral("cycle-one");
        case PM::Shuffle:    return QStringLiteral("shuffle");
        }
        return QStringLiteral("sequence");
    }();

    QIcon icon;
    icon.addPixmap(QPixmap(QStringLiteral(":/icons/%1.svg").arg(base)),
                   QIcon::Normal);
    icon.addPixmap(QPixmap(QStringLiteral(":/icons/%1-hover.svg").arg(base)),
                   QIcon::Active);
    ui->playmode->setIcon(icon);

    const QString tooltip = [this]{
        switch (m_player->playMode()) {
        case PM::Sequential: return QStringLiteral("顺序播放");
        case PM::LoopAll:    return QStringLiteral("列表循环");
        case PM::LoopOne:    return QStringLiteral("单曲循环");
        case PM::Shuffle:    return QStringLiteral("随机播放");
        }
        return QString();
    }();
    ui->playmode->setToolTip(tooltip);
}

// -------------------- 音量弹窗：接到播放器 --------------------

void Rhythm::setupVolumePopup()
{
    m_volumePopup = new QWidget(this, Qt::Popup);
    m_volumePopup->setObjectName("volumePopup");
    m_volumePopup->setFixedSize(60, 180);

    auto *layout = new QVBoxLayout(m_volumePopup);
    layout->setContentsMargins(10, 12, 10, 12);
    layout->setSpacing(6);

    m_volumeSlider = new QSlider(Qt::Vertical, m_volumePopup);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(50);
    m_volumeSlider->setFixedSize(16, 140);
    auto *valueLabel = new QLabel(QStringLiteral("50"), m_volumePopup);
    valueLabel->setAlignment(Qt::AlignCenter);

    layout->addWidget(m_volumeSlider, 0, Qt::AlignHCenter);
    layout->addWidget(valueLabel,     0, Qt::AlignHCenter);

    connect(ui->volumeBtn, &QToolButton::clicked, this, [this]{
        if (m_volumePopup->isVisible()) {
            m_volumePopup->hide();
            return;
        }
        const QPoint btnTopLeft = ui->volumeBtn->mapToGlobal(QPoint(0, 0));
        const int x = btnTopLeft.x()
                      + (ui->volumeBtn->width() - m_volumePopup->width()) / 2;
        const int y = btnTopLeft.y() - m_volumePopup->height() - 4;
        m_volumePopup->move(x, y);
        m_volumePopup->show();
    });

    m_level = Volume::Middle;
    buildvolumeIcon();

    connect(m_volumeSlider, &QSlider::valueChanged, this,
            [this, valueLabel](int v){
                valueLabel->setText(QString::number(v));
                if (m_player) m_player->setVolume(v);
                if(v == 0)          m_level = Volume::No;
                else if (v <= 50)   m_level = Volume::Middle;
                else                m_level = Volume::Large;
                buildvolumeIcon();
                QJsonObject o;
                o.insert("value", v);
                m_library->setConfigValue("volume", o);
            });
}

void Rhythm::setupTitleBar()
{
    ui->titleBar->setObjectName("TitleBar");
    if (auto *topBar = ui->titleBar->findChild<QWidget*>("topBar")) {
        if (auto *layout = qobject_cast<QHBoxLayout*>(ui->titleBar->layout())) {
            layout->insertWidget(2, topBar, 0, Qt::AlignVCenter);
        }
    }

    ui->titleBar->setTitleContent("Rhythm", 11);
    ui->titleBar->setBackgroundColor(QColor(255, 255, 255, 0));

    connect(ui->titleBar, &TitleBar::signalMinClicked,
            this, &QWidget::showMinimized);

    connect(ui->titleBar, &TitleBar::signalCloseClicked,
            this, &QWidget::close);

    connect(ui->titleBar, &TitleBar::signalMaxRestoreClicked,
            this, [this]{
                if (isMaximized()) {
                    showNormal();
                } else {
                    showMaximized();
                }
                ui->titleBar->setMaximized(isMaximized());
            });

    // 汉堡菜单里的项
    connect(ui->titleBar, &TitleBar::signalSetting, this, [this] {
        ui->pages->setCurrentIndex(3);
    });
    connect(ui->titleBar, &TitleBar::signalAbout, this, [this]{
        QMessageBox::about(this, QStringLiteral("关于 Rhythm"),
                           QStringLiteral("Rhythm 音乐播放器\n基于 Qt 6 构建"));
    });

    connect(ui->titleBar, &TitleBar::signalExit, this, &QWidget::close);
}

bool Rhythm::eventFilter(QObject *obj, QEvent *event)
{
    // 窗口状态变化 → 同步最大化/还原图标
    if (obj == this && event->type() == QEvent::WindowStateChange) {
        ui->titleBar->setMaximized(isMaximized());
        return QMainWindow::eventFilter(obj, event);
    }

    // 全局鼠标移动 → 统一处理窗口边缘光标
    if (event->type() == QEvent::MouseMove) {
        auto *w = qobject_cast<QWidget*>(obj);
        if (w && (w == this || isAncestorOf(w))) {
            auto *me = static_cast<QMouseEvent*>(event);
            const QPoint globalPos = me->globalPosition().toPoint();
            const QPoint localPos  = mapFromGlobal(globalPos);

            if (rect().contains(localPos))
                updateResizeCursor(localPos);
            else
                unsetCursor();
        }
    }

    return QMainWindow::eventFilter(obj, event);
}

int Rhythm::hitTest(const QPoint &pos) const
{
    const int m = 6;
    int edge = EdgeNone;
    if (pos.x() <= m)             edge |= EdgeLeft;
    if (pos.x() >= width() - m)   edge |= EdgeRight;
    if (pos.y() <= m)             edge |= EdgeTop;
    if (pos.y() >= height() - m)  edge |= EdgeBottom;
    return edge;
}

void Rhythm::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        const int edge = hitTest(event->position().toPoint());
        Qt::Edges sysEdges;
        if (edge & EdgeLeft)   sysEdges |= Qt::LeftEdge;
        if (edge & EdgeRight)  sysEdges |= Qt::RightEdge;
        if (edge & EdgeTop)    sysEdges |= Qt::TopEdge;
        if (edge & EdgeBottom) sysEdges |= Qt::BottomEdge;

        if (sysEdges != Qt::Edges() && windowHandle()) {
            windowHandle()->startSystemResize(sysEdges);
            event->accept();
            return;
        }
    }
    QMainWindow::mousePressEvent(event);
}

void Rhythm::setupQueueToggle() {
    m_queuePanelWidth = ui->rightPanel->width();
    ui->rightPanel->setMinimumWidth(0);

    m_queueAnim = new QPropertyAnimation(ui->rightPanel, "maximumWidth", this);
    m_queueAnim->setDuration(220);
    m_queueAnim->setEasingCurve(QEasingCurve::InOutCubic);

    connect(ui->tracklist, &QToolButton::clicked,
            this, &Rhythm::toggleQueuePanel);
}

void Rhythm::toggleQueuePanel()
{
    if (m_queueAnim->state() == QAbstractAnimation::Running)
        return;

    const bool currentlyVisible = ui->rightPanel->maximumWidth() > 0;
    const bool inLyricsPage = (ui->pages->currentIndex() == 1);

    if (currentlyVisible) {
        // 收起
        setQueuePanelVisible(false, /*animated=*/true);
        // 在歌词页：恢复左侧封面
        if (inLyricsPage)
            ui->lyricsCoverArea->setVisible(true);
    } else {
        // 展开
        setQueuePanelVisible(true, /*animated=*/true);
        // 在歌词页：收起左侧封面，让歌词占满左侧
        if (inLyricsPage)
            ui->lyricsCoverArea->setVisible(false);
    }
}

// void Rhythm::onLyricItemClicked(QListWidgetItem *item)
// {
//     if (!item || m_lyrics.isEmpty()) return;

//     // “暂无歌词”那项 flags 为 NoItemFlags，点不了；保险起见判断
//     const qint64 t = item->data(Qt::UserRole).toLongLong();
//     m_player->setPosition(t);
// }

void Rhythm::cyclePlayMode()
{
    // 取当前模式，切到下一个
    using PM = PlayerController::PlayMode;
    PM m = m_player->playMode();
    switch (m) {
    case PM::Sequential: m = PM::LoopAll;  break;
    case PM::LoopAll:    m = PM::LoopOne;  break;
    case PM::LoopOne:    m = PM::Shuffle;  break;
    case PM::Shuffle:    m = PM::Sequential; break;
    }
    m_player->setPlayMode(m);
    updatePlayModeIcon();

    QJsonObject o;
    o.insert("mode", int(m_player->playMode()));
    m_library->setConfigValue("play_mode", o);
}

void Rhythm::setupProgressSliderOverlay()
{
    // 1) 从 playerBar 的布局里移除 progressSlider，但保留 2px 的占位
    auto *pbLayout = qobject_cast<QVBoxLayout*>(ui->playerBar->layout());
    if (!pbLayout) return;

    for (int i = 0; i < pbLayout->count(); ++i) {
        auto *item = pbLayout->itemAt(i);
        if (item && item->widget() == ui->progressSlider) {
            pbLayout->takeAt(i);
            delete item;
            pbLayout->insertSpacing(i, 2);   // 布局里仍保留 2px 空位
            break;
        }
    }

    // 2) 把 slider 挂到主窗口上，让它不受 playerBar 边界裁切
    ui->progressSlider->setParent(this);
    ui->progressSlider->raise();
}

void Rhythm::resizeEvent(QResizeEvent *e)
{
    QMainWindow::resizeEvent(e);
    repositionProgressSlider();
    repositionSpectrum();
    updateLyricsCoverSize();
}

void Rhythm::showEvent(QShowEvent *e)
{
    QMainWindow::showEvent(e);
    repositionProgressSlider();
    repositionSpectrum();
}

void Rhythm::closeEvent(QCloseEvent *e)
{
    if (m_queueSaveTimer) {
        m_queueSaveTimer->stop();
        persistPlayQueue();
    }
    if (m_desktopLyrics) {
        m_desktopLyrics->hide();      // 保证主窗口关闭后应用能退出
    }
    QMainWindow::closeEvent(e);
}

void Rhythm::repositionProgressSlider()
{
    if (!ui->playerBar || !ui->progressSlider) return;

    constexpr int handleR  = 6;
    constexpr int sliderH  = handleR * 2 + 8;   // 20：容纳手柄 + 底部余量

    const QPoint pbTL = ui->playerBar->mapTo(this, QPoint(0, 0));
    const int    w    = ui->playerBar->width();

    // slider 顶部在 playerBar 顶边上方 handleR 处
    // 这样 slider 的 y=handleR 恰好落在 playerBar 的 y=0 —— 视觉贴顶
    ui->progressSlider->setGeometry(pbTL.x(),
                                    pbTL.y() - handleR,
                                    w, sliderH);
    ui->progressSlider->raise();
}