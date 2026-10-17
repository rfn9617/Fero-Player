#include "i18n.h"
#include "playerwindow.h"
#include "audiowarmup.h"
#include "icons.h"
#include "medialist.h"
#include "thumbnails.h"
#include "widgets.h"

#include <QAbstractItemModel>
#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeData>
#include <QProcess>
#include <QPushButton>
#include <QShortcut>
#include <QStackedWidget>
#include <QStyle>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <thread>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif

namespace {
constexpr int PollPlayingMs = 200, PollIdleMs = 500, PollHiddenMs = 1000;

QString stateText(MpvPlayer::State state) {
    switch (state) {
    case MpvPlayer::State::Playing: return L("Воспроизводится");
    case MpvPlayer::State::Paused: return L("Пауза");
    case MpvPlayer::State::Opening: return L("Открываем файл…");
    case MpvPlayer::State::Buffering: return L("Буферизация…");
    case MpvPlayer::State::Ended: return L("Просмотр завершён");
    case MpvPlayer::State::Error: return L("Ошибка воспроизведения");
    case MpvPlayer::State::Idle: return L("Готов к просмотру");
    }
    return {};
}
} // namespace

PlayerWindow::PlayerWindow(Catalog *catalog, QWidget *parent) : QMainWindow(parent), catalog(catalog) {
    palette = darkPalette();
    rateValue = catalog->settings.speed;
    setWindowTitle(QStringLiteral("Fero Player"));
    setWindowIcon(applicationIcon());
    setMinimumSize(900, 570);
    resize(1180, 790);
    setAcceptDrops(true);
    thumbnails = new Thumbnails(QDir(catalog->directory()).filePath(QStringLiteral("thumbs")), this);
    buildUi();
    applyTheme(catalog->settings.theme);
    setupShortcuts();
    connect(&poller, &QTimer::timeout, this, &PlayerWindow::tick);
    poller.start(PollHiddenMs);
    connect(&saver, &QTimer::timeout, this, &PlayerWindow::checkpoint);
    saver.start(5000);
    chromeTimer.setSingleShot(true);
    connect(&chromeTimer, &QTimer::timeout, this, &PlayerWindow::hideChrome);
    persistenceTimer.setSingleShot(true);
    persistenceTimer.setInterval(250);
    connect(&persistenceTimer, &QTimer::timeout, this, &PlayerWindow::flushPersistence);
    qApp->installEventFilter(this);
    trackKeyTimer.setSingleShot(true);
    connect(&trackKeyTimer, &QTimer::timeout, this, [this] {
        const bool audio = trackKey == 'A';
        trackKey = 0;
        trackHotkey(audio, -1);
    });
    if (!catalog->lastError().isEmpty())
        QTimer::singleShot(100, this, [this] { notify(L("Не удалось прочитать прежние настройки.")); });
}

PlayerWindow::~PlayerWindow() = default;

// ---- Engine ------------------------------------------------------------------------

void PlayerWindow::keepAudioAwake() {
    // Exclusive mode needs the device free, so no warm-up stream then.
    if (!catalog->settings.audioExclusive && !isClosing)
        AudioWarmup::touch();
}

void PlayerWindow::beginEngineLoad() {
    keepAudioAwake();
    if (isClosing || engineLoading || engine)
        return;
    engineLoading = true;
    nowDetail->setText(L("Подготовка к воспроизведению…"));
    EngineOptions options;
    options.window = quint64(surface->winId());
    options.hardware = catalog->settings.hardware;
    options.quality = catalog->settings.quality;
    options.audioExclusive = catalog->settings.audioExclusive;
    options.logFile = QDir(catalog->directory()).filePath(QStringLiteral("player.log"));
    options.extraOptions = engineExtraOptions;
    options.cacheDirectory = QDir(catalog->directory()).filePath(QStringLiteral("shader-cache"));
    QDir().mkpath(catalog->directory());
    QPointer<PlayerWindow> self(this);
    std::thread([self, options] {
        QElapsedTimer clock;
        clock.start();
        QString error;
        MpvPlayer *player = MpvPlayer::create(options, &error);
        const double elapsed = clock.nsecsElapsed() / 1e6;
        QMetaObject::invokeMethod(qApp, [self, player, error, elapsed] {
            if (self)
                self->engineReady(player, error, elapsed);
            else
                delete player;
        }, Qt::QueuedConnection);
    }).detach();
}

void PlayerWindow::engineReady(MpvPlayer *player, const QString &error, double initializationMs) {
    if (isClosing) {
        delete player;
        return;
    }
    engineLoading = false;
    engine.reset(player);
    engineError = error;
    nowDetail->setText(player ? L("Готов к просмотру") : L("Воспроизведение недоступно"));
    if (player) {
        connect(player, &MpvPlayer::stateChanged, this, &PlayerWindow::onStateChanged);
        connect(player, &MpvPlayer::fileLoaded, this, &PlayerWindow::onFileLoaded);
        connect(player, &MpvPlayer::firstFrame, this, &PlayerWindow::onFirstFrame);
        connect(player, &MpvPlayer::outputResized, this, &PlayerWindow::transitionOutputResized);
        connect(player, &MpvPlayer::chapterChanged, this, &PlayerWindow::chapterChanged);
        connect(player, &MpvPlayer::durationChanged, this, [this](qint64 value) {
            if (value > 0)
                lastDuration = value;
            durationLabel->setText(formatTime(lastDuration));
        });
        connect(player, &MpvPlayer::videoMouse, this, [this](const QString &event) {
            if (event == QLatin1String("move")) {
                pointerMoved();
                return;
            }
            surface->mouseFromEngine(event);
#ifdef Q_OS_WIN
            // Keep keyboard focus in Fero rather than in the engine's child window.
            ::SetFocus(reinterpret_cast<HWND>(surface->winId()));
#endif
            surface->setFocus();
        });
        player->setVolume(volumeSlider->value());
        player->setMute(isMuted);
    }
    updateTransport();
    const auto pending = pendingPlay;
    pendingPlay.reset();
    const QStringList subtitles = pendingSubtitles;
    pendingSubtitles.clear();
    if (player && pending) {
        const QString key = pathKey(pending->first);
        for (int i = 0; i < catalog->queue.size(); ++i) {
            if (pathKey(catalog->queue[i]) == key) {
                playIndex(i, pending->second);
                for (const QString &subtitle : subtitles)
                    loadSubtitle(subtitle);
                break;
            }
        }
    }
    if (startupStarted) {
        startupMetrics.insert(QStringLiteral("engine_initialization_ms"), qRound(initializationMs * 10) / 10.0);
        startupMetrics.insert(QStringLiteral("engine_ready_ms"),
                              double(QDateTime::currentMSecsSinceEpoch() - startupStarted));
        startupMetrics.insert(QStringLiteral("engine_available"), player != nullptr);
        startupMetrics.insert(QStringLiteral("engine"), player ? player->version() : error);
        writeJson(QStringLiteral("startup-metrics.json"), startupMetrics);
    }
}

