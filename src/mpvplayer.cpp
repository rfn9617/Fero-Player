#include "i18n.h"
#include "mpvplayer.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QThread>
#include <QRegularExpression>
#include <QSet>
#include <mpv/client.h>
#include <deque>
#include <mutex>
#include <memory>
#include <vector>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

struct Api {
    decltype(&mpv_client_api_version) client_api_version = nullptr;
    decltype(&mpv_create) create = nullptr;
    decltype(&mpv_initialize) initialize = nullptr;
    decltype(&mpv_terminate_destroy) terminate_destroy = nullptr;
    decltype(&mpv_set_option) set_option = nullptr;
    decltype(&mpv_set_option_string) set_option_string = nullptr;
    decltype(&mpv_set_property) set_property = nullptr;
    decltype(&mpv_set_property_string) set_property_string = nullptr;
    decltype(&mpv_get_property) get_property = nullptr;
    decltype(&mpv_get_property_string) get_property_string = nullptr;
    decltype(&mpv_command_node) command_node = nullptr;
    decltype(&mpv_observe_property) observe_property = nullptr;
    decltype(&mpv_unobserve_property) unobserve_property = nullptr;
    decltype(&mpv_hook_add) hook_add = nullptr;
    decltype(&mpv_hook_continue) hook_continue = nullptr;
    decltype(&mpv_request_log_messages) request_log_messages = nullptr;
    decltype(&mpv_set_wakeup_callback) set_wakeup_callback = nullptr;
    decltype(&mpv_wait_event) wait_event = nullptr;
    decltype(&mpv_error_string) error_string = nullptr;
    decltype(&mpv_free) free = nullptr;
    decltype(&mpv_free_node_contents) free_node_contents = nullptr;
};

Api api;
QString logPath;

template <typename T>
bool resolve(void *library, const char *name, T &target) {
#ifdef Q_OS_WIN
    target = reinterpret_cast<T>(reinterpret_cast<void *>(GetProcAddress(static_cast<HMODULE>(library), name)));
#else
    target = reinterpret_cast<T>(dlsym(library, name));
#endif
    return target != nullptr;
}

std::mutex apiMutex;

