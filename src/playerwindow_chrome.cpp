// Window chrome: toolbar and controls docked in the layout, floating above the
// video in a normal window, or as a rounded panel in fullscreen.
#include "i18n.h"
#include "playerwindow.h"
#include "icons.h"
#include "widgets.h"

#include <QApplication>
#include <QComboBox>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>
#ifdef Q_OS_WIN
#include <dwmapi.h>
#endif

namespace {
constexpr int ChromeHideMs = 2600;
constexpr int EdgeZone = 24;
const Qt::WindowFlags OverlayFlags = Qt::Tool | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint |
                                     Qt::WindowDoesNotAcceptFocus;
} // namespace

PlayerWindow::Chrome PlayerWindow::desiredChrome() const {
    if (isFullScreen())
        return Chrome::Fullscreen;
    if (currentPage == QLatin1String("player") && !current.isEmpty() && stage->currentWidget() == surface)
        return Chrome::Window;
    return Chrome::Docked;
}

bool PlayerWindow::sessionActive() const {
    return !current.isEmpty() || pendingPlay.has_value();
}

void PlayerWindow::setOverlay(QWidget *widget, bool overlay) {
    if (overlay == widget->isWindow())
        return;
    widget->hide();
    if (overlay) {
        (widget == topbar ? contentLayout : rootLayout)->removeWidget(widget);
        widget->setParent(this, OverlayFlags);
        widget->setAttribute(Qt::WA_ShowWithoutActivating, true);
    } else {
        widget->setAttribute(Qt::WA_ShowWithoutActivating, false);
        if (widget == topbar) {
            widget->setParent(content, Qt::Widget);
            contentLayout->insertWidget(0, widget);
        } else {
            widget->setParent(centralWidget(), Qt::Widget);
            rootLayout->addWidget(widget);
        }
    }
}

void PlayerWindow::applyChrome(Chrome mode) {
    if (mode == chromeMode)
        return;
    chromeMode = mode;
    chromeTimer.stop();
    const bool overlay = mode != Chrome::Docked;
    setOverlay(topbar, overlay);
    setOverlay(controls, overlay);
    controls->setFloating(mode == Chrome::Fullscreen);
    setChromeCursorHidden(false);
    if (overlay) {
        notification->hide();
        if (mode == Chrome::Fullscreen)
            topbar->hide();
        rootLayout->activate();
        positionOverlays();
    } else {
        topbar->show();
        controls->setVisible(sessionActive());
        if (noticeClock.isValid() && noticeClock.elapsed() < noticeMs) {
            notification->setText(noticeText);
            notification->show();
        }
        updateVolumeVisibility();
    }
}

void PlayerWindow::syncChrome() {
    if (!controls)
        return;
    applyChrome(desiredChrome());
    updateSkipButton();
    if (chromeMode != Chrome::Docked)
        positionOverlays();
}

QRect PlayerWindow::overlayArea() const {
    const QRect area = centralWidget()->rect();
    return {centralWidget()->mapToGlobal(area.topLeft()), area.size()};
}

void PlayerWindow::positionOverlays() {
    updateTransitionCover();
    updateSkipButton();
    if (chromeMode == Chrome::Docked)
        return;
    const QRect area = overlayArea();
    const int height = controls->sizeHint().height();
    if (chromeMode == Chrome::Fullscreen) {
        const int width = qMin(1160, area.width() - 40);
        controls->setGeometry(area.x() + (area.width() - width) / 2, area.y() + area.height() - height - 20, width, height);
    } else {
        topbar->setGeometry(area.x(), area.y(), area.width(), topbar->height());
        controls->setGeometry(area.x(), area.y() + area.height() - height, area.width(), height);
    }
    updateVolumeVisibility();
}

void PlayerWindow::updateVolumeVisibility() {
    if (chromeMode == Chrome::Fullscreen)
        volumeSlider->setVisible(qMin(1160, centralWidget()->width() - 40) >= 1000);
    else
        volumeSlider->setVisible(width() >= 1050);
}