void PlayerWindow::showEngineError() {
    if (engineLoading) {
        notify(L("Подготовка к воспроизведению…"));
        return;
    }
    QMessageBox::warning(this, L("Движок не найден"),
                         L("Для воспроизведения нужен libmpv-2.dll рядом с FeroPlayer.exe.\n\n") + engineError);
}

void PlayerWindow::writeJson(const QString &name, const QJsonObject &data) {
    QDir().mkpath(catalog->directory());
    QFile file(QDir(catalog->directory()).filePath(name));
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        file.write(QJsonDocument(data).toJson(QJsonDocument::Indented));
}

// ---- Construction ------------------------------------------------------------------

void PlayerWindow::buildUi() {
    auto *root = new QWidget;
    root->setObjectName(QStringLiteral("main"));
    setCentralWidget(root);
    rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    content = new QWidget;
    content->setObjectName(QStringLiteral("content"));
    contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    topbar = new QFrame;
    topbar->setObjectName(QStringLiteral("topbar"));
    topbar->setFixedHeight(64);
    auto *top = new QHBoxLayout(topbar);
    top->setContentsMargins(18, 0, 18, 0);
    top->setSpacing(0);
    brand = new BrandMark;
    top->addWidget(brand);
    top->addSpacing(14);
    const std::pair<const char *, const char *> tabs[] = {
        {"home", "Недавние"}, {"library", "Медиатека"}, {"queue", "Далее"}, {"player", "Сейчас играет"}};
    for (const auto &[page, text] : tabs) {
        auto *tab = new NavTab(L(text));
        const QString name = QString::fromLatin1(page);
        connect(tab, &QPushButton::clicked, this, [this, name] { navigate(name); });
        navTabs.insert(name, tab);
        top->addWidget(tab);
    }
    navTabs.value(QStringLiteral("home"))->setChecked(true);
    playingTab = navTabs.value(QStringLiteral("player"));
    playingTab->setToolTip(L("Вернуться к воспроизведению"));
    playingTab->hide();
    top->addStretch(1);
    // Two clear icon buttons: open media files, add a folder to the library.
    openTop = makeTool(QStringLiteral("file_open"), L("Открыть один или несколько файлов (Ctrl+O)"),
                       [this] { openDialog(); }, 40);
    openTop->setObjectName(QStringLiteral("topPrimary"));
    openTop->setIconSize(QSize(24, 24));
    top->addWidget(openTop);
    top->addSpacing(8);
    addFolderButton = makeTool(QStringLiteral("folder_add"), L("Добавить папку в медиатеку (Ctrl+Shift+O)"),
                               [this] { openFolderDialog(); }, 40);
    addFolderButton->setObjectName(QStringLiteral("topIcon"));
    addFolderButton->setIconSize(QSize(24, 24));
    top->addWidget(addFolderButton);
    top->addSpacing(4);
    topMore = makeTool(QStringLiteral("more"), L("Настройки и горячие клавиши"), [this] { showFileMenu(); }, 36);
    top->addWidget(topMore);
    contentLayout->addWidget(topbar);

    pages = new QStackedWidget;
    pageMap.insert(QStringLiteral("home"), buildHome());
    pageMap.insert(QStringLiteral("library"), buildLibrary());
    pageMap.insert(QStringLiteral("queue"), buildQueue());
    pageMap.insert(QStringLiteral("player"), buildPlayer());
    for (const char *page : {"home", "library", "queue", "player"})
        pages->addWidget(pageMap.value(QString::fromLatin1(page)));
    contentLayout->addWidget(pages, 1);
    notification = makeLabel(QString(), QStringLiteral("hint"));
    notification->setContentsMargins(28, 8, 28, 8);
    notification->hide();
    contentLayout->addWidget(notification);
    rootLayout->addWidget(content, 1);
    controls = buildControls();
    rootLayout->addWidget(controls);
    controls->hide();
}

QWidget *PlayerWindow::emptyState(const QString &text) {
    QLabel *empty = makeLabel(text, QStringLiteral("muted"));
    empty->setAlignment(Qt::AlignCenter);
    empty->setWordWrap(true);
    return empty;
}

static void connectList(PlayerWindow *window, MediaList *list, std::function<void(int)> play,
                        std::function<void(int, QPoint)> menu) {
    QObject::connect(list->rows(), &MediaDelegate::playClicked, window, [play](int row) { play(row); });
    QObject::connect(list->rows(), &MediaDelegate::menuClicked, window, [menu](int row, const QPoint &at) { menu(row, at); });
    QObject::connect(list, &QListWidget::itemDoubleClicked, window, [list, play](QListWidgetItem *item) { play(list->row(item)); });
    list->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(list, &QWidget::customContextMenuRequested, window, [list, menu](const QPoint &point) {
        if (QListWidgetItem *item = list->itemAt(point))
            menu(list->row(item), list->viewport()->mapToGlobal(point));
    });
}

