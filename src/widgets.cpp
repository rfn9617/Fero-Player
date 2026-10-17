#include "i18n.h"
#include "widgets.h"
#include "icons.h"

#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRegion>
#include <QStyle>
#include <QSvgRenderer>
#include <QVBoxLayout>
#include <QWheelEvent>

QLabel *makeLabel(const QString &text, const QString &name, int pixelSize) {
    auto *label = new QLabel(text);
    if (!name.isEmpty())
        label->setObjectName(name);
    if (pixelSize > 0) {
        QFont font = label->font();
        font.setPixelSize(pixelSize);
        label->setFont(font);
        label->setStyleSheet(QStringLiteral("font-size: %1px;").arg(pixelSize));
    }
    return label;
}

QPushButton *makeButton(const QString &text, const QString &name, const QString &icon,
                        std::function<void()> callback) {
    auto *button = new QPushButton(text);
    if (!name.isEmpty())
        button->setObjectName(name);
    if (!icon.isEmpty()) {
        button->setIcon(vectorIcon(icon));
        button->setIconSize(QSize(18, 18));
    }
    QObject::connect(button, &QPushButton::clicked, button, [callback] { callback(); });
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

QToolButton *makeTool(const QString &icon, const QString &tooltip, std::function<void()> callback, int size) {
    QToolButton *button = icon == QLatin1String("play") ? new PlaybackButton : new QToolButton;
    button->setObjectName(QStringLiteral("iconButton"));
    button->setProperty("iconName", icon);
    button->setIcon(vectorIcon(icon));
    button->setIconSize(QSize(22, 22));
    button->setFixedSize(size, size);
    button->setToolTip(tooltip);
    button->setAccessibleName(tooltip.section(QStringLiteral(" ("), 0, 0));
    button->setCursor(Qt::PointingHandCursor);
    QObject::connect(button, &QToolButton::clicked, button, [callback] { callback(); });
    return button;
}

// ---- PlaybackButton -------------------------------------------------------------

void PlaybackButton::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QString color = accent;
    if (!isEnabled())
        color = disabled;
    else if (isDown() || underMouse())
        color = hover;
    const QRectF circle = QRectF(rect()).adjusted(1, 1, -1, -1);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(color));
    painter.drawEllipse(circle);
    painter.setBrush(QColor(foreground));
    const qreal cx = circle.center().x(), cy = circle.center().y();
    if (symbol == QLatin1String("pause")) {
        for (qreal x : {cx - 6, cx + 2})
            painter.drawRoundedRect(QRectF(x, cy - 8, 4, 16), 1, 1);
    } else {
        QPainterPath path;
        path.moveTo(cx - 4.5, cy - 8);
        path.lineTo(cx + 9, cy);
        path.lineTo(cx - 4.5, cy + 8);
        path.closeSubpath();
        painter.drawPath(path);
    }
    if (hasFocus()) {
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(hover), 1));
        painter.drawEllipse(QRectF(rect()).adjusted(.5, .5, -.5, -.5));
    }
}

// ---- NavTab -----------------------------------------------------------------------

NavTab::NavTab(const QString &text, QWidget *parent) : QPushButton(text, parent) {
    setObjectName(QStringLiteral("navTab"));
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::TabFocus);
    setAttribute(Qt::WA_Hover);
}

void NavTab::setColors(const QString &text, const QString &muted, const QString &accent) {
    textColor = text;
    mutedColor = muted;
    accentColor = accent;
    update();
}

QSize NavTab::sizeHint() const {
    QFont bold = font();
    bold.setPixelSize(14);
    bold.setWeight(QFont::DemiBold);   // widest state: no layout shift on selection
    return {QFontMetrics(bold).horizontalAdvance(text()) + 28, 64};
}