void PlayerWindow::revealControls() {
    if (chromeMode == Chrome::Docked)
        controls->show();
    else
        showChrome(false, true);
}

bool PlayerWindow::playerHasFocus() const {
    QWidget *active = QApplication::activeWindow();
    return active == this || active == controls || active == topbar;
}

void PlayerWindow::setChromeCursorHidden(bool hidden) {
    if (hidden) {
        surface->setCursor(Qt::BlankCursor);
        audioStage->setCursor(Qt::BlankCursor);
    } else {
        surface->unsetCursor();
        audioStage->unsetCursor();
    }
    if (engine)
        engine->setCursorHidden(hidden);
}

void PlayerWindow::showChrome(bool top, bool bottom) {
    if (chromeMode == Chrome::Docked)
        return;
    positionOverlays();
    if (bottom) {
        controls->show();
        controls->raise();
    }
    if (top && chromeMode == Chrome::Window) {
        topbar->show();
        topbar->raise();
    }
    setChromeCursorHidden(false);
    chromeTimer.start(ChromeHideMs);
}

bool PlayerWindow::pointerOver(QWidget *widget) const {
    return widget->isVisible() && widget->frameGeometry().contains(QCursor::pos());
}

void PlayerWindow::hideChrome() {
    if (chromeMode == Chrome::Docked)
        return;
    if (!engine || engine->state() != MpvPlayer::State::Playing)
        return;   // paused or stopped: panels stay until playback resumes
    if (QApplication::activePopupWidget() || QApplication::activeModalWidget() || seekSlider->isSliderDown() ||
        volumeSlider->isSliderDown() || pointerOver(controls) || pointerOver(topbar) ||
        (chromeMode == Chrome::Fullscreen && !playerHasFocus())) {
        chromeTimer.start(ChromeHideMs);
        return;
    }
    controls->hide();
    topbar->hide();
    setChromeCursorHidden(true);
}

void PlayerWindow::pointerMoved() {
    // One physical move can reach us several times (widget chain, engine).
    const QPoint position = QCursor::pos();
    if (position == lastPointer || chromeMode == Chrome::Docked || !playerHasFocus())
        return;
    lastPointer = position;
    if (chromeMode == Chrome::Fullscreen) {
        showChrome(false, true);
        return;
    }
    const QRect area = overlayArea();
    if (!area.contains(position))
        return;
    const int y = position.y() - area.y();
    const bool nearTop = y <= topbar->height() + EdgeZone;
    const bool nearBottom = y >= area.height() - controls->sizeHint().height() - EdgeZone;
    if (nearTop || nearBottom) {
        showChrome(nearTop, nearBottom);
    } else {
        // Only the pointer returns; it hides again together with the panels.
        setChromeCursorHidden(false);
        if (!chromeTimer.isActive())
            chromeTimer.start(ChromeHideMs);
    }
}

// ---- Seamless resize ---------------------------------------------------------------
// While the window changes size (fullscreen on/off) Windows briefly stretches
// the last frame of the old size. A still copy of the current frame, placed
// exactly like the video, covers that moment until the engine renders at the
// new size.

void PlayerWindow::beginTransition() {
    if (!engine || current.isEmpty() || currentIsAudio || stage->currentWidget() != surface ||
        currentPage != QLatin1String("player"))
        return;
    double aspect = 0;
    const QImage frame = engine->currentFrame(&aspect);
    if (frame.isNull())
        return;
    if (!cover) {
        cover = new TransitionCover(this);
        coverTimer.setSingleShot(true);
        connect(&coverTimer, &QTimer::timeout, cover, &QWidget::hide);
    }
    cover->setFrame(frame, aspect);
    cover->setGeometry(QRect(surface->mapToGlobal(QPoint(0, 0)), surface->size()));
    cover->show();
    cover->raise();
    cover->repaint();
#ifdef Q_OS_WIN
    DwmFlush();   // the cover is on screen before the window starts to change
#endif
    coverTimer.start(500);   // safety net
}