QWidget *PlayerWindow::buildHome() {
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 16, 20, 16);
    recentStack = new QStackedWidget;
    recentStack->addWidget(emptyState(L("Пока нет просмотренных файлов")));
    recentList = new TileView(thumbnails);
    connectList(this, recentList, [this](int row) { playLibrary(recentList->pathAt(row)); },
                [this](int row, QPoint at) { mediaMenu(recentList->pathAt(row), -1, at); });
    recentStack->addWidget(recentList);
    layout->addWidget(recentStack, 1);
    return page;
}

QWidget *PlayerWindow::buildLibrary() {
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(12);
    auto *row = new QHBoxLayout;
    search = new QLineEdit;
    search->setPlaceholderText(L("Найти в медиатеке"));
    search->setClearButtonEnabled(true);
    search->setFocusPolicy(Qt::ClickFocus);
    connect(search, &QLineEdit::textChanged, this, [this] { refreshLibraryList(); });
    row->addWidget(search, 1);
    libraryCount = makeLabel(QString(), QStringLiteral("muted"));
    row->addWidget(libraryCount);
    layout->addLayout(row);
    libraryStack = new QStackedWidget;
    libraryStack->addWidget(emptyState(L("Нет файлов")));
    libraryList = new TileView(thumbnails);
    connectList(this, libraryList, [this](int row) { playLibrary(libraryList->pathAt(row)); },
                [this](int row, QPoint at) { mediaMenu(libraryList->pathAt(row), -1, at); });
    libraryStack->addWidget(libraryList);
    layout->addWidget(libraryStack, 1);
    return page;
}

QWidget *PlayerWindow::buildQueue() {
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(12);
    auto *row = new QHBoxLayout;
    queueCount = makeLabel(L("0 файлов"), QStringLiteral("muted"));
    row->addWidget(queueCount);
    row->addStretch();
    addQueueButton = makeButton(L("Добавить из медиатеки"), QStringLiteral("secondary"), QStringLiteral("plus"),
                                [this] { addFromLibrary(); });
    row->addWidget(addQueueButton);
    clearQueueButton = makeButton(L("Очистить"), QString(), QString(), [this] { clearQueue(); });
    row->addWidget(clearQueueButton);
    layout->addLayout(row);
    queueStack = new QStackedWidget;
    queueStack->addWidget(emptyState(L("Очередь пуста")));
    queueList = new MediaList(true);
    queueList->rows()->thumbnails = thumbnails;
    connect(thumbnails, &Thumbnails::ready, queueList->viewport(), [this] { queueList->viewport()->update(); });
    connectList(this, queueList, [this](int row) { playIndex(row); },
                [this](int row, QPoint at) { mediaMenu(queueList->pathAt(row), row, at); });
    connect(queueList->model(), &QAbstractItemModel::rowsMoved, this, [this] { syncQueueOrder(); });
    queueStack->addWidget(queueList);
    layout->addWidget(queueStack, 1);
    return page;
}

QWidget *PlayerWindow::buildPlayer() {
    stage = new QStackedWidget;
    stage->setStyleSheet(QStringLiteral("QStackedWidget { background: #000000; }"));
    surface = new VideoSurface;
    connect(surface, &VideoSurface::clicked, this, &PlayerWindow::playPause);
    connect(surface, &VideoSurface::doubleClicked, this, &PlayerWindow::toggleFullscreen);
    connect(surface, &VideoSurface::contextRequested, this, [this] { showMoreMenu(QCursor::pos()); });
    connect(surface, &VideoSurface::wheel, this, [this](int steps) {
        volumeSlider->setValue(volumeSlider->value() + steps * 5);
        showChrome(false, true);
    });
    audioStage = new AudioStage;
    playerEmpty = emptyState(L("Откройте файл"));
    stage->addWidget(playerEmpty);
    stage->addWidget(surface);
    stage->addWidget(audioStage);
    return stage;
}

