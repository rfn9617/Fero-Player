#include "catalog.h"

#include <QCollator>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QDirIterator>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>
#include <algorithm>

const QSet<QString> mediaExtensions = {
    "mkv", "mp4", "avi", "mov", "webm", "m4v", "wmv", "flv", "mpg", "mpeg", "ts", "m2ts", "mts",
    "vob", "ogv", "3gp", "3g2", "divx", "asf", "mp3", "flac", "wav", "ogg", "m4a", "aac", "opus",
    "wma", "aiff", "aif", "ape", "alac", "mp2", "ac3", "eac3", "dts", "mka", "mpv", "m2v", "mxf",
    "rm", "rmvb", "f4v", "hevc", "h264", "265", "264", "y4m", "nut", "dv", "wv", "tta", "dsf",
    "dff", "mpc", "spx", "amr", "caf", "mlp", "thd", "avs", "ivf", "evo", "m2p", "ps", "trp",
    "tp", "ogm", "mk3d", "webp", "gif", "wtv", "dvr-ms", "mod", "tod", "bik", "smk", "xvid"};
const QSet<QString> subtitleExtensions = {"srt", "ass", "ssa", "vtt", "sub", "idx", "sup", "smi", "txt", "lrc"};
const QSet<QString> audioExtensions = {
    "mp3", "flac", "wav", "ogg", "m4a", "aac", "opus", "wma", "aiff", "aif", "ape", "alac", "mka",
    "ac3", "eac3", "dts", "mp2", "wv", "tta", "dsf", "dff", "mpc", "spx", "amr", "caf", "mlp", "thd"};

QString suffixOf(const QString &path) {
    return QFileInfo(path).suffix().toLower();
}

QString pathKey(const QString &path) {
    return QDir::toNativeSeparators(QDir::cleanPath(QFileInfo(path).absoluteFilePath())).toLower();
}

QString formatTime(qint64 milliseconds) {
    qint64 seconds = qMax<qint64>(0, milliseconds) / 1000;
    const qint64 hours = seconds / 3600;
    const qint64 minutes = (seconds % 3600) / 60;
    seconds %= 60;
    if (hours)
        return QStringLiteral("%1:%2:%3").arg(hours).arg(minutes, 2, 10, QLatin1Char('0')).arg(seconds, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2").arg(minutes).arg(seconds, 2, 10, QLatin1Char('0'));
}

static QCollator &collator() {
    static QCollator c = [] {
        QCollator value;
        value.setNumericMode(true);
        value.setCaseSensitivity(Qt::CaseInsensitive);
        return value;
    }();
    return c;
}

QStringList naturalSorted(QStringList names) {
    std::sort(names.begin(), names.end(), [](const QString &a, const QString &b) { return collator().compare(a, b) < 0; });
    return names;
}

QStringList scanFolder(const QString &directory) {
    // Breadth by folder, natural order, never follows directory links.
    QStringList result;
    QStringList pending{directory};
    while (!pending.isEmpty()) {
        const QString current = pending.takeFirst();
        QDir dir(current);
        const QStringList files = naturalSorted(dir.entryList(QDir::Files | QDir::Readable));
        for (const QString &name : files)
            if (mediaExtensions.contains(suffixOf(name)))
                result << QDir::toNativeSeparators(dir.absoluteFilePath(name));
        QStringList subdirs = naturalSorted(dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks));
        for (int i = subdirs.size() - 1; i >= 0; --i)
            if (!subdirs[i].startsWith(QLatin1Char('.')))
                pending.prepend(dir.absoluteFilePath(subdirs[i]));
    }
    return result;
}

std::optional<int> nextQueueIndex(int current, int count) {
    if (current + 1 < count)
        return current + 1;
    return std::nullopt;
}

Catalog::Catalog(const QString &directory) {
    if (directory.isEmpty()) {
        const QString local = qEnvironmentVariable("LOCALAPPDATA");
        const QDir base(local.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) : local);
        dir = base.filePath(QStringLiteral("FeroPlayer"));
        // The player was called Luma Player before: keep its library and history.
        const QString previous = base.filePath(QStringLiteral("LumaPlayer"));
        if (!QFileInfo::exists(dir) && QFileInfo::exists(previous) && !QDir().rename(previous, dir)) {
            QDir().mkpath(dir);
            QFile::copy(QDir(previous).filePath(QStringLiteral("library.json")),
                        QDir(dir).filePath(QStringLiteral("library.json")));
        }
    } else {
        dir = directory;
    }
    file = QDir(dir).filePath(QStringLiteral("library.json"));
    load();
}