void NavTab::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QFont f = font();
    f.setPixelSize(14);
    f.setWeight(isChecked() ? QFont::DemiBold : QFont::Medium);
    painter.setFont(f);
    const bool hot = underMouse() || hasFocus();
    painter.setPen(QColor(isChecked() || hot ? textColor : mutedColor));
    const QRect area = rect().adjusted(14, 0, -14, 0);
    painter.drawText(area, Qt::AlignCenter, text());
    if (isChecked()) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(accentColor));
        const int width = QFontMetrics(f).horizontalAdvance(text()) + 8;
        QPainterPath bar;
        const QRectF r((this->width() - width) / 2.0, height() - 3.0, width, 4.0);
        bar.addRoundedRect(r, 2, 2);
        painter.drawPath(bar);
    }
}

// ---- BrandMark -------------------------------------------------------------------

BrandMark::BrandMark(QWidget *parent) : QWidget(parent) {
    setFixedSize(sizeHint());
}

void BrandMark::setTextColor(const QString &color) {
    textColor = color;
    update();
}

void BrandMark::paintEvent(QPaintEvent *) {
    static QSvgRenderer renderer(QStringLiteral(":/fero.svg"));
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const int mark = 26;
    renderer.render(&painter, QRectF(0, (height() - mark) / 2.0, mark, mark));
    QFont f = font();
    f.setPixelSize(17);
    f.setWeight(QFont::DemiBold);
    painter.setFont(f);
    painter.setPen(QColor(textColor));
    painter.drawText(QRect(mark + 10, 0, width() - mark - 10, height()), Qt::AlignVCenter | Qt::AlignLeft,
                     QStringLiteral("Fero"));
}

// ---- ControlsPanel ---------------------------------------------------------------

ControlsPanel::ControlsPanel() {
    setObjectName(QStringLiteral("controls"));
    setAttribute(Qt::WA_DontCreateNativeAncestors);
    setMouseTracking(true);
}

void ControlsPanel::setFloating(bool floating) {
    isFloating = floating;
    setProperty("floating", floating);
    style()->unpolish(this);
    style()->polish(this);
    updateMask();
}

void ControlsPanel::updateMask() {
    if (isFloating) {
        QPainterPath path;
        path.addRoundedRect(QRectF(rect()), 18, 18);
        setMask(QRegion(path.toFillPolygon().toPolygon()));
    } else {
        clearMask();
    }
}

void ControlsPanel::resizeEvent(QResizeEvent *event) {
    QFrame::resizeEvent(event);
    updateMask();
}

// ---- Sliders --------------------------------------------------------------------

SmoothSlider::SmoothSlider(QWidget *parent) : QSlider(Qt::Horizontal, parent) {
    setAttribute(Qt::WA_Hover);
    setCursor(Qt::PointingHandCursor);
}

void SmoothSlider::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QColor accentColor(accent), railColor(rail);
    if (!isEnabled())
        accentColor = railColor;
    const qreal width = qMax(1, this->width() - 12);
    const qreal fraction = qreal(value() - minimum()) / qMax(1, maximum() - minimum());
    const qreal center = height() / 2.0;
    painter.setPen(Qt::NoPen);
    painter.setBrush(railColor);
    painter.drawRoundedRect(QRectF(6, center - 2, width, 4), 2, 2);
    painter.setBrush(accentColor);
    if (fraction > 0)
        painter.drawRoundedRect(QRectF(6, center - 2, width * fraction, 4), 2, 2);
    if (isEnabled()) {
        const qreal radius = underMouse() || isSliderDown() ? 6 : 4.5;
        painter.drawEllipse(QRectF(6 + width * fraction - radius, center - radius, radius * 2, radius * 2));
    }
}

int SmoothSlider::valueAt(const QPointF &point) const {
    const qreal width = qMax(1, this->width() - 12);
    const qreal fraction = qBound(0.0, (point.x() - 6) / width, 1.0);
    return qRound(minimum() + fraction * (maximum() - minimum()));
}

void SmoothSlider::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && isEnabled()) {
        setSliderDown(true);
        setValue(valueAt(event->position()));
        event->accept();
    } else {
        QSlider::mousePressEvent(event);
    }
}

void SmoothSlider::mouseMoveEvent(QMouseEvent *event) {
    if (isSliderDown()) {
        setValue(valueAt(event->position()));
        event->accept();
    } else {
        QSlider::mouseMoveEvent(event);
    }
}