ControlsPanel *PlayerWindow::buildControls() {
    auto *frame = new ControlsPanel;
    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(22, 12, 22, 16);
    layout->setSpacing(10);
    auto *seekRow = new QHBoxLayout;
    seekRow->setSpacing(14);
    timeLabel = makeLabel(QStringLiteral("0:00"), QStringLiteral("time"));
    timeLabel->setMinimumWidth(38);
    seekRow->addWidget(timeLabel);
    seekSlider = new SeekSlider;
    seekSlider->setEnabled(false);
    connect(seekSlider, &SeekSlider::committed, this, &PlayerWindow::seekFraction);
    connect(seekSlider, &QSlider::valueChanged, this, &PlayerWindow::previewSeek);
    seekRow->addWidget(seekSlider, 1);
    durationLabel = makeLabel(QStringLiteral("0:00"), QStringLiteral("time"));
    durationLabel->setMinimumWidth(38);
    durationLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    seekRow->addWidget(durationLabel);
    layout->addLayout(seekRow);

    auto *mainRow = new QHBoxLayout;
    mainRow->setSpacing(12);
    auto *metadata = new QWidget;
    auto *titles = new QVBoxLayout(metadata);
    titles->setContentsMargins(0, 0, 0, 0);
    titles->setSpacing(4);
    nowTitle = makeLabel(L("Выберите видео"), QStringLiteral("nowTitle"));
    nowTitle->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    nowDetail = makeLabel(L("Готов к просмотру"), QStringLiteral("hint"));
    nowDetail->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    titles->addWidget(nowTitle);
    titles->addWidget(nowDetail);
    metadata->setMinimumWidth(120);
    mainRow->addWidget(metadata, 1);

    auto *transport = new QHBoxLayout;
    transport->setSpacing(8);
    previousButton = makeTool(QStringLiteral("previous"), L("Предыдущий файл (Ctrl+←)"), [this] { previous(); }, 32);
    rewindButton = makeTool(QStringLiteral("rewind"), L("Назад на 10 секунд (←)"), [this] { seekRelative(-10000); }, 36);
    playButton = static_cast<PlaybackButton *>(makeTool(QStringLiteral("play"), L("Воспроизвести / пауза (Пробел)"),
                                                        [this] { playPause(); }, 56));
    playButton->setObjectName(QStringLiteral("playButton"));
    skipButton = makeTool(QStringLiteral("skip"), L("Вперёд на 10 секунд (→)"), [this] { seekRelative(10000); }, 36);
    nextButton = makeTool(QStringLiteral("next"), L("Следующий файл (Ctrl+→)"), [this] { next(); }, 32);
    for (QToolButton *button : {previousButton, rewindButton, static_cast<QToolButton *>(playButton), skipButton, nextButton}) {
        button->setEnabled(false);
        transport->addWidget(button);
    }
    mainRow->addLayout(transport);

    auto *extra = new QHBoxLayout;
    extra->setSpacing(4);
    extra->addStretch();
    rateButton = new QToolButton;
    rateButton->setObjectName(QStringLiteral("rateButton"));
    rateButton->setText(QStringLiteral("%1×").arg(rateValue));
    rateButton->setFixedSize(58, 34);
    rateButton->setToolTip(L("Скорость воспроизведения"));
    rateButton->setCursor(Qt::PointingHandCursor);
    connect(rateButton, &QToolButton::clicked, this, &PlayerWindow::showRateMenu);
    extra->addWidget(rateButton);
    audioButton = makeTool(QStringLiteral("audio_track"), L("Звуковая дорожка"), [this] { showAudioMenu(); }, 36);
    extra->addWidget(audioButton);
    ccButton = makeTool(QStringLiteral("cc"), L("Субтитры"), [this] { showSubtitlesMenu(); }, 36);
    extra->addWidget(ccButton);
    muteButton = makeTool(QStringLiteral("volume"), L("Выключить / включить звук (M)"), [this] { toggleMute(); }, 34);
    extra->addWidget(muteButton);
    volumeSlider = new SmoothSlider;
    volumeSlider->setRange(0, 100);
    volumeSlider->setValue(catalog->settings.volume);
    volumeSlider->setFixedWidth(78);
    volumeSlider->setAccessibleName(L("Громкость"));
    volumeSlider->setToolTip(L("Громкость: %1%").arg(volumeSlider->value()));
    connect(volumeSlider, &QSlider::valueChanged, this, &PlayerWindow::setVolume);
    extra->addWidget(volumeSlider);
    fullscreenButton = makeTool(QStringLiteral("fullscreen"), L("Полный экран (F / F11)"), [this] { toggleFullscreen(); }, 34);
    extra->addWidget(fullscreenButton);
    moreButton = makeTool(QStringLiteral("more"), L("Ещё"), [this] { showMoreMenu(); }, 32);
    extra->addWidget(moreButton);
    mainRow->addLayout(extra, 1);
    layout->addLayout(mainRow);
    return frame;
}

void PlayerWindow::applyTheme(const QString &name) {
    catalog->settings.theme = name;
    palette = name == QLatin1String("light") ? lightPalette() : darkPalette();
    setStyleSheet(makeStyleSheet(palette));
    playButton->accent = palette.accent;
    playButton->hover = palette.accentHover;
    playButton->foreground = palette.accentText;
    playButton->disabled = palette.border;
    playButton->update();
    for (SmoothSlider *slider : {static_cast<SmoothSlider *>(seekSlider), volumeSlider}) {
        slider->accent = palette.accent;
        slider->rail = palette.border;
        slider->update();
    }
    for (QToolButton *button : findChildren<QToolButton *>()) {
        const QString symbol = button->property("iconName").toString();
        if (!symbol.isEmpty() && button != playButton)
            button->setIcon(vectorIcon(symbol, palette.text));
    }
    openTop->setIcon(vectorIcon(QStringLiteral("file_open"), palette.accentText));
    addQueueButton->setIcon(vectorIcon(QStringLiteral("plus"), palette.text));
    brand->setTextColor(palette.text);
    for (NavTab *tab : navTabs)
        tab->setColors(palette.text, palette.muted, palette.accent);
    const RowColors rows{palette.text, palette.muted, palette.accent, palette.accentText, palette.hover, palette.border};
    for (MediaList *list : {recentList, libraryList, queueList})
        list->setColors(rows);
    updateMuteIcon();
    refreshLibrary();
    applyNativeTitlebar(palette.dark);
}

void PlayerWindow::applyNativeTitlebar(bool dark) {
#ifdef Q_OS_WIN
    BOOL value = dark ? TRUE : FALSE;
    DwmSetWindowAttribute(reinterpret_cast<HWND>(winId()), 20, &value, sizeof(value));
#else
    Q_UNUSED(dark);
#endif
}

void PlayerWindow::setupShortcuts() {
    const std::pair<const char *, std::function<void()>> shortcuts[] = {
        {"Ctrl+O", [this] { openDialog(); }},
        {"Ctrl+Shift+O", [this] { openFolderDialog(); }},
        {"Ctrl+S", [this] { snapshotDialog(); }},
    };
    for (const auto &[keys, callback] : shortcuts) {
        auto *shortcut = new QShortcut(QKeySequence(QString::fromLatin1(keys)), this);
        connect(shortcut, &QShortcut::activated, this, callback);
    }
}

// ---- Navigation and lists ------------------------------------------------------------

void PlayerWindow::navigate(const QString &page) {
    if (isFullScreen() && page != QLatin1String("player"))
        toggleFullscreen();
    currentPage = page;
    pages->setCurrentWidget(pageMap.value(page));
    for (auto it = navTabs.cbegin(); it != navTabs.cend(); ++it)
        it.value()->setChecked(it.key() == page);
    if (page != QLatin1String("player"))
        refreshLibrary();
    syncChrome();
}