void Catalog::load() {
    QFile input(file);
    if (!input.exists())
        return;
    if (!input.open(QIODevice::ReadOnly)) {
        error = input.errorString();
        return;
    }
    QJsonParseError parse{};
    const QJsonDocument document = QJsonDocument::fromJson(input.readAll(), &parse);
    if (parse.error != QJsonParseError::NoError || !document.isObject()) {
        error = parse.errorString();   // the original file is left untouched
        return;
    }
    const QJsonObject data = document.object();
    series = data.value(QStringLiteral("series")).toObject();
    const QJsonObject s = data.value(QStringLiteral("settings")).toObject();
    extraSettings = s;
    auto text = [&](const char *key, QString &target, std::initializer_list<const char *> allowed) {
        const QString value = s.value(QLatin1String(key)).toString();
        for (const char *option : allowed)
            if (value == QLatin1String(option))
                target = value;
    };
    text("theme", settings.theme, {"dark", "light"});
    text("quality", settings.quality, {"high", "balanced"});
    text("language", settings.language, {"en", "ru"});
    if (s.contains(QStringLiteral("volume")))
        settings.volume = qBound(0, s.value(QStringLiteral("volume")).toInt(75), 100);
    if (s.contains(QStringLiteral("resume")))
        settings.resume = s.value(QStringLiteral("resume")).toBool(true);
    if (s.contains(QStringLiteral("autoplay")))
        settings.autoplay = s.value(QStringLiteral("autoplay")).toBool(true);
    if (s.contains(QStringLiteral("hardware")))
        settings.hardware = s.value(QStringLiteral("hardware")).toBool(true);
    // Exclusive audio was easy to mistake for "best quality"; the old setting is
    // dropped once so everyone starts from the shared Windows mixer again.
    extraSettings.remove(QStringLiteral("audio_exclusive"));
    extraSettings.remove(QStringLiteral("repeat"));   // removed feature
    if (s.contains(QStringLiteral("audio_exclusive_dac")))
        settings.audioExclusive = s.value(QStringLiteral("audio_exclusive_dac")).toBool(false);
    const double speed = s.value(QStringLiteral("speed")).toDouble(1.0);
    settings.speed = (speed >= 0.25 && speed <= 2.0) ? speed : 1.0;
    for (const QJsonValue &value : data.value(QStringLiteral("library")).toArray()) {
        const QJsonObject item = value.toObject();
        const QString path = item.value(QStringLiteral("path")).toString();
        if (path.isEmpty())
            continue;
        const QString key = pathKey(path);
        if (!library.contains(key))
            order << key;
        library.insert(key, item);
    }
    for (const QJsonValue &value : data.value(QStringLiteral("queue")).toArray())
        if (value.isString())
            queue << value.toString();
}

bool Catalog::save() {
    QDir().mkpath(dir);
    QJsonObject s = extraSettings;   // keeps keys written by other versions
    s.insert(QStringLiteral("theme"), settings.theme);
    s.insert(QStringLiteral("volume"), settings.volume);
    s.insert(QStringLiteral("resume"), settings.resume);
    s.insert(QStringLiteral("autoplay"), settings.autoplay);
    s.insert(QStringLiteral("hardware"), settings.hardware);
    s.insert(QStringLiteral("speed"), settings.speed);
    s.insert(QStringLiteral("quality"), settings.quality);
    s.insert(QStringLiteral("audio_exclusive_dac"), settings.audioExclusive);
    s.insert(QStringLiteral("language"), settings.language);
    QJsonArray items;
    for (const QString &key : order)
        if (library.contains(key))
            items.append(library.value(key));
    QJsonObject data{{QStringLiteral("schema"), 1}, {QStringLiteral("settings"), s},
                     {QStringLiteral("library"), items}, {QStringLiteral("queue"), QJsonArray::fromStringList(queue)},
                     {QStringLiteral("series"), series}};
    QSaveFile output(file);
    if (!output.open(QIODevice::WriteOnly)) {
        error = output.errorString();
        return false;
    }
    output.write(QJsonDocument(data).toJson(QJsonDocument::Indented));
    if (!output.commit()) {
        error = output.errorString();
        return false;
    }
    error.clear();
    return true;
}

QStringList Catalog::add(const QStringList &paths) {
    QStringList valid;
    QSet<QString> seen;
    for (const QString &raw : paths) {
        QFileInfo info(raw);
        if (!info.isFile())
            continue;
        const QString path = QDir::toNativeSeparators(info.canonicalFilePath().isEmpty() ? info.absoluteFilePath()
                                                                                         : info.canonicalFilePath());
        const QString key = pathKey(path);
        if (seen.contains(key))
            continue;
        seen.insert(key);
        valid << path;
        if (!library.contains(key)) {
            order << key;
            library.insert(key, QJsonObject{{QStringLiteral("path"), path},
                                            {QStringLiteral("title"), info.completeBaseName()},
                                            {QStringLiteral("added"), QDateTime::currentMSecsSinceEpoch() / 1000.0},
                                            {QStringLiteral("played"), 0},
                                            {QStringLiteral("position"), 0},
                                            {QStringLiteral("duration"), 0}});
        }
    }
    return valid;
}

const QJsonObject *Catalog::record(const QString &path) const {
    auto found = library.constFind(pathKey(path));
    return found == library.constEnd() ? nullptr : &found.value();
}

void Catalog::markPlayed(const QString &path) {
    add({path});
    auto found = library.find(pathKey(path));
    if (found != library.end())
        found->insert(QStringLiteral("played"), QDateTime::currentMSecsSinceEpoch() / 1000.0);
}