bool loadApi(QString *error) {
    std::lock_guard<std::mutex> lock(apiMutex);
    if (api.create)
        return true;
    const QString directory = QCoreApplication::applicationDirPath();
#ifdef Q_OS_WIN
    const QString path = QDir::toNativeSeparators(directory + QStringLiteral("/libmpv-2.dll"));
    void *library = LoadLibraryExW(reinterpret_cast<LPCWSTR>(path.utf16()), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
#else
    const QString path = directory + QStringLiteral("/libmpv.so.2");
    void *library = dlopen(QFile::exists(path) ? path.toUtf8().constData() : "libmpv.so.2", RTLD_NOW);
#endif
    if (!library) {
        *error = L("Не найден движок libmpv-2.dll рядом с FeroPlayer.exe.");
        return false;
    }
    Api loaded;
    const bool ok = resolve(library, "mpv_client_api_version", loaded.client_api_version) &&
                    resolve(library, "mpv_create", loaded.create) &&
                    resolve(library, "mpv_initialize", loaded.initialize) &&
                    resolve(library, "mpv_terminate_destroy", loaded.terminate_destroy) &&
                    resolve(library, "mpv_set_option", loaded.set_option) &&
                    resolve(library, "mpv_set_option_string", loaded.set_option_string) &&
                    resolve(library, "mpv_set_property", loaded.set_property) &&
                    resolve(library, "mpv_set_property_string", loaded.set_property_string) &&
                    resolve(library, "mpv_get_property", loaded.get_property) &&
                    resolve(library, "mpv_get_property_string", loaded.get_property_string) &&
                    resolve(library, "mpv_command_node", loaded.command_node) &&
                    resolve(library, "mpv_observe_property", loaded.observe_property) &&
                    resolve(library, "mpv_unobserve_property", loaded.unobserve_property) &&
                    resolve(library, "mpv_hook_add", loaded.hook_add) &&
                    resolve(library, "mpv_hook_continue", loaded.hook_continue) &&
                    resolve(library, "mpv_request_log_messages", loaded.request_log_messages) &&
                    resolve(library, "mpv_set_wakeup_callback", loaded.set_wakeup_callback) &&
                    resolve(library, "mpv_wait_event", loaded.wait_event) &&
                    resolve(library, "mpv_error_string", loaded.error_string) &&
                    resolve(library, "mpv_free", loaded.free) &&
                    resolve(library, "mpv_free_node_contents", loaded.free_node_contents);
    if (!ok) {
        *error = L("Версия libmpv-2.dll не подходит.");
        return false;
    }
    if ((loaded.client_api_version() >> 16) != MPV_CLIENT_API_VERSION >> 16) {
        *error = L("Несовместимая версия API libmpv.");
        return false;
    }
    api = loaded;
    return true;
}

// ---- mpv_node helpers --------------------------------------------------------

class NodeBuilder {
public:
    mpv_node string(const QString &value) {
        strings.emplace_back(value.toUtf8());
        mpv_node node{};
        node.format = MPV_FORMAT_STRING;
        node.u.string = strings.back().data();
        return node;
    }
    mpv_node map(const std::vector<std::pair<QString, mpv_node>> &entries) {
        auto list = std::make_unique<mpv_node_list>();
        nodes.emplace_back();
        auto &values = nodes.back();
        keys.emplace_back();
        auto &names = keys.back();
        for (const auto &[key, value] : entries) {
            strings.emplace_back(key.toUtf8());
            names.push_back(strings.back().data());
            values.push_back(value);
        }
        list->num = int(values.size());
        list->values = values.data();
        list->keys = names.data();
        mpv_node node{};
        node.format = MPV_FORMAT_NODE_MAP;
        node.u.list = list.get();
        lists.push_back(std::move(list));
        return node;
    }

private:
    std::deque<QByteArray> strings;
    std::deque<std::vector<mpv_node>> nodes;
    std::deque<std::vector<char *>> keys;
    std::vector<std::unique_ptr<mpv_node_list>> lists;
};

QVariant nodeToVariant(const mpv_node &node) {
    switch (node.format) {
    case MPV_FORMAT_STRING:
        return QString::fromUtf8(node.u.string);
    case MPV_FORMAT_FLAG:
        return bool(node.u.flag);
    case MPV_FORMAT_INT64:
        return qlonglong(node.u.int64);
    case MPV_FORMAT_DOUBLE:
        return node.u.double_;
    case MPV_FORMAT_NODE_ARRAY: {
        QVariantList list;
        for (int i = 0; i < node.u.list->num; ++i)
            list << nodeToVariant(node.u.list->values[i]);
        return list;
    }
    case MPV_FORMAT_NODE_MAP: {
        QVariantMap map;
        for (int i = 0; i < node.u.list->num; ++i)
            map.insert(QString::fromUtf8(node.u.list->keys[i]), nodeToVariant(node.u.list->values[i]));
        return map;
    }
    default:
        return {};
    }
}

void wakeup(void *context) {
    auto *self = static_cast<MpvPlayer *>(context);
    QMetaObject::invokeMethod(self, "drainEvents", Qt::QueuedConnection);
}

void appendLog(const QString &line) {
    if (logPath.isEmpty())
        return;
    QFile file(logPath);
    if (file.size() > 512 * 1024)
        file.remove();   // keep the log small
    if (file.open(QIODevice::Append | QIODevice::Text))
        file.write((QDateTime::currentDateTime().toString(Qt::ISODate) + QLatin1Char(' ') + line + QLatin1Char('\n')).toUtf8());
}

} // namespace

bool MpvPlayer::extractFrame(const QString &media, const QString &output, int width, const std::atomic_bool *cancel) {
    QString error;
    if (!loadApi(&error))
        return false;
    mpv_handle *handle = api.create();
    if (!handle)
        return false;
    auto set = [&](const char *name, const QString &value) {
        api.set_option_string(handle, name, value.toUtf8().constData());
    };
    // Encoding mode: decode one frame at 20 % of the file and write it as PNG.
    set("config", QStringLiteral("no"));
    set("terminal", QStringLiteral("no"));
    set("load-scripts", QStringLiteral("no"));
    set("ytdl", QStringLiteral("no"));
    set("idle", QStringLiteral("no"));
    set("o", QDir::toNativeSeparators(output));
    set("ovc", QStringLiteral("png"));
    set("frames", QStringLiteral("1"));
    set("start", QStringLiteral("20%"));
    set("hr-seek", QStringLiteral("no"));
    set("aid", QStringLiteral("no"));
    set("sid", QStringLiteral("no"));
    set("hwdec", QStringLiteral("no"));
    set("vf", QStringLiteral("scale=%1:-2").arg(width));
    if (api.initialize(handle) < 0) {
        api.terminate_destroy(handle);
        return false;
    }
    const QByteArray url = QDir::toNativeSeparators(media).toUtf8();
    const char *arguments[] = {"loadfile", url.constData(), nullptr};
    mpv_node args{};
    std::vector<mpv_node> values;
    for (const char *argument : {arguments[0], arguments[1]}) {
        mpv_node node{};
        node.format = MPV_FORMAT_STRING;
        node.u.string = const_cast<char *>(argument);
        values.push_back(node);
    }
    mpv_node_list list{int(values.size()), values.data(), nullptr};
    args.format = MPV_FORMAT_NODE_ARRAY;
    args.u.list = &list;
    mpv_node result{};
    if (api.command_node(handle, &args, &result) >= 0)
        api.free_node_contents(&result);
    QElapsedTimer clock;
    clock.start();
    while (clock.elapsed() < 15000 && !(cancel && *cancel)) {
        mpv_event *event = api.wait_event(handle, 0.2);
        if (event->event_id == MPV_EVENT_END_FILE || event->event_id == MPV_EVENT_SHUTDOWN)
            break;
    }
    api.terminate_destroy(handle);   // flushes the encoder
    return QFileInfo(output).size() > 0;
}