void PlayerWindow::refreshLibrary() {
    if (!recentList)
        return;
    const QVector<QJsonObject> records = catalog->recent(12);
    recentList->populate(records);
    recentStack->setCurrentIndex(records.isEmpty() ? 0 : 1);
    // Off-screen lists are rebuilt only when shown.
    if (currentPage == QLatin1String("library"))
        refreshLibraryList();
    refreshQueue(currentPage == QLatin1String("queue"));
}

void PlayerWindow::refreshLibraryList() {
    const QString query = search->text().trimmed().toCaseFolded();
    QVector<QJsonObject> records;
    for (const QJsonObject &item : catalog->allItems())
        if (query.isEmpty() || item.value(QStringLiteral("title")).toString().toCaseFolded().contains(query) ||
            item.value(QStringLiteral("path")).toString().toCaseFolded().contains(query))
            records << item;
    libraryList->populate(records);
    libraryStack->setCurrentIndex(records.isEmpty() && query.isEmpty() ? 0 : 1);
    libraryCount->setText(L("%1 файлов").arg(records.size()));
}

void PlayerWindow::refreshQueue(std::optional<bool> populate) {
    const bool rebuild = populate.value_or(currentPage == QLatin1String("queue"));
    if (rebuild) {
        QVector<QJsonObject> records;
        for (const QString &path : catalog->queue) {
            const QJsonObject *record = catalog->record(path);
            records << (record ? *record : QJsonObject{{QStringLiteral("path"), path},
                                                       {QStringLiteral("title"), QFileInfo(path).completeBaseName()}});
        }
        queueList->populate(records, currentIndex);
    }
    queueStack->setCurrentIndex(catalog->queue.isEmpty() ? 0 : 1);
    queueCount->setText(L("%1 файлов").arg(catalog->queue.size()));
    clearQueueButton->setEnabled(!catalog->queue.isEmpty());
    updateTransport();
}

void PlayerWindow::syncQueueOrder() {
    QStringList items;
    for (int i = 0; i < queueList->count(); ++i)
        items << queueList->pathAt(i);
    catalog->queue = items;
    currentIndex = -1;
    if (!current.isEmpty()) {
        const QString key = pathKey(current);
        for (int i = 0; i < items.size(); ++i)
            if (pathKey(items[i]) == key) {
                currentIndex = i;
                break;
            }
    }
    persist();
    QTimer::singleShot(0, this, [this] { refreshQueue(); });   // let Qt finish moving rows
}

void PlayerWindow::forgetFile(const QString &path) {
    catalog->forget(path);
    persist();
    refreshLibrary();
}

void PlayerWindow::revealFile(const QString &path) {
#ifdef Q_OS_WIN
    if (QFileInfo::exists(path)) {
        QProcess::startDetached(QStringLiteral("explorer.exe"),
                                {QStringLiteral("/select,"), QDir::toNativeSeparators(path)});
        return;
    }
#endif
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
}

// ---- Opening ---------------------------------------------------------------------------

void PlayerWindow::openDialog(bool append) {
    QStringList patterns;
    for (const QString &extension : mediaExtensions)
        patterns << QStringLiteral("*.") + extension;
    patterns.sort();
    const QStringList paths = QFileDialog::getOpenFileNames(
        this, L("Открыть медиафайлы"), QString(),
        L("Видео и аудио (%1);;Все файлы (*)").arg(patterns.join(QLatin1Char(' '))));
    if (paths.isEmpty())
        return;
    if (append)
        enqueue(paths);
    else
        openPaths(paths);
}

void PlayerWindow::openPaths(const QStringList &paths, bool append) {
    QStringList subtitles, media;
    for (const QString &path : paths)
        (subtitleExtensions.contains(suffixOf(path)) ? subtitles : media) << path;
    const QStringList valid = catalog->add(media);
    if (!valid.isEmpty()) {
        if (append) {
            enqueue(valid);
        } else if (valid.size() == 1) {
            playLibrary(valid.first());   // one file: the rest of its folder follows
        } else {
            catalog->queue = valid;
            playIndex(0);
            refreshLibrary();
            persist();
        }
    }
    for (const QString &subtitle : subtitles)
        loadSubtitle(subtitle);
    if (!media.isEmpty() && valid.isEmpty())
        notify(L("Файл не найден или недоступен."));
}

void PlayerWindow::enqueue(const QStringList &paths) {
    const QStringList valid = catalog->add(paths);
    catalog->queue << valid;
    refreshLibrary();
    persist();
    notify(L("Добавлено в очередь: %1").arg(valid.size()));
}

void PlayerWindow::openFolderDialog() {
    const QString directory = QFileDialog::getExistingDirectory(this, L("Добавить папку в медиатеку"));
    if (!directory.isEmpty())
        scanFolder(directory);
}

void PlayerWindow::scanFolder(const QString &directory) {
    notify(L("Ищем видео и аудио в выбранной папке…"), 30);
    QPointer<PlayerWindow> self(this);
    std::thread([self, directory] {
        const QStringList paths = ::scanFolder(directory);
        QMetaObject::invokeMethod(qApp, [self, paths] {
            if (self)
                self->folderScanned(paths, QString());
        }, Qt::QueuedConnection);
    }).detach();
}

void PlayerWindow::folderScanned(const QStringList &paths, const QString &error) {
    if (isClosing)
        return;
    if (!error.isEmpty()) {
        notify(L("Не удалось прочитать папку: ") + error);
        return;
    }
    const int previous = catalog->librarySize();
    catalog->add(paths);
    const int count = catalog->librarySize() - previous;
    persist();
    navigate(QStringLiteral("library"));
    notify(paths.isEmpty() ? L("В этой папке не найдено видео или аудио.")
                           : L("Добавлено файлов: %1").arg(count));
}