void Catalog::updatePosition(const QString &path, qint64 position, qint64 duration, bool ended) {
    auto found = library.find(pathKey(path));
    if (found == library.end())
        return;
    found->insert(QStringLiteral("position"), double(ended ? 0 : qMax<qint64>(0, position)));
    found->insert(QStringLiteral("duration"), double(qMax<qint64>(0, duration)));
}

qint64 Catalog::resumePosition(const QJsonObject *record) {
    if (!record)
        return 0;
    const qint64 position = qint64(record->value(QStringLiteral("position")).toDouble());
    const qint64 duration = qint64(record->value(QStringLiteral("duration")).toDouble());
    // Stopped during the end credits counts as watched (last 5 %, 10–90 s).
    const qint64 tail = qBound(qint64(10000), duration / 20, qint64(90000));
    if (position < 10000 || (duration > 0 && position >= duration - tail))
        return 0;
    return position;
}

TrackPreferences Catalog::trackPreferences(const QString &path) const {
    TrackPreferences result;
    const QJsonObject *item = record(path);
    if (!item)
        return result;
    const QJsonObject tracks = item->value(QStringLiteral("tracks")).toObject();
    for (const char *kind : {"audio", "subtitle"}) {
        const QJsonValue value = tracks.value(QLatin1String(kind));
        if (!value.isObject())
            continue;
        const QJsonObject o = value.toObject();
        if (!o.value(QStringLiteral("id")).isDouble())
            continue;
        TrackChoice choice;
        choice.id = o.value(QStringLiteral("id")).toInt();
        if (choice.id < -1)
            continue;
        choice.name = o.value(QStringLiteral("name")).toString();
        choice.lang = o.value(QStringLiteral("lang")).toString();
        choice.external = o.value(QStringLiteral("external")).toString();
        if (qstrcmp(kind, "audio") == 0)
            result.audio = choice;
        else
            result.subtitle = choice;
    }
    return result;
}

namespace {
QString folderKey(const QString &path) {
    return pathKey(QFileInfo(path).absolutePath());
}
} // namespace

void Catalog::rememberSeriesTrack(const QString &path, bool audio, const TrackChoice &choice) {
    if (!choice.external.isEmpty() || choice.id < -1)
        return;   // an external subtitle file belongs to one episode only
    const QString key = folderKey(path);
    QJsonObject folder = series.value(key).toObject();
    QJsonObject o{{QStringLiteral("off"), choice.id < 0}};
    if (!choice.name.isEmpty())
        o.insert(QStringLiteral("name"), choice.name);
    if (!choice.lang.isEmpty())
        o.insert(QStringLiteral("lang"), choice.lang);
    folder.insert(audio ? QStringLiteral("audio") : QStringLiteral("subtitle"), o);
    series.insert(key, folder);
}

std::optional<TrackChoice> Catalog::seriesTrack(const QString &path, bool audio) const {
    const QJsonObject o = series.value(folderKey(path)).toObject()
                              .value(audio ? QStringLiteral("audio") : QStringLiteral("subtitle")).toObject();
    if (o.isEmpty())
        return std::nullopt;
    TrackChoice choice;
    choice.id = o.value(QStringLiteral("off")).toBool() ? -1 : 0;
    choice.name = o.value(QStringLiteral("name")).toString();
    choice.lang = o.value(QStringLiteral("lang")).toString();
    return choice;
}

void Catalog::rememberTrack(const QString &path, bool audio, const TrackChoice &choice) {
    auto found = library.find(pathKey(path));
    if (found == library.end() || choice.id < -1)
        return;
    QJsonObject tracks = found->value(QStringLiteral("tracks")).toObject();
    QJsonObject o{{QStringLiteral("id"), choice.id}, {QStringLiteral("name"), choice.name}};
    if (!choice.lang.isEmpty())
        o.insert(QStringLiteral("lang"), choice.lang);
    if (!audio && !choice.external.isEmpty())
        o.insert(QStringLiteral("external"), choice.external);
    tracks.insert(audio ? QStringLiteral("audio") : QStringLiteral("subtitle"), o);
    found->insert(QStringLiteral("tracks"), tracks);
}

QVector<QJsonObject> Catalog::recent(int limit) const {
    QVector<QJsonObject> items;
    for (const QJsonObject &item : library)
        if (item.value(QStringLiteral("played")).toDouble() > 0)
            items << item;
    std::sort(items.begin(), items.end(), [](const QJsonObject &a, const QJsonObject &b) {
        return a.value(QStringLiteral("played")).toDouble() > b.value(QStringLiteral("played")).toDouble();
    });
    if (items.size() > limit)
        items.resize(limit);
    return items;
}

QVector<QJsonObject> Catalog::allItems() const {
    QVector<QJsonObject> items;
    items.reserve(library.size());
    for (const QJsonObject &item : library)
        items << item;
    std::sort(items.begin(), items.end(), [](const QJsonObject &a, const QJsonObject &b) {
        return collator().compare(a.value(QStringLiteral("title")).toString(), b.value(QStringLiteral("title")).toString()) < 0;
    });
    return items;
}

void Catalog::forget(const QString &path) {
    const QString key = pathKey(path);
    library.remove(key);
    order.removeAll(key);
}