MpvPlayer *MpvPlayer::create(const EngineOptions &options, QString *error) {
    if (!loadApi(error))
        return nullptr;
    mpv_handle *handle = api.create();
    if (!handle) {
        *error = L("libmpv не удалось создать.");
        return nullptr;
    }
    logPath = options.logFile;
    auto set = [&](const char *name, const QString &value) {
        return api.set_option_string(handle, name, value.toUtf8().constData()) >= 0;
    };
    int64_t window = int64_t(options.window);
    api.set_option(handle, "wid", MPV_FORMAT_INT64, &window);
    // Rendering: libplacebo renderer, Direct3D 11, hardware decoding.
    set("vo", QStringLiteral("gpu-next,gpu,direct3d"));
#ifdef Q_OS_WIN
    set("gpu-api", QStringLiteral("d3d11"));
#endif
    set("hwdec", options.hardware ? QStringLiteral("auto-safe") : QStringLiteral("no"));
    if (options.quality == QLatin1String("high")) {
        set("profile", QStringLiteral("high-quality"));   // EWA scaling, debanding, accurate HDR
    } else {
        set("scale", QStringLiteral("spline36"));
        set("dscale", QStringLiteral("mitchell"));
    }
    set("target-colorspace-hint", QStringLiteral("yes"));
    if (!options.cacheDirectory.isEmpty()) {
        // Compiled GPU shaders are reused: later files show their first frame sooner.
        QDir().mkpath(options.cacheDirectory);
        set("gpu-shader-cache", QStringLiteral("yes"));
        set("gpu-shader-cache-dir", QDir::toNativeSeparators(options.cacheDirectory));
    }   // HDR passthrough on HDR displays
    set("background-color", QStringLiteral("#000000"));
    // Audio: system mixer by default, optional exclusive (bit-perfect) mode.
    set("audio-exclusive", options.audioExclusive ? QStringLiteral("yes") : QStringLiteral("no"));
    set("audio-client-name", QStringLiteral("Fero Player"));
    set("volume-max", QStringLiteral("100"));
    set("audio-file-auto", QStringLiteral("no"));
    // Embedded player: Fero owns the interface and the input.
    set("keep-open", QStringLiteral("yes"));
    set("idle", QStringLiteral("yes"));
    // The video output (D3D11 device, swap chain, renderer) is created once at
    // start-up, while the library is shown, instead of when a file is opened.
    set("force-window", QStringLiteral("yes"));
    // Show the first decoded frame without waiting for the next ones.
    set("video-latency-hacks", QStringLiteral("yes"));
    set("input-default-bindings", QStringLiteral("no"));
    set("input-builtin-bindings", QStringLiteral("no"));
    set("input-vo-keyboard", QStringLiteral("no"));
    set("input-cursor", QStringLiteral("yes"));
    set("cursor-autohide", QStringLiteral("no"));
    set("osc", QStringLiteral("no"));
    set("osd-level", QStringLiteral("0"));
    set("load-scripts", QStringLiteral("no"));
    set("ytdl", QStringLiteral("no"));
    set("terminal", QStringLiteral("no"));
    set("config", QStringLiteral("no"));
    // Lower memory use: local files do not need a large read-ahead buffer.
    set("demuxer-max-bytes", QStringLiteral("64MiB"));
    set("demuxer-max-back-bytes", QStringLiteral("16MiB"));
    // Subtitles and screenshots.
    set("sub-auto", QStringLiteral("fuzzy"));
    set("sub-font", QStringLiteral("Segoe UI Semibold"));
    set("osd-font", QStringLiteral("Segoe UI Semibold"));
    set("osd-font-size", QStringLiteral("34"));
    set("osd-border-size", QStringLiteral("2"));
    set("osd-margin-x", QStringLiteral("36"));
    set("osd-margin-y", QStringLiteral("30"));
    set("sub-font-size", QStringLiteral("44"));
    set("sub-border-size", QStringLiteral("2.6"));
    set("sub-shadow-offset", QStringLiteral("1.2"));
    set("screenshot-format", QStringLiteral("png"));
    set("screenshot-tag-colorspace", QStringLiteral("yes"));
    for (const QString &option : options.extraOptions) {
        const int split = option.indexOf(QLatin1Char('='));
        if (split > 0)
            set(option.left(split).toUtf8().constData(), option.mid(split + 1));
    }
    if (api.initialize(handle) < 0) {
        *error = L("libmpv не удалось инициализировать.");
        api.terminate_destroy(handle);
        return nullptr;
    }
    api.request_log_messages(handle, "warn");
    // Runs after a file's tracks are known and before decoding starts.
    api.hook_add(handle, MpvPlayer::HookReply, "on_preloaded", 0);
    auto *player = new MpvPlayer(handle);
    if (QCoreApplication::instance())
        player->moveToThread(QCoreApplication::instance()->thread());
    player->command({QStringLiteral("define-section"), QStringLiteral("fero"),
                     QStringLiteral("MBTN_LEFT script-message fero-mouse click\n"
                                    "MBTN_LEFT_DBL script-message fero-mouse double\n"
                                    "MBTN_RIGHT script-message fero-mouse right\n"
                                    "MOUSE_MOVE script-message fero-mouse move\n"
                                    "WHEEL_UP script-message fero-mouse wheel-up\n"
                                    "WHEEL_DOWN script-message fero-mouse wheel-down\n"),
                     QStringLiteral("force")});
    player->command({QStringLiteral("enable-section"), QStringLiteral("fero"), QStringLiteral("allow-hide-cursor")});
    const std::pair<const char *, mpv_format> observed[] = {
        {"pause", MPV_FORMAT_FLAG}, {"duration", MPV_FORMAT_DOUBLE}, {"eof-reached", MPV_FORMAT_FLAG},
        {"idle-active", MPV_FORMAT_FLAG}, {"seekable", MPV_FORMAT_FLAG},
        {"paused-for-cache", MPV_FORMAT_FLAG}, {"track-list/count", MPV_FORMAT_INT64},
        {"vid", MPV_FORMAT_STRING}};
    for (const auto &[name, format] : observed)
        api.observe_property(handle, 0, name, format);
    api.observe_property(handle, MpvPlayer::OutputReply, "osd-width", MPV_FORMAT_INT64);
    api.observe_property(handle, MpvPlayer::OutputReply, "osd-height", MPV_FORMAT_INT64);
    api.observe_property(handle, MpvPlayer::ChapterReply, "chapter", MPV_FORMAT_INT64);
    api.set_wakeup_callback(handle, wakeup, player);
    return player;
}