void PlayerWindow::playLibrary(const QString &path) {
    if (path.isEmpty())
        return;
    // Series and films: the following files of the same folder play next.
    const QFileInfo info(path);
    const QDir folder = info.dir();
    QStringList playlist;
    int index = -1;
    const QString key = pathKey(path);
    for (const QString &name : naturalSorted(folder.entryList(QDir::Files | QDir::Readable))) {
        if (!mediaExtensions.contains(suffixOf(name)))
            continue;
        const QString file = QDir::toNativeSeparators(folder.absoluteFilePath(name));
        if (pathKey(file) == key)
            index = int(playlist.size());
        playlist << file;
    }
    if (index < 0) {
        playlist = QStringList{path};
        index = 0;
    }
    catalog->queue = playlist;
    playIndex(index);
    refreshLibrary();
    persist();
}

void PlayerWindow::playIndex(int index, bool fromStart) {
    if (index < 0 || index >= catalog->queue.size())
        return;
    const QString path = catalog->queue[index];
    const QFileInfo info(path);
    if (!info.isFile()) {
        QMessageBox::warning(this, L("Файл недоступен"),
                             L("Файл был перемещён или удалён:\n\n") + QDir::toNativeSeparators(path));
        return;
    }
    if (!engine) {
        if (engineLoading) {
            if (pendingPlay && pathKey(pendingPlay->first) != pathKey(path))
                pendingSubtitles.clear();
            pendingPlay = std::make_pair(path, fromStart);
            nowTitle->setText(info.completeBaseName());
            nowDetail->setText(L("Подготовка к воспроизведению…"));
            // Go to the (black) video page at once instead of showing the library
            // while the engine finishes loading.
            currentIsAudio = audioExtensions.contains(suffixOf(path));
            stage->setCurrentWidget(currentIsAudio ? static_cast<QWidget *>(audioStage) : surface);
            audioStage->title->setText(info.completeBaseName());
            setWindowTitle(info.completeBaseName() + QStringLiteral(" — Fero Player"));
            playingTab->show();
            navigate(QStringLiteral("player"));
            revealControls();
            return;
        }
        showEngineError();
        return;
    }
    savePosition();
    const QJsonObject *record = catalog->record(path);
    const qint64 resume = fromStart || !catalog->settings.resume ? 0 : Catalog::resumePosition(record);
    current = path;
    currentIndex = index;
    pendingTracks = catalog->trackPreferences(path);
    lastDuration = 0;
    endHandled = errorHandled = appliedOptions = firstFrameReported = false;
    seekSlider->setValue(0);
    seekSlider->setEnabled(false);
    timeLabel->setText(QStringLiteral("0:00"));
    durationLabel->setText(QStringLiteral("0:00"));
    nowTitle->setText(info.completeBaseName());
    nowTitle->setToolTip(info.fileName());
    nowDetail->setText(L("Открываем файл…"));
    setWindowTitle(info.completeBaseName() + QStringLiteral(" — Fero Player"));
    currentIsAudio = audioExtensions.contains(suffixOf(path));
    stage->setCurrentWidget(currentIsAudio ? static_cast<QWidget *>(audioStage) : surface);
    audioStage->title->setText(info.completeBaseName());
    playingTab->show();
    navigate(QStringLiteral("player"));
    revealControls();

    // Saved per-file choices are passed to the engine before the input starts.
    QString audioId, subtitleId;
    if (pendingTracks.audio) {
        const TrackChoice &choice = *pendingTracks.audio;
        if (choice.id < 0)
            audioId = QStringLiteral("no");
        else if (choice.id > 0)
            audioId = QString::number(choice.id);
    }
    if (pendingTracks.subtitle) {
        const TrackChoice &choice = *pendingTracks.subtitle;
        const bool external = !choice.external.isEmpty() && QFileInfo::exists(choice.external);
        if (choice.id < 0)
            subtitleId = QStringLiteral("no");
        else if (!external && choice.id > 0)
            subtitleId = QString::number(choice.id);
    }
    // No choice saved for this file: take the dub/subtitles picked earlier in
    // this folder (the previous episode), matched by name and language.
    TrackHint audioHint, subtitleHint;
    auto hint = [this, &path](bool audio, TrackHint &target) {
        if (auto choice = catalog->seriesTrack(path, audio)) {
            target.set = true;
            target.off = choice->id < 0;
            target.title = choice->name;
            target.lang = choice->lang;
        }
    };
    if (!pendingTracks.audio)
        hint(true, audioHint);
    if (!pendingTracks.subtitle)
        hint(false, subtitleHint);
    engine->setTrackHints(audioHint, subtitleHint);
    keepAudioAwake();
    openClock.start();
    engine->open(path, resume, audioId, subtitleId);
    if (resume)
        notify(L("Продолжаем с ") + formatTime(resume));
    catalog->markPlayed(path);
    persist();
    updateTransport();
    refreshQueue();
    showChrome();
    setPollInterval(PollPlayingMs);
}

void PlayerWindow::onFileLoaded() {
    restoreExternalSubtitle();
    if (rateValue != 1.0 && engine && !engine->setSpeed(rateValue)) {
        rateValue = 1.0;
        rateButton->setText(QStringLiteral("1×"));
    }
    if (engine) {
        engine->setVolume(volumeSlider->value());
        engine->setMute(isMuted);
    }
    appliedOptions = true;
    tick();
}

void PlayerWindow::onFirstFrame() {
    if (firstFrameReported || !openClock.isValid())
        return;
    firstFrameReported = true;
    firstFrameMs = openClock.elapsed();
    writePlaybackMetrics();
}

void PlayerWindow::writePlaybackMetrics() {
    QJsonObject metrics{{QStringLiteral("file"), QFileInfo(current).fileName()},
                        {QStringLiteral("first_frame_ms"), double(firstFrameMs)},
                        {QStringLiteral("hardware_requested"), catalog->settings.hardware},
                        {QStringLiteral("quality"), catalog->settings.quality},
                        {QStringLiteral("audio_exclusive"), catalog->settings.audioExclusive}};
    if (engine)
        metrics.insert(QStringLiteral("phases"), engine->openPhases());
    writeJson(QStringLiteral("playback-metrics.json"), metrics);
}

