#include "icons.h"

#include <QHash>
#include <QIconEngine>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>
#include <cmath>

namespace {

const char *seekArrow = R"(<path d="M5.636 5.636A9 9 0 1 1 3.137 13.563M5.636 2v3.636h3.636"/>)";
const char *seekDigits =
    R"(<g fill="{c}" stroke="none" fill-rule="evenodd">)"
    R"(<path d="M7.1 9.6l1.6-1.4h1.4v7.6H8.7v-5.7l-1.6 1.3z"/>)"
    R"(<path d="M13.9 8.2c-1.7 0-2.6 1.5-2.6 3.8s.9 3.8 2.6 3.8 2.6-1.5 2.6-3.8-.9-3.8-2.6-3.8z)"
    R"(m0 1.45c.75 0 1.15.8 1.15 2.35s-.4 2.35-1.15 2.35-1.15-.8-1.15-2.35.4-2.35 1.15-2.35z"/></g>)";

const QHash<QString, QString> &paths() {
    static const QHash<QString, QString> table = [] {
        QHash<QString, QString> t;
        t["play"] = R"(<path d="M8 5l12 7-12 7z" fill="{c}" stroke="none"/>)";
        t["pause"] = R"(<path d="M8.5 5v14M15.5 5v14" stroke-width="3.5"/>)";
        t["home"] = R"(<path d="M3 10l9-7 9 7v10H4V10M9 20v-7h6v7"/>)";
        t["video"] = R"(<rect x="3" y="4" width="18" height="16" rx="3"/><path d="M10 8l6 4-6 4z" fill="{c}" stroke="none"/>)";
        t["queue"] = R"(<path d="M4 5h14M4 10h14M4 15h8M4 20h8M17 14l5 3-5 3z"/>)";
        t["folder"] = R"(<path d="M3 7V5h6l2 2h10v13H3zM3 10h18"/>)";
        t["file_plus"] = R"(<path d="M14 3H7a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V8z"/><path d="M14 3v5h5M12 11.5v6M9 14.5h6"/>)";
        t["folder_plus"] = R"(<path d="M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v9a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/><path d="M12 10.5v6M9 13.5h6"/>)";
        t["file_open"] = R"(<path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z" stroke-width="2"/><path d="M14 2v6h6" stroke-width="2"/><path d="M10 11.5v6l5-3z" fill="{c}" stroke-width="1.2"/>)";
        t["folder_add"] = R"(<path d="M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v9a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z" stroke-width="2"/><path d="M12 10v6M9 13h6" stroke-width="2"/>)";
        t["plus"] = R"(<path d="M12 5v14M5 12h14"/>)";
        t["previous"] = R"(<path d="M5 5v14M19 5l-10 7 10 7z"/>)";
        t["next"] = R"(<path d="M19 5v14M5 5l10 7-10 7z"/>)";
        t["rewind"] = QString::fromLatin1(seekArrow) + QString::fromLatin1(seekDigits);
        t["skip"] = QString::fromLatin1("<g transform=\"translate(24 0) scale(-1 1)\">") +
                    QString::fromLatin1(seekArrow) + QStringLiteral("</g>") + QString::fromLatin1(seekDigits);
        t["volume"] = R"(<path d="M3 9h4l5-4v14l-5-4H3zM16 8a6 6 0 0 1 0 8M19 5a10 10 0 0 1 0 14"/>)";
        t["mute"] = R"(<path d="M3 9h4l5-4v14l-5-4H3zM16 9l6 6M22 9l-6 6"/>)";
        t["fullscreen"] = R"(<path d="M8 3H3v5M16 3h5v5M3 16v5h5M16 21h5v-5"/>)";
        t["restore"] = R"(<path d="M3 8h5V3M21 8h-5V3M3 16h5v5M21 16h-5v5"/>)";
        t["cc"] = R"(<rect x="2.5" y="4.5" width="19" height="15" rx="3.5"/><path d="M6.5 12.5h3.5M12.5 12.5h5M6.5 15.8h7.5M16.5 15.8h1"/>)";
        t["audio"] = R"(<path d="M9 17V5l11-2v12M9 8l11-2"/><ellipse cx="6" cy="18" rx="3" ry="2"/><ellipse cx="17" cy="16" rx="3" ry="2"/>)";
        t["audio_track"] = R"(<path d="M6.5 4.5h11a3.5 3.5 0 0 1 3.5 3.5v6a3.5 3.5 0 0 1-3.5 3.5H11l-4.5 3.2v-3.2a3.5 3.5 0 0 1-3.5-3.5V8a3.5 3.5 0 0 1 3.5-3.5z"/><path d="M8 10v2M10.7 8.6v4.8M13.4 9.6v2.8M16 10.3v1.4"/>)";
        t["player"] = R"(<rect x="2.5" y="4.5" width="19" height="13" rx="3"/><path d="M8 21h8"/><path d="M10.2 8.2l4.4 2.8-4.4 2.8z" fill="{c}" stroke="none"/>)";
        t["more"] = R"(<circle cx="5" cy="12" r="1.4" fill="{c}"/><circle cx="12" cy="12" r="1.4" fill="{c}"/><circle cx="19" cy="12" r="1.4" fill="{c}"/>)";
        t["search"] = R"(<circle cx="10.5" cy="10.5" r="6.5"/><path d="M15.5 15.5l5 5"/>)";
        t["close"] = R"(<path d="M6 6l12 12M18 6L6 18"/>)";
        return t;
    }();
    return table;
}

