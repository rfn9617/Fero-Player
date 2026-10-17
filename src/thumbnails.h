#pragma once
#include <QCache>
#include <QHash>
#include <QImage>
#include <QObject>
#include <QPixmap>
#include <QSet>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

// Video previews for library tiles. Generated once on a background thread
// (Windows' own thumbnail cache first, a frame decoded by libmpv otherwise),
// stored as small JPEG files and kept in memory only while needed.
class Thumbnails : public QObject {
    Q_OBJECT
public:
    explicit Thumbnails(const QString &cacheDirectory, QObject *parent = nullptr);
    ~Thumbnails() override;
    // Returns a cached preview or a null pixmap; a missing one is queued.
    QPixmap get(const QString &path);
signals:
    void ready(const QString &path);
private:
    void work();
    void deliver(const QString &path, const QImage &image);
    QString cacheFile(const QString &path) const;
    QImage produce(const QString &path);

    QString directory;
    QCache<QString, QPixmap> memory{48 * 1024};   // cost in KiB
    QSet<QString> failed;
    std::mutex mutex;
    std::condition_variable wake;
    std::deque<QString> queue;
    QSet<QString> queued;
    std::atomic_bool stopping{false};
    std::thread worker;
};