void PlayerWindow::updateTransitionCover() {
    if (!cover || !cover->isVisible())
        return;
    cover->setGeometry(QRect(surface->mapToGlobal(QPoint(0, 0)), surface->size()));
    cover->repaint();
    cover->raise();
    if (controls->isWindow() && controls->isVisible())
        controls->raise();
    if (topbar->isWindow() && topbar->isVisible())
        topbar->raise();
}

void PlayerWindow::transitionOutputResized(int width, int height) {
    if (!cover || !cover->isVisible())
        return;
    const qreal ratio = surface->devicePixelRatioF();
    const QSize expected(qRound(surface->width() * ratio), qRound(surface->height() * ratio));
    if (qAbs(width - expected.width()) <= 2 && qAbs(height - expected.height()) <= 2)
        coverTimer.start(40);   // one more frame so the new size is actually presented
}

// ---- Skip intro / recap / credits ---------------------------------------------------
// Only from chapters the release itself names (Intro, Opening, Recap, Credits…):
// nothing is guessed and nothing is skipped without a click.

namespace {
enum class Segment { None, Intro, Recap, Credits };

Segment segmentOf(const QString &title) {
    static const QRegularExpression intro(
        QStringLiteral("\\b(intro|opening|op\\d*|title sequence|main titles?)\\b|заставк|вступлени|опенинг"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression recap(
        QStringLiteral("\\b(recap|previously)\\b|ранее в|в предыдущ|пересказ"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression credits(
        QStringLiteral("\\b(credits|end credits|outro|ending|ed\\d*)\\b|титры|эндинг"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    if (intro.match(title).hasMatch())
        return Segment::Intro;
    if (recap.match(title).hasMatch())
        return Segment::Recap;
    if (credits.match(title).hasMatch())
        return Segment::Credits;
    return Segment::None;
}
} // namespace

void PlayerWindow::chapterChanged(int index) {
    if (!skipSegmentButton) {
        skipSegmentButton = new QPushButton(this);
        skipSegmentButton->setObjectName(QStringLiteral("skipSegment"));
        skipSegmentButton->setWindowFlags(OverlayFlags);
        skipSegmentButton->setAttribute(Qt::WA_ShowWithoutActivating);
        skipSegmentButton->setAttribute(Qt::WA_TranslucentBackground);   // rounded corners over the video
        skipSegmentButton->setCursor(Qt::PointingHandCursor);
        connect(skipSegmentButton, &QPushButton::clicked, this, &PlayerWindow::skipSegment);
    }
    const Segment segment = engine && index >= 0 ? segmentOf(engine->chapterTitle(index)) : Segment::None;
    if (segment == Segment::None) {
        skipSegmentButton->setProperty("segment", false);
        skipSegmentButton->hide();
        return;
    }
    skipTarget = index + 1 < engine->chapterCount() ? engine->chapterStart(index + 1) : -1;
    skipSegmentButton->setText(segment == Segment::Intro   ? L("Пропустить заставку")
                               : segment == Segment::Recap ? L("Пропустить пересказ")
                                                           : L("Пропустить титры"));
    skipSegmentButton->adjustSize();
    skipSegmentButton->setProperty("segment", true);
    updateSkipButton();
}

void PlayerWindow::updateSkipButton() {
    if (!skipSegmentButton)
        return;
    const bool wanted = skipSegmentButton->property("segment").toBool() && chromeMode != Chrome::Docked &&
                        !current.isEmpty() && !isMinimized() && playerHasFocus() &&
                        (skipTarget >= 0 || nextQueueIndex(currentIndex, catalog->queue.size()));
    if (!wanted) {
        skipSegmentButton->hide();
        return;
    }
    const QRect area = overlayArea();
    const int bottom = chromeMode == Chrome::Fullscreen ? 20 + controls->sizeHint().height() + 16
                                                        : controls->sizeHint().height() + 20;
    const QSize size = skipSegmentButton->sizeHint();
    skipSegmentButton->setGeometry(area.right() - size.width() - 28, area.bottom() - bottom - size.height(),
                                   size.width(), size.height());
    skipSegmentButton->show();
    skipSegmentButton->raise();
}

void PlayerWindow::skipSegment() {
    if (!engine)
        return;
    skipSegmentButton->setProperty("segment", false);
    skipSegmentButton->hide();
    if (skipTarget >= 0)
        engine->seek(skipTarget);
    else
        next();   // credits at the end: straight to the next episode
}

void PlayerWindow::toggleFullscreen() {
    beginTransition();
    if (isFullScreen()) {
        chromeTimer.stop();
        if (wasMaximized)
            showMaximized();
        else
            showNormal();
        if (!normalGeometry.isEmpty() && !wasMaximized)
            restoreGeometry(normalGeometry);
        fullscreenButton->setIcon(vectorIcon(QStringLiteral("fullscreen"), palette.text));
        fullscreenButton->setProperty("iconName", QStringLiteral("fullscreen"));
        syncChrome();
        showChrome();
    } else {
        if (current.isEmpty()) {
            notify(L("Сначала откройте видео."));
            return;
        }
        wasMaximized = isMaximized();
        normalGeometry = saveGeometry();
        navigate(QStringLiteral("player"));
        showFullScreen();
        syncChrome();
        fullscreenButton->setIcon(vectorIcon(QStringLiteral("restore"), palette.text));
        fullscreenButton->setProperty("iconName", QStringLiteral("restore"));
        showChrome(false, true);
    }
}

void PlayerWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    updateTransitionCover();
    if (!controls)
        return;
    if (chromeMode == Chrome::Docked)
        updateVolumeVisibility();
    else
        positionOverlays();
}

void PlayerWindow::moveEvent(QMoveEvent *event) {
    QMainWindow::moveEvent(event);
    if (controls && chromeMode != Chrome::Docked)
        positionOverlays();
}

void PlayerWindow::changeEvent(QEvent *event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::ActivationChange && isActiveWindow())
        keepAudioAwake();   // the user is back: have the audio device ready
    if (event->type() == QEvent::WindowStateChange && controls)
        QTimer::singleShot(0, this, [this] { syncChrome(); });
}

bool PlayerWindow::eventFilter(QObject *watched, QEvent *event) {
    if (isClosing)
        return QMainWindow::eventFilter(watched, event);
    const QEvent::Type type = event->type();
    if (watched == surface && (type == QEvent::Resize || type == QEvent::Move))
        updateTransitionCover();
    if (type == QEvent::MouseMove) {
        if (chromeMode != Chrome::Docked)
            pointerMoved();
        return false;
    }
    if (type == QEvent::ApplicationDeactivate && isFullScreen())
        controls->hide();
    if (type != QEvent::KeyPress || !playerHasFocus())
        return false;
    auto *key = static_cast<QKeyEvent *>(event);
    QWidget *focus = QApplication::focusWidget();
    const bool typing = qobject_cast<QLineEdit *>(focus) || qobject_cast<QSpinBox *>(focus) ||
                        qobject_cast<QComboBox *>(focus);
    const int code = key->key();
    const Qt::KeyboardModifiers modifiers = key->modifiers();
    if (code == Qt::Key_Escape && isFullScreen()) {
        toggleFullscreen();
        return true;
    }
    if (code == Qt::Key_F11) {
        toggleFullscreen();
        return true;
    }
    if (typing)
        return false;
    if (code == Qt::Key_Space || code == Qt::Key_MediaTogglePlayPause || code == Qt::Key_MediaPlay ||
        code == Qt::Key_MediaPause) {
        if (!key->isAutoRepeat())
            playPause();
        return true;
    }
    if (code == Qt::Key_MediaNext) {
        next();
        return true;
    }
    if (code == Qt::Key_MediaPrevious) {
        previous();
        return true;
    }
    if (code == Qt::Key_Left || code == Qt::Key_Right) {
        if (modifiers & Qt::ControlModifier) {
            code == Qt::Key_Left ? previous() : next();
        } else {
            const qint64 amount = (modifiers & Qt::ShiftModifier) ? 60000 : 10000;
            seekRelative(code == Qt::Key_Left ? -amount : amount);
        }
        return true;
    }
    if (code == Qt::Key_Up || code == Qt::Key_Down) {
        volumeSlider->setValue(volumeSlider->value() + (code == Qt::Key_Up ? 5 : -5));
        showOsd(L("Громкость: %1%").arg(volumeSlider->value()));
        return true;
    }
    // Letters and digits by physical key, so they also work in other keyboard layouts.
    const quint32 vk = key->nativeVirtualKey();
    int letter = 0, digit = -1;
    if (vk >= 'A' && vk <= 'Z')
        letter = int(vk);
    else if (code >= Qt::Key_A && code <= Qt::Key_Z)
        letter = 'A' + (code - Qt::Key_A);
    if (vk >= '0' && vk <= '9')
        digit = int(vk - '0');
    else if (vk >= 0x60 && vk <= 0x69)   // numeric keypad
        digit = int(vk - 0x60);
    else if (code >= Qt::Key_0 && code <= Qt::Key_9)
        digit = code - Qt::Key_0;
    // A / S, optionally followed by a digit: A2 = second audio track, S0 = subtitles off.
    if (digit >= 0 && trackKey && trackKeyTimer.isActive()) {
        trackKeyTimer.stop();
        const bool audio = trackKey == 'A';
        trackKey = 0;
        trackHotkey(audio, digit);
        return true;
    }
    if (modifiers == Qt::NoModifier || modifiers == Qt::KeypadModifier) {
        if ((letter == 'A' || letter == 'S') && !key->isAutoRepeat()) {
            trackKey = letter;
            trackKeyTimer.start(450);   // no digit in time: switch to the next track
            return true;
        }
        if (letter == 'F') {
            toggleFullscreen();
            return true;
        }
        if (letter == 'M') {
            toggleMute();
            showOsd(isMuted ? L("Звук выключен") : L("Звук включён"));
            return true;
        }
        if (letter == 'E') {
            nextFrame();
            return true;
        }
    }
    return false;
}

// ---- Track hot keys --------------------------------------------------------------------
// number: -1 = next track (subtitles also pass through "off"), 0 = subtitles off,
// 1..9 = that track.

void PlayerWindow::trackHotkey(bool audio, int number) {
    if (!engine || current.isEmpty())
        return;
    QVector<MediaTrack> tracks;
    for (const MediaTrack &track : engine->tracks(audio))
        if (track.id > 0)
            tracks << track;
    if (tracks.isEmpty()) {
        showOsd(audio ? L("Нет аудиодорожек") : L("Нет субтитров"));
        return;
    }
    const int selected = engine->currentTrack(audio);
    int index = -1;   // position in `tracks`, -1 = off
    for (int i = 0; i < tracks.size(); ++i)
        if (tracks[i].id == selected)
            index = i;
    int target;
    if (number < 0) {
        target = index + 1;
        if (target >= tracks.size())
            target = audio ? 0 : -1;   // subtitles: last → off → first
    } else if (number == 0) {
        if (audio)
            return;   // audio is never switched off by a hot key
        target = -1;
    } else if (number > tracks.size()) {
        showOsd(L("Дорожки %1 нет").arg(number));
        return;
    } else {
        target = number - 1;
    }
    if (target < 0) {
        changeTrack(false, -1);
        showOsd(L("Субтитры выключены"));
        return;
    }
    changeTrack(audio, tracks[target].id);
    const QString label = tracks[target].label.isEmpty() ? tracks[target].title : tracks[target].label;
    showOsd((audio ? L("Аудио %1/%2: %3") : L("Субтитры %1/%2: %3"))
                .arg(target + 1).arg(tracks.size()).arg(label));
}

void PlayerWindow::showOsd(const QString &text) {
    if (engine && !current.isEmpty() && !currentIsAudio)
        engine->showText(text, 1800);
    else
        notify(text, 2);
}
