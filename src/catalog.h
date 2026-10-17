#pragma once
#include <QJsonObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>
#include <optional>

// Local settings, media library, queue and resume points.
// Stored in %LOCALAPPDATA%\FeroPlayer\library.json (moved from the former LumaPlayer folder).

struct TrackChoice {
    int id = 0;           // -1: disabled
    QString name;
    QString lang;
    QString external;     // external subtitle path
};

struct TrackPreferences {
    std::optional<TrackChoice> audio, subtitle;
};

struct Settings {
    QString theme = QStringLiteral("dark");
    int volume = 75;
    bool resume = true, autoplay = true, hardware = true;
    double speed = 1.0;
    QString quality = QStringLiteral("high");   // high | balanced
    bool audioExclusive = false;
    QString language = QStringLiteral("en");   // en | ru
};

QString pathKey(const QString &path);
QString formatTime(qint64 milliseconds);
QStringList naturalSorted(QStringList names);
QStringList scanFolder(const QString &directory);
std::optional<int> nextQueueIndex(int current, int count);

extern const QSet<QString> mediaExtensions;
extern const QSet<QString> subtitleExtensions;
extern const QSet<QString> audioExtensions;
QString suffixOf(const QString &path);

class Catalog {
public:
    explicit Catalog(const QString &directory = QString());

    QString directory() const { return dir; }
    QString lastError() const { return error; }
    Settings settings;
    QStringList queue;

    bool save();
    QStringList add(const QStringList &paths);
    const QJsonObject *record(const QString &path) const;
    void markPlayed(const QString &path);
    void updatePosition(const QString &path, qint64 position, qint64 duration, bool ended = false);
    TrackPreferences trackPreferences(const QString &path) const;
    void rememberTrack(const QString &path, bool audio, const TrackChoice &choice);
    // Per-folder (series) choice: the next episodes pick the same dub/subtitles.
    void rememberSeriesTrack(const QString &path, bool audio, const TrackChoice &choice);
    std::optional<TrackChoice> seriesTrack(const QString &path, bool audio) const;
    QVector<QJsonObject> recent(int limit = 12) const;
    QVector<QJsonObject> allItems() const;
    void forget(const QString &path);
    int librarySize() const { return int(library.size()); }

    static qint64 resumePosition(const QJsonObject *record);

private:
    void load();
    QString dir, file, error;
    QHash<QString, QJsonObject> library;
    QStringList order;   // insertion order of keys, keeps the file stable
    QJsonObject extraSettings;
    QJsonObject series;   // folder key -> {audio: {name, lang, off}, subtitle: {...}}
};
