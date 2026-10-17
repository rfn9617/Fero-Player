#include "selftest.h"
#include "playerwindow.h"
#include "widgets.h"

#include <QApplication>
#include <QCursor>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPainter>
#include <QTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#endif

namespace {
double workingSetMb() {
#ifdef Q_OS_WIN
    PROCESS_MEMORY_COUNTERS counters{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)))
        return counters.WorkingSetSize / 1048576.0;
#endif
    return 0;
}
} // namespace

SelfTest::SelfTest(PlayerWindow *window, const QString &media, const QString &output)
    : QObject(window), window(window), media(media), output(output) {}

void SelfTest::start() {
    QDir().mkpath(output);
    QTimer::singleShot(400, this, [this] { step(); });
}

void SelfTest::capture(const QString &name) {
    // Render Fero's own widgets (never the screen): other windows stay private
    // and the test works while the user keeps using the computer. The native
    // video area renders black here.
    QApplication::processEvents();
    QImage image = window->grab().toImage();
    QPainter painter(&image);
    const QPoint origin = window->mapToGlobal(QPoint());
    for (QWidget *overlay : {static_cast<QWidget *>(window->topbar), static_cast<QWidget *>(window->controls)})
        if (overlay->isWindow() && overlay->isVisible())
            painter.drawImage(overlay->geometry().topLeft() - origin, overlay->grab().toImage());
    painter.end();
    image.save(QDir(output).filePath(name + QStringLiteral(".png")));
}

void SelfTest::step() {
    auto later = [this](int ms) { QTimer::singleShot(ms, this, [this] { step(); }); };
    switch (stage++) {
    case 0:
        window->resize(1280, 760);
        capture(QStringLiteral("0-start"));
        if (!window->engine) {   // engine still loading
            --stage;
            later(200);
            return;
        }
        idleMemory = workingSetMb();
        {
            // A small library for the tile view: the demo clips next to the test
            // clip (tools/demo), or copies of the test clip when they are missing.
            const QString folder = QDir(output).filePath(QStringLiteral("library"));
            QDir().mkpath(folder);
            const QDir demo(QFileInfo(media).dir().filePath(QStringLiteral("demo")));
            QStringList copies;
            const char *names[] = {"Night Drive.webm", "Ocean Waves.webm", "City Lights.webm",
                                   "Mountain Trail.webm", "Episode 05.webm", "Episode 06.webm"};
            for (const char *name : names) {
                const QString target = QDir(folder).filePath(QString::fromLatin1(name));
                const QString source = demo.filePath(QString::fromLatin1(name));
                const QString from = QFileInfo::exists(source) ? source : media;
                if (QFileInfo(target).size() != QFileInfo(from).size()) {   // same file: keep (and its cached preview)
                    QFile::remove(target);
                    QFile::copy(from, target);
                }
                copies << target;
            }
            window->catalog->add(copies);
        }
        window->openPaths({media});
        later(2500);
        return;
    case 1:
        capture(QStringLiteral("1-playing-panels"));
        later(3600);
        return;
    case 2: {
        window->hideChrome();
        later(400);
        return;
    }
    case 3:
        capture(QStringLiteral("2-playing-hidden"));
        window->showChrome(true, false);
        later(400);
        return;
    case 4:
        capture(QStringLiteral("3-top-edge"));
        window->hideChrome();
        window->showChrome(false, true);
        later(400);
        return;
    case 5:
        capture(QStringLiteral("4-bottom-edge"));
        window->playPause();
        later(500);
        return;
    case 6:
        capture(QStringLiteral("5-paused"));
        window->navigate(QStringLiteral("library"));
        later(3500);   // previews are generated in the background
        return;
    case 7:
        capture(QStringLiteral("6-library"));
        window->playLibrary(QDir(QDir(output).filePath(QStringLiteral("library"))).filePath(QStringLiteral("Episode 05.webm")));
        window->navigate(QStringLiteral("queue"));
        later(3000);
        return;
    case 8:
        capture(QStringLiteral("6b-up-next"));
        window->navigate(QStringLiteral("player"));
        window->playPause();
        window->toggleFullscreen();
        later(1200);
        return;
    case 9:
        capture(QStringLiteral("7-fullscreen"));
        window->toggleFullscreen();
        later(800);
        return;
    case 10:
        capture(QStringLiteral("8-after-fullscreen"));
        firstOpen = QJsonDocument::fromJson([this] {
            QFile f(QDir(window->catalog->directory()).filePath(QStringLiteral("playback-metrics.json")));
            return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
        }()).object();
        window->playIndex(0, true);   // second open: warm caches
        later(2500);
        return;
    case 11: {
        QJsonObject report = window->startupMetrics;
        report.insert(QStringLiteral("working_set_mb_idle"), idleMemory);
        report.insert(QStringLiteral("fullscreen_restored"), !window->isFullScreen());
        report.insert(QStringLiteral("first_open"), firstOpen);
        report.insert(QStringLiteral("working_set_mb_playing"), workingSetMb());
        report.insert(QStringLiteral("engine"), window->engine ? window->engine->version() : QString());
        report.insert(QStringLiteral("state"), int(window->currentState));
        report.insert(QStringLiteral("duration_ms"), double(window->lastDuration));
        report.insert(QStringLiteral("has_video"), window->engine && window->engine->hasVideo());
        QJsonArray audio, subtitles;
        if (window->engine) {
            for (const MediaTrack &track : window->engine->tracks(true))
                audio.append(track.label);
            for (const MediaTrack &track : window->engine->tracks(false))
                subtitles.append(track.label);
            report.insert(QStringLiteral("error"), window->engine->lastError());
        }
        report.insert(QStringLiteral("audio_tracks"), audio);
        report.insert(QStringLiteral("subtitle_tracks"), subtitles);
        QFile metrics(QDir(window->catalog->directory()).filePath(QStringLiteral("playback-metrics.json")));
        if (metrics.open(QIODevice::ReadOnly))
            report.insert(QStringLiteral("playback"), QJsonDocument::fromJson(metrics.readAll()).object());
        QFile file(QDir(output).filePath(QStringLiteral("self-test.json")));
        if (file.open(QIODevice::WriteOnly))
            file.write(QJsonDocument(report).toJson());
        window->close();
        QApplication::quit();
        return;
    }
    default:
        return;
    }
}