void PlayerWindow::restoreExternalSubtitle() {
    if (!engine || !pendingTracks.subtitle)
        return;
    const TrackChoice choice = *pendingTracks.subtitle;
    if (!choice.external.isEmpty() && QFileInfo::exists(choice.external))
        engine->addSubtitle(choice.external, choice.id >= 0);
    pendingTracks.subtitle.reset();
}

// ---- Transport -------------------------------------------------------------------------

void PlayerWindow::playPause() {
    if (!engine) {
        showEngineError();
        return;
    }
    if (current.isEmpty()) {
        if (!catalog->queue.isEmpty())
            playIndex(0);
        else
            openDialog();
        return;
    }
    const auto state = engine->state();
    if (state == MpvPlayer::State::Error || state == MpvPlayer::State::Idle) {
        if (currentIndex >= 0)
            playIndex(currentIndex, true);
        else
            playLibrary(current);
    } else {
        engine->togglePause();
    }
    showChrome();
}

void PlayerWindow::previous() {
    if (engine && !current.isEmpty() && engine->position() > 3000)
        engine->seek(0);
    else if (currentIndex > 0)
        playIndex(currentIndex - 1);
}

void PlayerWindow::next() {
    if (auto index = nextQueueIndex(currentIndex, catalog->queue.size()))
        playIndex(*index);
}

void PlayerWindow::removeQueueItem(int index) {
    if (index < 0 || index >= catalog->queue.size())
        return;
    catalog->queue.removeAt(index);
    if (index == currentIndex)
        currentIndex = -1;
    else if (index < currentIndex)
        --currentIndex;
    refreshQueue();
    persist();
}

void PlayerWindow::clearQueue() {
    catalog->queue.clear();
    currentIndex = -1;
    refreshQueue();
    persist();
}

void PlayerWindow::seekRelative(qint64 milliseconds) {
    if (engine && !current.isEmpty() && engine->seekable()) {
        engine->seek(engine->position() + milliseconds);
        showChrome(false, true);
    }
}

void PlayerWindow::seekFraction(int value) {
    if (engine && !current.isEmpty())
        engine->seek(engine->duration() * value / 10000);
}

void PlayerWindow::previewSeek(int value) {
    if (seekSlider->isSliderDown() && lastDuration)
        timeLabel->setText(formatTime(lastDuration * value / 10000));
}

void PlayerWindow::setVolume(int value) {
    catalog->settings.volume = value;
    volumeSlider->setToolTip(L("Громкость: %1%").arg(value));
    if (engine)
        engine->setVolume(value);
    if (value && isMuted) {
        isMuted = false;
        if (engine)
            engine->setMute(false);
    }
    updateMuteIcon();
    persist();
}

void PlayerWindow::toggleMute() {
    isMuted = !isMuted;
    if (engine)
        engine->setMute(isMuted);
    updateMuteIcon();
}

void PlayerWindow::updateMuteIcon() {
    const QString symbol = isMuted || volumeSlider->value() == 0 ? QStringLiteral("mute") : QStringLiteral("volume");
    muteButton->setProperty("iconName", symbol);
    muteButton->setIcon(vectorIcon(symbol, palette.text));
}

void PlayerWindow::setRate(double value) {
    if (engine && !current.isEmpty() && !engine->setSpeed(value)) {
        notify(L("Для этого файла изменить скорость не удалось."));
        return;
    }
    rateValue = value;
    catalog->settings.speed = value;
    rateButton->setText(QStringLiteral("%1×").arg(value));
    persist();
}

void PlayerWindow::nextFrame() {
    if (engine && !current.isEmpty()) {
        engine->frameStep();
        showChrome(false, true);
    }
}