MpvPlayer::MpvPlayer(mpv_handle *handle) : mpv(handle) {}

MpvPlayer::~MpvPlayer() {
    if (mpv) {
        api.set_wakeup_callback(mpv, nullptr, nullptr);
        api.terminate_destroy(mpv);
        mpv = nullptr;
    }
}

QString MpvPlayer::version() const {
    const QString value = mpvGet("mpv-version").toString();
    const QString ffmpeg = mpvGet("ffmpeg-version").toString();
    return ffmpeg.isEmpty() ? value : value + QStringLiteral(" · FFmpeg ") + ffmpeg;
}

void MpvPlayer::command(const QStringList &arguments) {
    std::vector<QByteArray> storage;
    for (const QString &argument : arguments)
        storage.push_back(argument.toUtf8());
    mpv_node args{};
    // Use mpv_command_node with an array for consistent error handling.
    std::vector<mpv_node> values;
    for (const QByteArray &item : storage) {
        mpv_node node{};
        node.format = MPV_FORMAT_STRING;
        node.u.string = const_cast<char *>(item.constData());
        values.push_back(node);
    }
    mpv_node_list list{int(values.size()), values.data(), nullptr};
    args.format = MPV_FORMAT_NODE_ARRAY;
    args.u.list = &list;
    mpv_node result{};
    if (api.command_node(mpv, &args, &result) >= 0)
        api.free_node_contents(&result);
}

QVariant MpvPlayer::mpvGet(const char *name) const {
    mpv_node node{};
    if (api.get_property(mpv, name, MPV_FORMAT_NODE, &node) < 0)
        return {};
    QVariant value = nodeToVariant(node);
    api.free_node_contents(&node);
    return value;
}

