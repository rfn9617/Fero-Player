#pragma once
#include <QElapsedTimer>
#include <QImage>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>
#include <atomic>

struct mpv_handle;

struct MediaTrack {
    int id = 0;             // -1 means "off"
    QString title, lang, codec, label;
    bool selected = false, external = false;
    QString externalFile;
};

// A track wish carried over from the previous episode, matched by name/language.
struct TrackHint {
    bool set = false, off = false;
    QString title, lang;
};

struct EngineOptions {
    quint64 window = 0;
    bool hardware = true;
    QString quality = QStringLiteral("high");   // high | balanced
    bool audioExclusive = false;
    QString logFile;
    QString cacheDirectory;
    QStringList extraOptions;   // key=value, developer experiments
};

// Thin, event-driven adapter over libmpv's client API.
// libmpv-2.dll is loaded with LoadLibrary on a worker thread so the window is
// shown before the engine is mapped and initialized.
class MpvPlayer : public QObject {
    Q_OBJECT
public:
    enum class State { Idle, Opening, Playing, Paused, Buffering, Ended, Error };

    // Creates and initializes the engine; safe to call on a worker thread.
    // Returns nullptr and sets `error` on failure.
    static MpvPlayer *create(const EngineOptions &options, QString *error);
    // Writes one frame of `media` (at 20 %) as a PNG of the given width.
    // Blocking; meant for a worker thread. Uses a separate, windowless engine.
    static bool extractFrame(const QString &media, const QString &output, int width,
                             const std::atomic_bool *cancel = nullptr);
    ~MpvPlayer() override;

    QString version() const;
    void open(const QString &path, qint64 startMs, const QString &audioId, const QString &subtitleId);
    // Applied to the next opened file once its tracks are known (before playback).
    void setTrackHints(const TrackHint &audio, const TrackHint &subtitle);
    void togglePause();
    void restart();
    void seek(qint64 milliseconds);
    void frameStep();
    void stop();
    void setVolume(int value);
    void setMute(bool value);
    bool setSpeed(double value);
    void setAudioDelay(int milliseconds);
    void setSubtitleDelay(int milliseconds);
    int audioDelay() const;
    int subtitleDelay() const;
    void setAspect(const QString &value);
    bool screenshot(const QString &path);
    // The frame on screen (with subtitles) and its display aspect; null if none.
    QImage currentFrame(double *aspect) const;
    int chapterCount() const;
    QString chapterTitle(int index) const;
    qint64 chapterStart(int index) const;   // milliseconds, -1 if unknown
    void setCursorHidden(bool hidden);
    void showText(const QString &text, int milliseconds);   // on-screen message over the video
    void addSubtitle(const QString &path, bool select);

    QVector<MediaTrack> tracks(bool audio) const;   // includes an "off" entry
    int currentTrack(bool audio) const;              // -1 when off
    bool selectTrack(bool audio, int id);

    State state() const { return currentState; }
    qint64 position() const;     // milliseconds (queried on demand)
    qint64 duration() const { return durationMs; }
    bool seekable() const { return isSeekable; }
    bool hasVideo() const { return videoPresent; }
    QString lastError() const { return errorText; }
    // Milliseconds from open() to each start-up stage of the current file.
    QJsonObject openPhases() const { return phases; }

signals:
    void stateChanged(MpvPlayer::State state);
    void durationChanged(qint64 milliseconds);
    void tracksChanged();
    void fileLoaded();
    void firstFrame();
    void videoMouse(const QString &event);   // click | double | right | move | wheel-up | wheel-down
    void outputResized(int width, int height);
    void chapterChanged(int index);   // -1: none   // the video output now renders at this size

private slots:
    void drainEvents();

private:
    explicit MpvPlayer(mpv_handle *handle);
    void handleProperty(const char *name, int format, void *data);
    void updateState();
    void command(const QStringList &arguments);
    QVariant mpvGet(const char *name) const;
    void mpvSet(const char *name, const QVariant &value);
    void markPhase(const char *name);
    void applyTrackHints();
    static constexpr quint64 HookReply = 7002;
    static constexpr quint64 OutputReply = 7003;
    static constexpr quint64 ChapterReply = 7004;
    qint64 osdWidth = 0, osdHeight = 0;

    mpv_handle *mpv = nullptr;
    State currentState = State::Idle;
    bool loading = false, paused = false, ended = false, buffering = false, failed = false;
    bool idle = true, isSeekable = false, videoPresent = false, waitingFirstFrame = false;
    qint64 durationMs = 0;
    QString errorText;
    QElapsedTimer openTimer;
    QJsonObject phases;
    TrackHint audioHint, subtitleHint;
};