void PlayerWindow::toggleOnTop() {
    onTop = !onTop;
#ifdef Q_OS_WIN
    // Win32 avoids recreating the native window that hosts the video output.
    SetWindowPos(reinterpret_cast<HWND>(winId()), onTop ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
#endif
}

void PlayerWindow::snapshotDialog() {
    if (!engine || current.isEmpty() || !engine->hasVideo())
        return;
    const QString pictures = QDir(QDir::homePath()).filePath(QStringLiteral("Pictures"));
    const QString suggestion = QDir(pictures).filePath(
        QStringLiteral("%1-%2.png").arg(QFileInfo(current).completeBaseName()).arg(QDateTime::currentSecsSinceEpoch()));
    QString path = QFileDialog::getSaveFileName(this, L("Сохранить кадр"), suggestion, QStringLiteral("PNG (*.png)"));
    if (path.isEmpty())
        return;
    if (!path.endsWith(QLatin1String(".png"), Qt::CaseInsensitive))
        path += QStringLiteral(".png");
    notify(engine->screenshot(path) ? L("Кадр сохранён.") : L("Не удалось сохранить кадр."));
}

// ---- Engine state ----------------------------------------------------------------------

void PlayerWindow::onStateChanged(MpvPlayer::State state) {
    if (current.isEmpty() || isClosing)
        return;
    currentState = state;
    const bool notice = chromeMode != Chrome::Docked && noticeClock.isValid() && noticeClock.elapsed() < noticeMs;
    if (!notice)
        nowDetail->setText(stateText(state));
    updatePlayIcon();
    updateTransport();
    if (state == MpvPlayer::State::Playing)
        setPollInterval(PollPlayingMs);
    if (state == MpvPlayer::State::Ended && !endHandled) {
        endHandled = true;
        catalog->updatePosition(current, 0, lastDuration, true);
        persist();
        const auto index = nextQueueIndex(currentIndex, catalog->queue.size());
        if (index && catalog->settings.autoplay) {
            const int target = *index;
            QTimer::singleShot(0, this, [this, target] { playIndex(target, true); });
        } else {
            showChrome();
            refreshLibrary();
        }
    }
    if (state == MpvPlayer::State::Error && !errorHandled) {
        errorHandled = true;
        showChrome();
        QMessageBox::warning(this, L("Ошибка воспроизведения"),
                             L("Не удалось воспроизвести этот файл.\n\n%1\n\n"
                                            "Если проблема с видео, попробуйте отключить аппаратное декодирование в настройках.")
                                 .arg(engine ? engine->lastError() : QString()));
    }
}

void PlayerWindow::tick() {
    if (!engine || current.isEmpty() || isClosing) {
        setPollInterval(PollHiddenMs);
        return;
    }
    const auto state = engine->state();
    if (state == MpvPlayer::State::Playing)
        keepAudioAwake();   // the next episode starts without waking the device
    const qint64 position = engine->position();
    if (engine->duration() > 0)
        lastDuration = engine->duration();
    if (!seekSlider->isSliderDown()) {
        timeLabel->setText(formatTime(position));
        if (lastDuration)
            seekSlider->setValue(int(qMin<qint64>(10000, position * 10000 / lastDuration)));
    }
    seekSlider->setEnabled(engine->seekable() && lastDuration > 0);
    const bool notice = noticeClock.isValid() && noticeClock.elapsed() < noticeMs;
    if (notification->isVisible() && !notice)
        notification->hide();
    if (chromeMode != Chrome::Docked && !notice && nowDetail->text() == noticeText && !noticeText.isEmpty())
        nowDetail->setText(stateText(state));
    if (state == MpvPlayer::State::Playing && chromeMode != Chrome::Docked && !chromeTimer.isActive() &&
        (controls->isVisible() || topbar->isVisible()))
        chromeTimer.start(2600);
    if (isMinimized() || !isVisible())
        setPollInterval(PollHiddenMs);
    else
        setPollInterval(state == MpvPlayer::State::Playing || state == MpvPlayer::State::Opening ||
                                state == MpvPlayer::State::Buffering
                            ? PollPlayingMs
                            : PollIdleMs);
}

void PlayerWindow::setPollInterval(int milliseconds) {
    if (poller.interval() != milliseconds)
        poller.setInterval(milliseconds);
}

void PlayerWindow::updatePlayIcon() {
    const QString symbol = currentState == MpvPlayer::State::Playing ? QStringLiteral("pause") : QStringLiteral("play");
    if (playButton->symbol != symbol) {
        playButton->symbol = symbol;
        playButton->update();
    }
}

void PlayerWindow::updateTransport() {
    const bool active = engine && !current.isEmpty();
    const bool running = currentState == MpvPlayer::State::Playing || currentState == MpvPlayer::State::Paused ||
                         currentState == MpvPlayer::State::Ended;
    playButton->setEnabled(engine && (!current.isEmpty() || !catalog->queue.isEmpty()));
    previousButton->setEnabled(active);
    rewindButton->setEnabled(active && running);
    skipButton->setEnabled(active && running);
    nextButton->setEnabled(engine && nextQueueIndex(currentIndex, catalog->queue.size()));
    ccButton->setEnabled(active);
    audioButton->setEnabled(active);
}

// ---- Persistence -------------------------------------------------------------------------

void PlayerWindow::savePosition() {
    if (!engine || current.isEmpty())
        return;
    const auto state = engine->state();
    const qint64 duration = engine->duration() ? engine->duration() : lastDuration;
    if (endHandled) {
        // Finished: the paused last frame must not be stored as a resume point.
        catalog->updatePosition(current, 0, duration, true);
        return;
    }
    if ((state == MpvPlayer::State::Playing || state == MpvPlayer::State::Paused) && duration)
        catalog->updatePosition(current, engine->position(), duration);
    else if (state == MpvPlayer::State::Ended)
        catalog->updatePosition(current, 0, duration, true);
}

void PlayerWindow::persist() {
    profileDirty = true;
    if (isClosing)
        flushPersistence();
    else if (!persistenceTimer.isActive())
        persistenceTimer.start();   // coalesce queue, history and settings into one write
}

void PlayerWindow::flushPersistence() {
    persistenceTimer.stop();
    if (!profileDirty)
        return;
    profileDirty = false;
    if (!catalog->save() && !isClosing)
        notify(L("Не удалось сохранить настройки. Проверьте доступ к папке профиля."));
}

void PlayerWindow::checkpoint() {
    if (current.isEmpty())
        return;
    savePosition();
    persist();
}

void PlayerWindow::notify(const QString &text, int seconds) {
    noticeText = text;
    noticeMs = seconds * 1000;
    noticeClock.start();
    notification->setText(text);
    if (chromeMode == Chrome::Docked) {
        notification->show();
    } else {
        // Over video the message goes to the controls: the video is never resized.
        nowDetail->setText(text);
        showChrome(false, true);
    }
    QTimer::singleShot(noticeMs, this, [this] {
        if (noticeClock.isValid() && noticeClock.elapsed() >= noticeMs) {
            notification->hide();
            if (nowDetail->text() == noticeText && engine && !current.isEmpty())
                nowDetail->setText(stateText(engine->state()));
        }
    });
}

// ---- Window events -----------------------------------------------------------------------

void PlayerWindow::dragEnterEvent(QDragEnterEvent *event) {
    for (const QUrl &url : event->mimeData()->urls())
        if (url.isLocalFile()) {
            event->acceptProposedAction();
            return;
        }
}

void PlayerWindow::dropEvent(QDropEvent *event) {
    QStringList files;
    for (const QUrl &url : event->mimeData()->urls()) {
        if (!url.isLocalFile())
            continue;
        const QString path = url.toLocalFile();
        if (QFileInfo(path).isDir())
            scanFolder(path);
        else if (QFileInfo(path).isFile())
            files << path;
    }
    if (!files.isEmpty())
        openPaths(files, currentPage == QLatin1String("queue"));
    event->acceptProposedAction();
}

void PlayerWindow::closeEvent(QCloseEvent *event) {
    isClosing = true;
    poller.stop();
    saver.stop();
    chromeTimer.stop();
    controls->hide();
    if (topbar->isWindow())
        topbar->hide();
    savePosition();
    persist();
    qApp->removeEventFilter(this);
    AudioWarmup::stop();
    engine.reset();
    event->accept();
}
