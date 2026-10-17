#include "thumbnails.h"
#include "catalog.h"
#include "mpvplayer.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QPainter>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shobjidl.h>
#endif

namespace {
constexpr int ThumbWidth = 480, ThumbHeight = 270;

// Fill a 16:9 frame (cropping, never stretching).
QImage cover(const QImage &source) {
    if (source.isNull())
        return {};
    const QImage scaled = source.scaled(ThumbWidth, ThumbHeight, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    return scaled.copy((scaled.width() - ThumbWidth) / 2, (scaled.height() - ThumbHeight) / 2, ThumbWidth, ThumbHeight)
        .convertToFormat(QImage::Format_RGB32);
}

#ifdef Q_OS_WIN
QImage shellThumbnail(const QString &path) {
    IShellItemImageFactory *factory = nullptr;
    const std::wstring native = QDir::toNativeSeparators(path).toStdWString();
    if (FAILED(SHCreateItemFromParsingName(native.c_str(), nullptr, IID_PPV_ARGS(&factory))) || !factory)
        return {};
    HBITMAP bitmap = nullptr;
    SIZE size{ThumbWidth, ThumbWidth};
    QImage image;
    if (SUCCEEDED(factory->GetImage(size, SIIGBF_THUMBNAILONLY | SIIGBF_BIGGERSIZEOK, &bitmap)) && bitmap) {
        image = QImage::fromHBITMAP(bitmap);
        DeleteObject(bitmap);
    }
    factory->Release();
    return image;
}
#endif
} // namespace

Thumbnails::Thumbnails(const QString &cacheDirectory, QObject *parent) : QObject(parent), directory(cacheDirectory) {
    QDir().mkpath(directory);
    worker = std::thread([this] { work(); });
}

Thumbnails::~Thumbnails() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        stopping = true;
        queue.clear();
    }
    wake.notify_all();
    if (worker.joinable())
        worker.join();
}

QString Thumbnails::cacheFile(const QString &path) const {
    const QFileInfo info(path);
    const QByteArray key = (pathKey(path) + QLatin1Char('|') + QString::number(info.size()) + QLatin1Char('|') +
                            QString::number(info.lastModified().toMSecsSinceEpoch())).toUtf8();
    return QDir(directory).filePath(
        QString::fromLatin1(QCryptographicHash::hash(key, QCryptographicHash::Sha1).toHex().left(24)) + QStringLiteral(".jpg"));
}

QPixmap Thumbnails::get(const QString &path) {
    if (QPixmap *found = memory.object(path))
        return *found;
    if (failed.contains(path))
        return {};
    std::lock_guard<std::mutex> lock(mutex);
    if (!queued.contains(path)) {
        queued.insert(path);
        queue.push_front(path);   // the most recently shown tile first
        wake.notify_one();
    }
    return {};
}

QImage Thumbnails::produce(const QString &path) {
    const QString file = cacheFile(path);
    QImage image(file);
    if (!image.isNull())
        return image;
#ifdef Q_OS_WIN
    image = cover(shellThumbnail(path));
#endif
    if (image.isNull() && !audioExtensions.contains(suffixOf(path))) {
        const QString frame = file + QStringLiteral(".png");
        if (MpvPlayer::extractFrame(path, frame, ThumbWidth, &stopping))
            image = cover(QImage(frame));
        QFile::remove(frame);
    }
    if (!image.isNull())
        image.save(file, "JPG", 86);
    return image;
}

void Thumbnails::work() {
#ifdef Q_OS_WIN
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
#endif
    for (;;) {
        QString path;
        {
            std::unique_lock<std::mutex> lock(mutex);
            wake.wait(lock, [this] { return stopping || !queue.empty(); });
            if (stopping)
                break;
            path = queue.front();
            queue.pop_front();
        }
        const QImage image = QFileInfo(path).isFile() ? produce(path) : QImage();
        QMetaObject::invokeMethod(this, [this, path, image] { deliver(path, image); }, Qt::QueuedConnection);
    }
#ifdef Q_OS_WIN
    CoUninitialize();
#endif
}

void Thumbnails::deliver(const QString &path, const QImage &image) {
    {
        std::lock_guard<std::mutex> lock(mutex);
        queued.remove(path);
    }
    if (image.isNull())
        failed.insert(path);
    else
        memory.insert(path, new QPixmap(QPixmap::fromImage(image)), qMax<qsizetype>(1, image.sizeInBytes() / 1024));
    emit ready(path);
}
