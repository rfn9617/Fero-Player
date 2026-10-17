#pragma once
#include <QFrame>
#include <QImage>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <QToolButton>
#include <functional>

QLabel *makeLabel(const QString &text, const QString &name = QString(), int pixelSize = 0);
QPushButton *makeButton(const QString &text, const QString &name, const QString &icon,
                        std::function<void()> callback);
QToolButton *makeTool(const QString &icon, const QString &tooltip, std::function<void()> callback, int size = 36);

class PlaybackButton : public QToolButton {
    Q_OBJECT
public:
    using QToolButton::QToolButton;
    QString symbol = QStringLiteral("play");
    QString accent = QStringLiteral("#ffb15b"), hover = QStringLiteral("#ffc27c"),
            foreground = QStringLiteral("#24180c"), disabled = QStringLiteral("#2c3340");
protected:
    void paintEvent(QPaintEvent *) override;
};

// Variant B toolbar tab: text only, accent underline under the active tab.
class NavTab : public QPushButton {
    Q_OBJECT
public:
    NavTab(const QString &text, QWidget *parent = nullptr);
    void setColors(const QString &text, const QString &muted, const QString &accent);
    QSize sizeHint() const override;
protected:
    void paintEvent(QPaintEvent *) override;
private:
    QString textColor = QStringLiteral("#f5f6f8"), mutedColor = QStringLiteral("#8f98a8"),
            accentColor = QStringLiteral("#ffb15b");
};

class BrandMark : public QWidget {
    Q_OBJECT
public:
    explicit BrandMark(QWidget *parent = nullptr);
    void setTextColor(const QString &color);
    QSize sizeHint() const override { return {104, 40}; }
protected:
    void paintEvent(QPaintEvent *) override;
private:
    QString textColor = QStringLiteral("#f5f6f8");
};

class ControlsPanel : public QFrame {
    Q_OBJECT
public:
    ControlsPanel();
    void setFloating(bool floating);
    bool floating() const { return isFloating; }
protected:
    void resizeEvent(QResizeEvent *event) override;
private:
    void updateMask();
    bool isFloating = false;
};

class SmoothSlider : public QSlider {
    Q_OBJECT
public:
    explicit SmoothSlider(QWidget *parent = nullptr);
    QString accent = QStringLiteral("#ff982f"), rail = QStringLiteral("#46464b");
protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *) override { update(); }
    void leaveEvent(QEvent *) override { update(); }
private:
    int valueAt(const QPointF &point) const;
};

class SeekSlider : public SmoothSlider {
    Q_OBJECT
public:
    explicit SeekSlider(QWidget *parent = nullptr);
signals:
    void committed(int value);
};

// Native window that hosts libmpv's video output.
class VideoSurface : public QFrame {
    Q_OBJECT
public:
    VideoSurface();
    void mouseFromEngine(const QString &event);   // events forwarded by libmpv
signals:
    void clicked();
    void doubleClicked();
    void contextRequested();
    void wheel(int steps);
protected:
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
private:
    QTimer clickTimer;
};

// Still frame shown over the video while the window changes size, so the
// stretched old frame of the resizing swap chain is never visible.
class TransitionCover : public QWidget {
public:
    explicit TransitionCover(QWidget *owner);
    void setFrame(const QImage &frame, double aspect);
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    QImage frame;
    double aspect = 0;
};

class AudioStage : public QWidget {
    Q_OBJECT
public:
    AudioStage();
    QLabel *title;
};