void MpvPlayer::mpvSet(const char *name, const QVariant &value) {
    switch (value.typeId()) {
    case QMetaType::Bool: {
        int flag = value.toBool() ? 1 : 0;
        api.set_property(mpv, name, MPV_FORMAT_FLAG, &flag);
        break;
    }
    case QMetaType::Double: {
        double number = value.toDouble();
        api.set_property(mpv, name, MPV_FORMAT_DOUBLE, &number);
        break;
    }
    case QMetaType::Int:
    case QMetaType::LongLong: {
        int64_t number = value.toLongLong();
        api.set_property(mpv, name, MPV_FORMAT_INT64, &number);
        break;
    }
    default:
        api.set_property_string(mpv, name, value.toString().toUtf8().constData());
    }
}

void MpvPlayer::open(const QString &path, qint64 startMs, const QString &audioId, const QString &subtitleId) {
    failed = false;
    ended = false;
    loading = true;
    durationMs = 0;
    isSeekable = false;
    waitingFirstFrame = true;
    errorText.clear();
    phases = QJsonObject();
    openTimer.start();
    mpvSet("pause", false);
    NodeBuilder b;
    std::vector<std::pair<QString, mpv_node>> options;
    if (startMs > 0) {
        options.push_back({QStringLiteral("start"), b.string(QString::number(startMs / 1000.0, 'f', 3))});
        // Resume at the nearest keyframe before the saved point: no decoding of
        // several seconds of hidden frames, and a short natural lead-in.
        options.push_back({QStringLiteral("hr-seek"), b.string(QStringLiteral("no"))});
    }
    if (!audioId.isEmpty())
        options.push_back({QStringLiteral("aid"), b.string(audioId)});
    if (!subtitleId.isEmpty())
        options.push_back({QStringLiteral("sid"), b.string(subtitleId)});
    std::vector<std::pair<QString, mpv_node>> entries = {
        {QStringLiteral("name"), b.string(QStringLiteral("loadfile"))},
        {QStringLiteral("url"), b.string(QDir::toNativeSeparators(path))},
        {QStringLiteral("flags"), b.string(QStringLiteral("replace"))}};
    if (!options.empty())
        entries.push_back({QStringLiteral("options"), b.map(options)});
    mpv_node commandNode = b.map(entries);
    mpv_node result{};
    const int status = api.command_node(mpv, &commandNode, &result);
    if (status >= 0) {
        api.free_node_contents(&result);
    } else {
        failed = true;
        loading = false;
        errorText = QString::fromUtf8(api.error_string(status));
    }
    updateState();
}

void MpvPlayer::togglePause() {
    if (currentState == State::Ended)
        restart();
    else
        mpvSet("pause", !paused);
}

void MpvPlayer::restart() {
    ended = false;
    command({QStringLiteral("seek"), QStringLiteral("0"), QStringLiteral("absolute")});
    mpvSet("pause", false);
    updateState();
}

void MpvPlayer::seek(qint64 milliseconds) {
    if (!isSeekable)
        return;
    qint64 target = qMax<qint64>(0, milliseconds);
    if (durationMs > 0)
        target = qMin(target, durationMs - 1);
    command({QStringLiteral("seek"), QString::number(target / 1000.0, 'f', 3), QStringLiteral("absolute")});
}

void MpvPlayer::frameStep() { command({QStringLiteral("frame-step")}); }
void MpvPlayer::stop() { command({QStringLiteral("stop")}); }
void MpvPlayer::setVolume(int value) { mpvSet("volume", double(qBound(0, value, 100))); }
void MpvPlayer::setMute(bool value) { mpvSet("mute", value); }

bool MpvPlayer::setSpeed(double value) {
    double number = value;
    return api.set_property(mpv, "speed", MPV_FORMAT_DOUBLE, &number) >= 0;
}

void MpvPlayer::setAudioDelay(int milliseconds) { mpvSet("audio-delay", milliseconds / 1000.0); }
void MpvPlayer::setSubtitleDelay(int milliseconds) { mpvSet("sub-delay", milliseconds / 1000.0); }
int MpvPlayer::audioDelay() const { return qRound(mpvGet("audio-delay").toDouble() * 1000); }
int MpvPlayer::subtitleDelay() const { return qRound(mpvGet("sub-delay").toDouble() * 1000); }

void MpvPlayer::setAspect(const QString &value) {
    if (value.isEmpty()) {
        if (api.set_property_string(mpv, "video-aspect-override", "no") < 0)
            api.set_property_string(mpv, "video-aspect-override", "-1");
    } else {
        api.set_property_string(mpv, "video-aspect-override", value.toUtf8().constData());
    }
}