void SmoothSlider::mouseReleaseEvent(QMouseEvent *event) {
    if (isSliderDown() && event->button() == Qt::LeftButton) {
        setValue(valueAt(event->position()));
        setSliderDown(false);
        event->accept();
    } else {
        QSlider::mouseReleaseEvent(event);
    }
}

SeekSlider::SeekSlider(QWidget *parent) : SmoothSlider(parent) {
    setRange(0, 10000);
    setTracking(true);
    setFixedHeight(20);
    setAccessibleName(L("Позиция воспроизведения"));
    connect(this, &QSlider::sliderReleased, this, [this] { emit committed(value()); });
}

// ---- VideoSurface ----------------------------------------------------------------

VideoSurface::VideoSurface() {
    setStyleSheet(QStringLiteral("background: #000000; border: none;"));
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_DontCreateNativeAncestors);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    clickTimer.setSingleShot(true);
    clickTimer.setInterval(220);
    connect(&clickTimer, &QTimer::timeout, this, &VideoSurface::clicked);
}

void VideoSurface::mouseFromEngine(const QString &event) {
    if (event == QLatin1String("click")) {
        clickTimer.start();
    } else if (event == QLatin1String("double")) {
        clickTimer.stop();
        emit doubleClicked();
    } else if (event == QLatin1String("right")) {
        emit contextRequested();
    } else if (event == QLatin1String("wheel-up")) {
        emit wheel(1);
    } else if (event == QLatin1String("wheel-down")) {
        emit wheel(-1);
    }
}

void VideoSurface::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton)
        clickTimer.start();
    QFrame::mouseReleaseEvent(event);
}

void VideoSurface::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        clickTimer.stop();
        emit doubleClicked();
        event->accept();
    }
}

void VideoSurface::contextMenuEvent(QContextMenuEvent *event) {
    emit contextRequested();
    event->accept();
}

void VideoSurface::wheelEvent(QWheelEvent *event) {
    const int steps = event->angleDelta().y() / 120;
    if (steps)
        emit wheel(steps);
    event->accept();
}

// ---- AudioStage ------------------------------------------------------------------

AudioStage::AudioStage() {
    setAttribute(Qt::WA_StyledBackground);
    setStyleSheet(QStringLiteral("background: #151517;"));
    auto *layout = new QVBoxLayout(this);
    layout->addStretch();
    auto *art = new QLabel;
    art->setAlignment(Qt::AlignCenter);
    art->setPixmap(vectorIcon(QStringLiteral("audio"), QStringLiteral("#ff982f")).pixmap(100, 100));
    layout->addWidget(art);
    layout->addSpacing(24);
    title = makeLabel(QString(), QString(), 26);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QStringLiteral("color: #f4f4f5; font-size: 26px; font-weight: 600; background: transparent;"));
    title->setWordWrap(true);
    layout->addWidget(title);
    auto *detail = makeLabel(L("Аудио"));
    detail->setStyleSheet(QStringLiteral("color: #a2a2ab; background: transparent;"));
    detail->setAlignment(Qt::AlignCenter);
    layout->addWidget(detail);
    layout->addStretch();
}

TransitionCover::TransitionCover(QWidget *owner) : QWidget(owner) {
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint |
                   Qt::WindowDoesNotAcceptFocus | Qt::WindowTransparentForInput);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_TransparentForMouseEvents);
}

void TransitionCover::setFrame(const QImage &image, double displayAspect) {
    frame = image;
    aspect = displayAspect > 0 ? displayAspect
                               : (image.height() > 0 ? double(image.width()) / image.height() : 0);
    update();
}

void TransitionCover::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);
    if (frame.isNull() || aspect <= 0)
        return;
    // Same placement as the engine: centred, letterboxed, display aspect.
    QSizeF size(width(), width() / aspect);
    if (size.height() > height())
        size = QSizeF(height() * aspect, height());
    const QRectF target((width() - size.width()) / 2, (height() - size.height()) / 2, size.width(), size.height());
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(target, frame);
}
