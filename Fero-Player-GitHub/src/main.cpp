#include "catalog.h"
#include "i18n.h"
#include "icons.h"
#include "mpvplayer.h"
#include "playerwindow.h"
#include "selftest.h"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QProxyStyle>
#include <QStyleFactory>
#include <QTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
// Tooltips appear only after the pointer rests on a control for a moment,
// and every control waits again (no instant tooltips while moving around).
class FeroStyle : public QProxyStyle {
public:
    using QProxyStyle::QProxyStyle;
    int styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget,
                  QStyleHintReturn *data) const override {
        if (hint == SH_ToolTip_WakeUpDelay)
            return 1100;
        if (hint == SH_ToolTip_FallAsleepDelay)
            return 0;
        return QProxyStyle::styleHint(hint, option, widget, data);
    }
};
} // namespace

int main(int argc, char *argv[]) {
    const qint64 started = QDateTime::currentMSecsSinceEpoch();
#ifdef Q_OS_WIN
    // Taskbar grouping and jump lists use one stable identity.
    using SetId = HRESULT(WINAPI *)(PCWSTR);
    if (HMODULE shell = LoadLibraryW(L"shell32.dll"))
        if (auto setId = reinterpret_cast<SetId>(reinterpret_cast<void *>(
                GetProcAddress(shell, "SetCurrentProcessExplicitAppUserModelID"))))
            setId(L"FeroPlayer.Desktop");
#endif
    // Only the video surface needs a native window; its page-stack siblings stay alien.
    QApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Fero Player"));
    QApplication::setOrganizationName(QStringLiteral("FeroPlayer"));
    QApplication::setApplicationVersion(QStringLiteral(FERO_VERSION));
    QApplication::setStyle(new FeroStyle(QStyleFactory::create(QStringLiteral("Fusion"))));
    QApplication::setWindowIcon(applicationIcon());
    QFont font(QStringLiteral("Segoe UI"));
    font.setPixelSize(13);
    font.setHintingPreference(QFont::PreferNoHinting);
    QApplication::setFont(font);

    const QStringList arguments = QApplication::arguments();
    const int selfTest = arguments.indexOf(QStringLiteral("--self-test"));
    const bool testing = selfTest > 0 && selfTest + 2 < arguments.size();
    // The self-test keeps its own profile: the real history is never touched.
    Catalog catalog(testing ? QDir(arguments[selfTest + 2]).filePath(QStringLiteral("profile")) : QString());
    setLanguage(catalog.settings.language);
    if (testing && arguments.contains(QStringLiteral("--ru")))
        setLanguage(QStringLiteral("ru"));
    if (testing && arguments.contains(QStringLiteral("--balanced")))
        catalog.settings.quality = QStringLiteral("balanced");
    PlayerWindow window(&catalog);
    window.startupStarted = started;
    if (testing) {
        // Off-screen and never activated: the self-test does not disturb the user.
        window.setAttribute(Qt::WA_ShowWithoutActivating);
        window.move(-6000, 120);
    }
    for (int i = 0; i + 1 < arguments.size(); ++i)
        if (testing && arguments[i] == QLatin1String("--mpv"))
            window.engineExtraOptions << arguments[i + 1];
    window.show();
    window.startupMetrics.insert(QStringLiteral("window_shown_ms"), double(QDateTime::currentMSecsSinceEpoch() - started));
    QStringList files;
    if (!testing)
        for (int i = 1; i < arguments.size(); ++i)
            if (QFileInfo(arguments[i]).isFile())
                files << arguments[i];
    if (!files.isEmpty()) {
        // Opened with a file: start the engine and switch to the video page
        // before the first paint, so the library never flashes up.
        window.beginEngineLoad();
        window.openPaths(files);
    } else {
        QTimer::singleShot(0, &window, [&window] { window.beginEngineLoad(); });
    }

    if (testing) {
        auto *test = new SelfTest(&window, arguments[selfTest + 1], arguments[selfTest + 2]);
        test->start();
        return app.exec();
    }
    return app.exec();
}