bool MpvPlayer::screenshot(const QString &path) {
    NodeBuilder b;
    mpv_node node = b.map({{QStringLiteral("name"), b.string(QStringLiteral("screenshot-to-file"))},
                           {QStringLiteral("filename"), b.string(QDir::toNativeSeparators(path))},
                           {QStringLiteral("flags"), b.string(QStringLiteral("video"))}});
    mpv_node result{};
    const bool ok = api.command_node(mpv, &node, &result) >= 0;
    if (ok)
        api.free_node_contents(&result);
    return ok && QFile::exists(path);
}

QImage MpvPlayer::currentFrame(double *aspect) const {
    *aspect = 0;
    NodeBuilder b;
    mpv_node node = b.map({{QStringLiteral("name"), b.string(QStringLiteral("screenshot-raw"))},
                           {QStringLiteral("flags"), b.string(QStringLiteral("subtitles"))}});
    mpv_node result{};
    if (api.command_node(mpv, &node, &result) < 0)
        return {};
    QImage image;
    if (result.format == MPV_FORMAT_NODE_MAP) {
        qint64 w = 0, h = 0, stride = 0;
        QByteArray format;
        const mpv_byte_array *data = nullptr;
        for (int i = 0; i < result.u.list->num; ++i) {
            const char *key = result.u.list->keys[i];
            const mpv_node &value = result.u.list->values[i];
            if (value.format == MPV_FORMAT_INT64) {
                if (qstrcmp(key, "w") == 0) w = value.u.int64;
                else if (qstrcmp(key, "h") == 0) h = value.u.int64;
                else if (qstrcmp(key, "stride") == 0) stride = value.u.int64;
            } else if (value.format == MPV_FORMAT_STRING && qstrcmp(key, "format") == 0) {
                format = value.u.string;
            } else if (value.format == MPV_FORMAT_BYTE_ARRAY && qstrcmp(key, "data") == 0) {
                data = value.u.ba;
            }
        }
        if (data && w > 0 && h > 0 && stride >= w * 4 && format == "bgr0" && qint64(data->size) >= stride * h)
            image = QImage(static_cast<const uchar *>(data->data), int(w), int(h), int(stride),
                           QImage::Format_RGB32).copy();
    }
    api.free_node_contents(&result);
    const double dw = mpvGet("video-out-params/dw").toDouble(), dh = mpvGet("video-out-params/dh").toDouble();
    if (dw > 0 && dh > 0)
        *aspect = dw / dh;
    return image;
}

int MpvPlayer::chapterCount() const {
    return mpvGet("chapter-list/count").toInt();
}

QString MpvPlayer::chapterTitle(int index) const {
    return mpvGet(QByteArray("chapter-list/" + QByteArray::number(index) + "/title").constData()).toString();
}

qint64 MpvPlayer::chapterStart(int index) const {
    const QVariant time = mpvGet(QByteArray("chapter-list/" + QByteArray::number(index) + "/time").constData());
    return time.isValid() ? qint64(time.toDouble() * 1000.0) : -1;
}

void MpvPlayer::showText(const QString &text, int milliseconds) {
    // Level 0: shown although the engine's own OSD is otherwise off.
    command({QStringLiteral("show-text"), text, QString::number(milliseconds), QStringLiteral("0")});
}

void MpvPlayer::setCursorHidden(bool hidden) {
    api.set_property_string(mpv, "cursor-autohide", hidden ? "always" : "no");
}

void MpvPlayer::addSubtitle(const QString &path, bool select) {
    command({QStringLiteral("sub-add"), QDir::toNativeSeparators(path),
             select ? QStringLiteral("select") : QStringLiteral("auto")});
}

QVector<MediaTrack> MpvPlayer::tracks(bool audio) const {
    QVector<MediaTrack> result;
    const QString type = audio ? QStringLiteral("audio") : QStringLiteral("sub");
    MediaTrack off;
    off.id = -1;
    off.label = L("Выключено");
    result << off;
    int number = 0;
    bool anySelected = false;
    for (const QVariant &item : mpvGet("track-list").toList()) {
        const QVariantMap map = item.toMap();
        if (map.value(QStringLiteral("type")).toString() != type)
            continue;
        MediaTrack track;
        track.id = map.value(QStringLiteral("id")).toInt();
        track.title = map.value(QStringLiteral("title")).toString();
        track.lang = map.value(QStringLiteral("lang")).toString();
        track.codec = map.value(QStringLiteral("codec")).toString();
        track.selected = map.value(QStringLiteral("selected")).toBool();
        track.external = map.value(QStringLiteral("external")).toBool();
        track.externalFile = map.value(QStringLiteral("external-filename")).toString();
        anySelected |= track.selected;
        ++number;
        QString label = track.title;
        if (label.isEmpty() && track.external)
            label = QFileInfo(track.externalFile).fileName();
        if (label.isEmpty())
            label = (audio ? L("Дорожка %1") : L("Субтитры %1")).arg(number);
        QStringList details;
        if (!track.lang.isEmpty())
            details << track.lang.toUpper();
        if (!track.codec.isEmpty())
            details << track.codec;
        if (audio) {
            const int channels = map.value(QStringLiteral("demux-channel-count")).toInt();
            if (channels > 0)
                details << (channels == 6 ? QStringLiteral("5.1") : channels == 8 ? QStringLiteral("7.1")
                                                                                  : L("%1 кан.").arg(channels));
        }
        if (!details.isEmpty())
            label += QStringLiteral("  ·  ") + details.join(QStringLiteral(" · "));
        track.label = label;
        result << track;
    }
    result[0].selected = !anySelected;
    return result;
}

