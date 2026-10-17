#pragma once
#include "catalog.h"
#include "mpvplayer.h"
#include "theme.h"

#include <QElapsedTimer>
#include <QJsonObject>
#include <QHash>
#include <QMainWindow>
#include <QPointer>
#include <QTimer>
#include <memory>
#include <optional>

class QBoxLayout;
class QFrame;
class QLabel;
class QLineEdit;
class QMenu;
class QPushButton;
class QStackedWidget;
class QToolButton;
class QVBoxLayout;
class AudioStage;
class BrandMark;
class ControlsPanel;
class MediaList;
class NavTab;
class PlaybackButton;
class SeekSlider;
class SmoothSlider;
class Thumbnails;
class TransitionCover;
class VideoSurface;

class PlayerWindow : public QMainWindow {
    Q_OBJECT
    friend class SelfTest;
public:
    explicit PlayerWindow(Catalog *catalog, QWidget *parent = nullptr);
    ~PlayerWindow() override;

    void beginEngineLoad();
    void openPaths(const QStringList &paths, bool append = false);
    qint64 startupStarted = 0;
    QStringList engineExtraOptions;
    QJsonObject startupMetrics;

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void changeEvent(QEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    enum class Chrome { Docked, Window, Fullscreen };

    // construction
    void buildUi();
    QWidget *buildHome();
    QWidget *buildLibrary();
    QWidget *buildQueue();
    QWidget *buildPlayer();
    ControlsPanel *buildControls();
    QWidget *emptyState(const QString &text);
    void setupShortcuts();
    void applyTheme(const QString &name);
    void applyNativeTitlebar(bool dark);

    // engine
    void engineReady(MpvPlayer *engine, const QString &error, double initializationMs);
    void showEngineError();
    void keepAudioAwake();

    // navigation & lists
    void navigate(const QString &page);
    void refreshLibrary();
    void refreshLibraryList();
    void refreshQueue(std::optional<bool> populate = std::nullopt);
    void syncQueueOrder();
    void mediaMenu(const QString &path, int queueIndex, const QPoint &position);
    void forgetFile(const QString &path);
    void revealFile(const QString &path);

    // opening
    void openDialog(bool append = false);
    void openFolderDialog();
    void scanFolder(const QString &directory);
    void folderScanned(const QStringList &paths, const QString &error);
    void enqueue(const QStringList &paths);
    void addFromLibrary();
    void playLibrary(const QString &path);
    void playIndex(int index, bool fromStart = false);

    // transport
    void playPause();
    void previous();
    void next();
    void removeQueueItem(int index);
    void clearQueue();
    void seekRelative(qint64 milliseconds);
    void seekFraction(int value);
    void previewSeek(int value);
    void setVolume(int value);
    void toggleMute();
    void updateMuteIcon();
    void setRate(double value);
    void nextFrame();
    void toggleOnTop();
    void snapshotDialog();

    // engine events
    void onStateChanged(MpvPlayer::State state);
    void onFileLoaded();
    void onFirstFrame();
    void writePlaybackMetrics();
    void tick();
    void updatePlayIcon();
    void updateTransport();
    void setPollInterval(int milliseconds);

    // tracks
    QMenu *audioMenu();
    QMenu *subtitlesMenu();
    void showAudioMenu();
    void showSubtitlesMenu();
    void addTrackActions(QMenu *menu, bool audio);
    void changeTrack(bool audio, int id);
    void restoreExternalSubtitle();
    void subtitleDialog();
    void loadSubtitle(const QString &path);
    void showSyncDialog();

    // menus & dialogs
    void showRateMenu();
    void showFileMenu();
    void showMoreMenu(std::optional<QPoint> position = std::nullopt);
    void showSettings();
    void showHelp();

    // chrome
    Chrome desiredChrome() const;
    bool sessionActive() const;
    void setOverlay(QWidget *widget, bool overlay);
    void applyChrome(Chrome mode);
    void syncChrome();
    QRect overlayArea() const;
    void positionOverlays();
    void updateVolumeVisibility();
    void revealControls();
    bool playerHasFocus() const;
    void showChrome(bool top = true, bool bottom = true);
    bool pointerOver(QWidget *widget) const;
    void hideChrome();
    void pointerMoved();
    void toggleFullscreen();
    void setChromeCursorHidden(bool hidden);
    void beginTransition();
    void updateTransitionCover();
    void transitionOutputResized(int width, int height);
    void trackHotkey(bool audio, int number);
    void showOsd(const QString &text);
    void chapterChanged(int index);
    void updateSkipButton();
    void skipSegment();

    // persistence & notices
    void savePosition();
    void persist();
    void flushPersistence();
    void checkpoint();
    void notify(const QString &text, int seconds = 4);
    void writeJson(const QString &name, const QJsonObject &data);

    Catalog *catalog;
    std::unique_ptr<MpvPlayer> engine;
    bool engineLoading = false;
    QString engineError;
    std::optional<std::pair<QString, bool>> pendingPlay;
    QStringList pendingSubtitles;
    Palette palette;
    QString current;
    int currentIndex = -1;
    QString currentPage = QStringLiteral("home");
    bool currentIsAudio = false;
    qint64 lastDuration = 0;
    bool endHandled = false, errorHandled = false, appliedOptions = false;
    bool isMuted = false, wasMaximized = false, isClosing = false, profileDirty = false;
    double rateValue = 1.0;
    MpvPlayer::State currentState = MpvPlayer::State::Idle;
    QByteArray normalGeometry;
    Chrome chromeMode = Chrome::Docked;
    QString noticeText;
    QElapsedTimer noticeClock;
    int noticeMs = 0;
    QPoint lastPointer;
    TrackPreferences pendingTracks;
    QElapsedTimer openClock;
    bool onTop = false;
    bool firstFrameReported = false;
    qint64 firstFrameMs = 0;

    QTimer poller, saver, chromeTimer, persistenceTimer;
    Thumbnails *thumbnails = nullptr;
    TransitionCover *cover = nullptr;
    QPushButton *skipSegmentButton = nullptr;   // "Skip intro" over the video
    qint64 skipTarget = -1;              // where the button jumps; -1: next file
    QTimer coverTimer;
    QTimer trackKeyTimer;   // A / S waiting for an optional digit
    int trackKey = 0;

    // widgets
    QWidget *content = nullptr;
    QVBoxLayout *contentLayout = nullptr, *rootLayout = nullptr;
    QFrame *topbar = nullptr;
    BrandMark *brand = nullptr;
    QHash<QString, NavTab *> navTabs;
    NavTab *playingTab = nullptr;
    QToolButton *openTop = nullptr, *addFolderButton = nullptr, *topMore = nullptr;
    QStackedWidget *pages = nullptr, *recentStack = nullptr, *libraryStack = nullptr,
                   *queueStack = nullptr, *stage = nullptr;
    QHash<QString, QWidget *> pageMap;
    MediaList *recentList = nullptr, *libraryList = nullptr, *queueList = nullptr;
    QLineEdit *search = nullptr;
    QLabel *libraryCount = nullptr, *queueCount = nullptr, *notification = nullptr;
    QPushButton *addQueueButton = nullptr, *clearQueueButton = nullptr;
    VideoSurface *surface = nullptr;
    AudioStage *audioStage = nullptr;
    QWidget *playerEmpty = nullptr;
    ControlsPanel *controls = nullptr;
    QLabel *timeLabel = nullptr, *durationLabel = nullptr, *nowTitle = nullptr, *nowDetail = nullptr;
    SeekSlider *seekSlider = nullptr;
    SmoothSlider *volumeSlider = nullptr;
    QToolButton *previousButton = nullptr, *rewindButton = nullptr, *skipButton = nullptr,
                *nextButton = nullptr, *rateButton = nullptr,
                *audioButton = nullptr, *ccButton = nullptr, *muteButton = nullptr,
                *fullscreenButton = nullptr, *moreButton = nullptr;
    PlaybackButton *playButton = nullptr;
};