class VectorIconEngine final : public QIconEngine {
public:
    explicit VectorIconEngine(const QByteArray &svg) : svg(svg), renderer(svg) {}
    QIconEngine *clone() const override { return new VectorIconEngine(svg); }
    QString key() const override { return QStringLiteral("FeroVector"); }
    bool isNull() override { return !renderer.isValid(); }

    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State) override {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        if (mode == QIcon::Disabled)
            painter->setOpacity(painter->opacity() * 0.4);
        renderer.render(painter, QRectF(rect));
        painter->restore();
    }

    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override {
        return scaledPixmap(size, mode, state, 1.0);
    }

    QPixmap scaledPixmap(const QSize &size, QIcon::Mode mode, QIcon::State state, qreal scale) override {
        const int w = qMax(1, int(std::floor(size.width() * scale + 0.5)));
        const int h = qMax(1, int(std::floor(size.height() * scale + 0.5)));
        const QString cacheKey = QStringLiteral("%1x%2@%3/%4").arg(w).arg(h).arg(scale).arg(int(mode));
        auto found = cache.constFind(cacheKey);
        if (found != cache.constEnd())
            return *found;
        QPixmap result(w, h);
        result.fill(Qt::transparent);
        QPainter painter(&result);
        paint(&painter, result.rect(), mode, state);
        painter.end();
        result.setDevicePixelRatio(scale);
        if (cache.size() > 16)
            cache.clear();
        cache.insert(cacheKey, result);
        return result;
    }

private:
    QByteArray svg;
    QSvgRenderer renderer;
    QHash<QString, QPixmap> cache;
};

} // namespace

QIcon vectorIcon(const QString &name, const QString &color) {
    static QHash<QString, QIcon> icons;
    const QString key = name + QLatin1Char('|') + color;
    auto found = icons.constFind(key);
    if (found != icons.constEnd())
        return *found;
    QString body = paths().value(name, paths().value(QStringLiteral("video")));
    body.replace(QStringLiteral("{c}"), color);
    const QString svg = QStringLiteral(
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"24\" height=\"24\" viewBox=\"0 0 24 24\">"
        "<g stroke=\"%1\" stroke-width=\"1.7\" fill=\"none\" stroke-linecap=\"round\" stroke-linejoin=\"round\">%2</g></svg>")
        .arg(color, body);
    QIcon result(new VectorIconEngine(svg.toUtf8()));
    icons.insert(key, result);
    return result;
}

QIcon applicationIcon() {
    static const QIcon result(QStringLiteral(":/fero.svg"));
    return result;
}