int MpvPlayer::currentTrack(bool audio) const {
    char *value = api.get_property_string(mpv, audio ? "aid" : "sid");
    if (!value)
        return -1;
    bool ok = false;
    const int id = QString::fromUtf8(value).toInt(&ok);
    api.free(value);
    if (ok)
        return id;
    for (const MediaTrack &track : tracks(audio))
        if (track.selected)
            return track.id;
    return -1;
}

bool MpvPlayer::selectTrack(bool audio, int id) {
    const QByteArray value = id < 0 ? QByteArray("no") : QByteArray::number(id);
    return api.set_property_string(mpv, audio ? "aid" : "sid", value.constData()) >= 0;
}

qint64 MpvPlayer::position() const {
    double seconds = 0;
    if (api.get_property(mpv, "time-pos", MPV_FORMAT_DOUBLE, &seconds) < 0)
        return 0;
    return qMax<qint64>(0, qint64(seconds * 1000));
}

void MpvPlayer::drainEvents() {
    while (mpv) {
        mpv_event *event = api.wait_event(mpv, 0);
        if (!event || event->event_id == MPV_EVENT_NONE)
            break;
        switch (event->event_id) {
        case MPV_EVENT_START_FILE:
            loading = true;
            break;
        case MPV_EVENT_FILE_LOADED:
            markPhase("loaded_ms");
            loading = false;
            emit fileLoaded();
            emit tracksChanged();
            break;
        case MPV_EVENT_END_FILE: {
            auto *end = static_cast<mpv_event_end_file *>(event->data);
            if (end && end->reason == MPV_END_FILE_REASON_ERROR) {
                failed = true;
                loading = false;
                errorText = QString::fromUtf8(api.error_string(end->error));
            }
            break;
        }
        case MPV_EVENT_HOOK: {
            auto *hook = static_cast<mpv_event_hook *>(event->data);
            if (event->reply_userdata == HookReply)
                applyTrackHints();
            api.hook_continue(mpv, hook->id);   // always, or playback would wait forever
            break;
        }
        case MPV_EVENT_VIDEO_RECONFIG:
            markPhase("video_ready_ms");
            break;
        case MPV_EVENT_AUDIO_RECONFIG:
            markPhase("audio_ready_ms");
            break;
        case MPV_EVENT_PLAYBACK_RESTART:
            markPhase("restart_ms");
            if (waitingFirstFrame) {
                waitingFirstFrame = false;
                emit firstFrame();
            }
            break;
        case MPV_EVENT_PROPERTY_CHANGE: {
            auto *change = static_cast<mpv_event_property *>(event->data);
            if (event->reply_userdata == ChapterReply) {
                const bool known = change->format == MPV_FORMAT_INT64 && change->data;
                emit chapterChanged(known ? int(*static_cast<int64_t *>(change->data)) : -1);
                break;
            }
            if (event->reply_userdata == OutputReply) {
                if (change->format == MPV_FORMAT_INT64 && change->data) {
                    const qint64 value = *static_cast<int64_t *>(change->data);
                    (qstrcmp(change->name, "osd-width") == 0 ? osdWidth : osdHeight) = value;
                    emit outputResized(int(osdWidth), int(osdHeight));
                }
                break;
            }
            handleProperty(change->name, change->format, change->data);
            break;
        }
        case MPV_EVENT_CLIENT_MESSAGE: {
            auto *message = static_cast<mpv_event_client_message *>(event->data);
            if (message->num_args >= 2 && qstrcmp(message->args[0], "fero-mouse") == 0)
                emit videoMouse(QString::fromUtf8(message->args[1]));
            break;
        }
        case MPV_EVENT_LOG_MESSAGE: {
            auto *message = static_cast<mpv_event_log_message *>(event->data);
            const QString line = QStringLiteral("[%1] %2: %3").arg(QString::fromUtf8(message->level),
                QString::fromUtf8(message->prefix), QString::fromUtf8(message->text).trimmed());
            appendLog(line);
            break;
        }
        default:
            break;
        }
        updateState();
    }
}

void MpvPlayer::setTrackHints(const TrackHint &audio, const TrackHint &subtitle) {
    audioHint = audio;
    subtitleHint = subtitle;
}

namespace {
QSet<QString> titleWords(const QString &title) {
    QSet<QString> words;
    static const QRegularExpression separators(QStringLiteral("[^\\w]+"),
                                               QRegularExpression::UseUnicodePropertiesOption);
    for (const QString &word : title.toCaseFolded().split(separators, Qt::SkipEmptyParts))
        words.insert(word);
    return words;
}
} // namespace

void MpvPlayer::applyTrackHints() {
    const TrackHint hints[] = {audioHint, subtitleHint};
    audioHint = subtitleHint = TrackHint();
    auto read = [this](const QByteArray &name) {
        QString value;
        if (char *text = api.get_property_string(mpv, name.constData())) {
            value = QString::fromUtf8(text);
            api.free(text);
        }
        return value;
    };
    const int count = read("track-list/count").toInt();
    for (int kind = 0; kind < 2; ++kind) {
        const TrackHint &hint = hints[kind];
        if (!hint.set)
            continue;
        // File-local: the wish applies to this episode only, never as a global default.
        const char *property = kind == 0 ? "file-local-options/aid" : "file-local-options/sid";
        if (hint.off) {
            api.set_property_string(mpv, property, "no");
            continue;
        }
        const QString type = kind == 0 ? QStringLiteral("audio") : QStringLiteral("sub");
        const QSet<QString> wanted = titleWords(hint.title);
        int best = -1, bestScore = 0;
        for (int i = 0; i < count; ++i) {
            const QByteArray prefix = "track-list/" + QByteArray::number(i) + "/";
            if (read(prefix + "type") != type || read(prefix + "external") == QLatin1String("yes"))
                continue;
            const QString title = read(prefix + "title"), lang = read(prefix + "lang");
            int score = 0;
            if (!hint.lang.isEmpty() && lang.compare(hint.lang, Qt::CaseInsensitive) == 0)
                score += 10;
            if (!hint.title.isEmpty() && title.compare(hint.title, Qt::CaseInsensitive) == 0)
                score += 6;
            score += int((titleWords(title) & wanted).size());   // same studio / dub name
            if (score > bestScore) {
                bestScore = score;
                best = read(prefix + "id").toInt();
            }
        }
        if (best > 0)
            api.set_property_string(mpv, property, QByteArray::number(best).constData());
    }
}

void MpvPlayer::markPhase(const char *name) {
    const QString key = QString::fromLatin1(name);
    if (waitingFirstFrame && openTimer.isValid() && !phases.contains(key))
        phases.insert(key, double(openTimer.elapsed()));
}

void MpvPlayer::handleProperty(const char *name, int format, void *data) {
    const bool present = format != MPV_FORMAT_NONE && data;
    const QByteArray key(name);
    if (key == "pause") {
        paused = present && *static_cast<int *>(data);
    } else if (key == "eof-reached") {
        ended = present && *static_cast<int *>(data);
    } else if (key == "idle-active") {
        idle = present && *static_cast<int *>(data);
    } else if (key == "seekable") {
        isSeekable = present && *static_cast<int *>(data);
    } else if (key == "paused-for-cache") {
        buffering = present && *static_cast<int *>(data);
    } else if (key == "duration") {
        const qint64 value = present ? qint64(*static_cast<double *>(data) * 1000) : 0;
        if (value != durationMs) {
            durationMs = value;
            emit durationChanged(durationMs);
        }
    } else if (key == "track-list/count") {
        emit tracksChanged();
    } else if (key == "vid") {
        const QString value = present ? QString::fromUtf8(*static_cast<char **>(data)) : QString();
        videoPresent = !value.isEmpty() && value != QLatin1String("no");
    }
}

void MpvPlayer::updateState() {
    State next;
    if (failed)
        next = State::Error;
    else if (loading)
        next = State::Opening;
    else if (idle)
        next = State::Idle;
    else if (ended)
        next = State::Ended;
    else if (buffering)
        next = State::Buffering;
    else if (paused)
        next = State::Paused;
    else
        next = State::Playing;
    if (next != currentState) {
        currentState = next;
        emit stateChanged(currentState);
    }
}
