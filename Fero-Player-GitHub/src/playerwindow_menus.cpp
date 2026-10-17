// Menus and dialogs: tracks, subtitles, speed, settings, help.
#include "i18n.h"
#include "playerwindow.h"
#include "audiowarmup.h"
#include "widgets.h"
#include "medialist.h"
#include "icons.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCursor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLineEdit>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

void PlayerWindow::mediaMenu(const QString &path, int queueIndex, const QPoint &position) {
    if (path.isEmpty())
        return;
    QMenu menu(this);
    menu.addAction(L("Воспроизвести"), this, [this, path, queueIndex] {
        queueIndex >= 0 ? playIndex(queueIndex) : playLibrary(path);
    });
    menu.addAction(L("Добавить в очередь"), this, [this, path] { enqueue({path}); });
    menu.addAction(L("Показать в папке"), this, [this, path] { revealFile(path); });
    menu.addSeparator();
    if (queueIndex >= 0)
        menu.addAction(L("Убрать из очереди"), this, [this, queueIndex] { removeQueueItem(queueIndex); });
    else
        menu.addAction(L("Убрать из медиатеки"), this, [this, path] { forgetFile(path); });
    menu.exec(position);
}

// ---- Tracks ------------------------------------------------------------------------------

void PlayerWindow::addTrackActions(QMenu *menu, bool audio) {
    const QVector<MediaTrack> tracks = engine->tracks(audio);
    if (tracks.size() <= 1) {
        menu->addAction(L("Дорожки не найдены"))->setEnabled(false);
    }
    for (const MediaTrack &track : tracks) {
        if (track.id < 0 && tracks.size() <= 1)
            continue;
        QAction *action = menu->addAction(track.label);
        action->setCheckable(true);
        action->setChecked(track.selected);
        const int id = track.id;
        connect(action, &QAction::triggered, this, [this, audio, id] { changeTrack(audio, id); });
    }
}

QMenu *PlayerWindow::audioMenu() {
    auto *menu = new QMenu(this);
    menu->addSection(L("Звуковая дорожка"));
    if (engine && !current.isEmpty())
        addTrackActions(menu, true);
    else
        menu->addAction(L("Сначала откройте файл"))->setEnabled(false);
    menu->addSeparator();
    menu->addAction(L("Синхронизация…"), this, [this] { showSyncDialog(); })
        ->setEnabled(engine && !current.isEmpty());
    return menu;
}

QMenu *PlayerWindow::subtitlesMenu() {
    auto *menu = new QMenu(this);
    menu->addSection(L("Субтитры"));
    if (engine && !current.isEmpty())
        addTrackActions(menu, false);
    else
        menu->addAction(L("Сначала откройте файл"))->setEnabled(false);
    menu->addSeparator();
    const bool active = engine && !current.isEmpty();
    menu->addAction(L("Загрузить файл субтитров…"), this, [this] { subtitleDialog(); })->setEnabled(active);
    menu->addAction(L("Синхронизация звука и субтитров…"), this, [this] { showSyncDialog(); })->setEnabled(active);
    return menu;
}

void PlayerWindow::showAudioMenu() {
    showChrome(false, true);
    QMenu *menu = audioMenu();
    menu->exec(audioButton->mapToGlobal(QPoint(0, 0)) - QPoint(0, menu->sizeHint().height() + 6));
    menu->deleteLater();
}

void PlayerWindow::showSubtitlesMenu() {
    showChrome(false, true);
    QMenu *menu = subtitlesMenu();
    menu->exec(ccButton->mapToGlobal(QPoint(0, 0)) - QPoint(0, menu->sizeHint().height() + 6));
    menu->deleteLater();
}

void PlayerWindow::changeTrack(bool audio, int id) {
    if (!engine || !engine->selectTrack(audio, id)) {
        notify(L("Не удалось переключить дорожку."));
        return;
    }
    (audio ? pendingTracks.audio : pendingTracks.subtitle).reset();
    TrackChoice choice;
    choice.id = id;
    for (const MediaTrack &track : engine->tracks(audio))
        if (track.id == id) {
            choice.name = track.title;
            choice.lang = track.lang;
            if (track.external)
                choice.external = track.externalFile;
        }
    if (!audio && id < 0) {
        // Keep a loaded external file remembered, but disabled.
        if (auto saved = catalog->trackPreferences(current).subtitle)
            choice.external = saved->external;
    }
    catalog->rememberTrack(current, audio, choice);
    catalog->rememberSeriesTrack(current, audio, choice);
    persist();
}

void PlayerWindow::subtitleDialog() {
    const QString path = QFileDialog::getOpenFileName(
        this, L("Открыть субтитры"), QFileInfo(current).absolutePath(),
        L("Субтитры (*.srt *.ass *.ssa *.vtt *.sub *.idx *.sup *.smi);;Все файлы (*)"));
    if (!path.isEmpty())
        loadSubtitle(path);
}

void PlayerWindow::loadSubtitle(const QString &path) {
    if (engineLoading && pendingPlay) {
        pendingSubtitles << path;
        return;
    }
    if (current.isEmpty() || !engine) {
        notify(L("Сначала откройте видео, затем добавьте субтитры."));
        return;
    }
    const QFileInfo info(path);
    if (!info.isFile()) {
        notify(L("Файл субтитров не найден."));
        return;
    }
    engine->addSubtitle(info.absoluteFilePath(), true);
    pendingTracks.subtitle.reset();
    TrackChoice choice;
    choice.id = 0;   // "the external file is selected"; resolved again on the next start
    choice.name = info.fileName();
    choice.external = QDir::toNativeSeparators(info.absoluteFilePath());
    catalog->rememberTrack(current, false, choice);
    persist();
    notify(L("Субтитры загружены: ") + info.fileName());
}

void PlayerWindow::showSyncDialog() {
    if (!engine || current.isEmpty())
        return;
    QDialog dialog(this);
    dialog.setWindowTitle(L("Синхронизация"));
    dialog.setMinimumWidth(410);
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(24, 24, 24, 24);
    QLabel *help = makeLabel(L("Положительное значение задерживает дорожку.\nНастройки действуют для текущего файла."),
                             QStringLiteral("muted"));
    help->setWordWrap(true);
    layout->addWidget(help);
    auto *form = new QFormLayout;
    auto *audio = new QSpinBox;
    auto *subtitles = new QSpinBox;
    for (QSpinBox *box : {audio, subtitles}) {
        box->setRange(-60000, 60000);
        box->setSingleStep(50);
        box->setSuffix(L(" мс"));
    }
    audio->setValue(engine->audioDelay());
    subtitles->setValue(engine->subtitleDelay());
    form->addRow(L("Звук"), audio);
    form->addRow(L("Субтитры"), subtitles);
    layout->addLayout(form);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(L("Применить"));
    buttons->button(QDialogButtonBox::Cancel)->setText(L("Отмена"));
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() == QDialog::Accepted && engine) {
        engine->setAudioDelay(audio->value());
        engine->setSubtitleDelay(subtitles->value());
    }
}

// ---- Other menus ----------------------------------------------------------------------------

void PlayerWindow::showRateMenu() {
    QMenu menu(this);
    for (double value : {0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0}) {
        QAction *action = menu.addAction(QStringLiteral("%1×").arg(value) +
                                         (value == 1.0 ? L(" · обычная") : QString()));
        action->setCheckable(true);
        action->setChecked(qFuzzyCompare(value, rateValue));
        connect(action, &QAction::triggered, this, [this, value] { setRate(value); });
    }
    menu.exec(rateButton->mapToGlobal(QPoint(0, 0)) - QPoint(0, menu.sizeHint().height() + 6));
}

void PlayerWindow::showFileMenu() {
    QMenu menu(this);
    menu.addAction(L("Добавить файлы в очередь…"), this, [this] { openDialog(true); });
    menu.addAction(L("Добавить папку…"), this, [this] { openFolderDialog(); });
    menu.addSeparator();
    menu.addAction(L("Настройки"), this, [this] { showSettings(); });
    menu.addAction(L("Горячие клавиши"), this, [this] { showHelp(); });
    menu.exec(topMore->mapToGlobal(topMore->rect().bottomLeft() + QPoint(0, 4)));
}

void PlayerWindow::showMoreMenu(std::optional<QPoint> position) {
    QMenu menu(this);
    const bool active = engine && !current.isEmpty();
    menu.addAction(L("Сохранить кадр…\tCtrl+S"), this, [this] { snapshotDialog(); })
        ->setEnabled(active && engine->hasVideo());
    QMenu *aspect = menu.addMenu(L("Соотношение сторон"));
    const std::pair<const char *, const char *> ratios[] = {{"Исходное", ""}, {"16:9", "16:9"}, {"4:3", "4:3"},
                                                            {"21:9", "21:9"}, {"2.35:1", "2.35"}};
    for (const auto &[title, value] : ratios) {
        const QString ratio = QString::fromLatin1(value);
        aspect->addAction(L(title), this, [this, ratio] {
            if (engine)
                engine->setAspect(ratio);
        });
    }
    aspect->setEnabled(active);
    menu.addAction(L("Следующий кадр\tE"), this, [this] { nextFrame(); })->setEnabled(active);
    QAction *top = menu.addAction(L("Поверх остальных окон"), this, [this] { toggleOnTop(); });
    top->setCheckable(true);
    top->setChecked(onTop);
    menu.addSeparator();
    menu.addAction(L("Далее"), this, [this] { navigate(QStringLiteral("queue")); });
    menu.addAction(L("Настройки"), this, [this] { showSettings(); });
    menu.addAction(L("Горячие клавиши"), this, [this] { showHelp(); });
    showChrome(false, true);
    if (position)
        menu.exec(*position);
    else
        menu.exec(moreButton->mapToGlobal(QPoint(0, 0)) - QPoint(menu.sizeHint().width() - moreButton->width(),
                                                                  menu.sizeHint().height() + 6));
}

void PlayerWindow::showSettings() {
    QDialog dialog(this);
    dialog.setWindowTitle(L("Настройки Fero Player"));
    dialog.setMinimumWidth(500);
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(28, 26, 28, 26);
    layout->setSpacing(16);
    layout->addWidget(makeLabel(L("Настройки"), QStringLiteral("pageTitle")));
    auto *form = new QFormLayout;
    form->setSpacing(14);
    auto *theme = new QComboBox;
    theme->addItem(L("Тёмная"), QStringLiteral("dark"));
    theme->addItem(L("Светлая"), QStringLiteral("light"));
    theme->setCurrentIndex(catalog->settings.theme == QLatin1String("light") ? 1 : 0);
    form->addRow(L("Оформление"), theme);
    auto *quality = new QComboBox;
    quality->addItem(L("Максимальное качество"), QStringLiteral("high"));
    quality->addItem(L("Экономия видеокарты"), QStringLiteral("balanced"));
    quality->setCurrentIndex(catalog->settings.quality == QLatin1String("balanced") ? 1 : 0);
    quality->setToolTip(L("Максимальное: масштабирование EWA Lanczos, дебандинг, точный HDR.\n"
                                       "Экономия: быстрые фильтры, меньше нагрузки на видеокарту."));
    form->addRow(L("Изображение"), quality);
    auto *language = new QComboBox;
    language->addItem(QStringLiteral("English"), QStringLiteral("en"));
    language->addItem(QStringLiteral("Русский"), QStringLiteral("ru"));
    language->setCurrentIndex(catalog->settings.language == QLatin1String("ru") ? 1 : 0);
    form->addRow(L("Язык"), language);
    layout->addLayout(form);
    auto *resume = new QCheckBox(L("Продолжать с места остановки"));
    auto *autoplay = new QCheckBox(L("Автоматически включать следующий файл"));
    auto *hardware = new QCheckBox(L("Аппаратное декодирование видео"));
    auto *exclusive = new QCheckBox(L("Эксклюзивный режим (только для внешних ЦАП)"));
    exclusive->setToolTip(L("Не улучшает звук фильмов и сериалов.\nНужен только для внешнего ЦАП и музыки Hi-Res.\nПока включён, другие программы не слышны."));
    resume->setChecked(catalog->settings.resume);
    autoplay->setChecked(catalog->settings.autoplay);
    hardware->setChecked(catalog->settings.hardware);
    exclusive->setChecked(catalog->settings.audioExclusive);
    for (QCheckBox *box : {resume, autoplay, hardware, exclusive})
        layout->addWidget(box);
    QLabel *hint = makeLabel(L("Изображение, декодирование, эксклюзивный режим и язык\nприменяются после перезапуска плеера."),
                             QStringLiteral("hint"));
    hint->setWordWrap(true);
    layout->addWidget(hint);
    layout->addSpacing(6);
    QLabel *version = makeLabel(L("Fero Player %1\nДвижок: %2")
                                    .arg(QStringLiteral(FERO_VERSION),
                                         engine ? engine->version() : L("не загружен")),
                                QStringLiteral("muted"));
    version->setWordWrap(true);
    version->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(version);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Save)->setText(L("Сохранить"));
    buttons->button(QDialogButtonBox::Cancel)->setText(L("Отмена"));
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted)
        return;
    Settings &s = catalog->settings;
    const bool restart = hardware->isChecked() != s.hardware || exclusive->isChecked() != s.audioExclusive ||
                         quality->currentData().toString() != s.quality ||
                         language->currentData().toString() != s.language;
    s.resume = resume->isChecked();
    s.autoplay = autoplay->isChecked();
    s.hardware = hardware->isChecked();
    s.audioExclusive = exclusive->isChecked();
    if (s.audioExclusive)
        AudioWarmup::stop();
    s.quality = quality->currentData().toString();
    s.language = language->currentData().toString();
    applyTheme(theme->currentData().toString());
    persist();
    if (restart)
        notify(L("Перезапустите плеер, чтобы применить новые настройки движка."));
}

void PlayerWindow::showHelp() {
    QMessageBox::information(this, L("Горячие клавиши"),
        L("Пробел / мультимедийная клавиша — воспроизведение и пауза\n"
                       "← / → — назад / вперёд на 10 секунд\n"
                       "Shift+← / Shift+→ — на 1 минуту\n"
                       "↑ / ↓ или колесо мыши над видео — громкость\n"
                       "M — выключить / включить звук\n"
                       "F / F11 — полный экран\nEsc — выйти из полного экрана\n"
                       "Ctrl+O — открыть файлы\nCtrl+Shift+O — добавить папку\n"
                       "Ctrl+← / Ctrl+→ — предыдущий / следующий файл\n"
                       "Ctrl+S — сохранить кадр\nE — следующий кадр\n"
                       "A — следующая аудиодорожка, A1…A9 — дорожка по номеру\n"
                       "S — следующие субтитры, S1…S9 — по номеру, S0 — выключить\n"
                       "Двойной щелчок по видео — полный экран\n"
                       "Перетащите файл субтитров на окно, чтобы подключить его."));
}

// ---- Queue: add from the library ------------------------------------------------------------

void PlayerWindow::addFromLibrary() {
    QDialog dialog(this);
    dialog.setWindowTitle(L("Добавить в очередь"));
    dialog.resize(qMin(1100, width() - 80), qMin(720, height() - 60));
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(12);
    auto *filter = new QLineEdit;
    filter->setPlaceholderText(L("Найти в медиатеке"));
    filter->setClearButtonEnabled(true);
    layout->addWidget(filter);
    auto *view = new TileView(thumbnails);
    view->setPickMode(true);
    view->setColors({palette.text, palette.muted, palette.accent, palette.accentText, palette.hover, palette.border});
    auto *empty = makeLabel(L("Медиатека пуста. Сначала добавьте папку."), QStringLiteral("muted"));
    empty->setAlignment(Qt::AlignCenter);
    layout->addWidget(view, 1);
    layout->addWidget(empty, 1);
    auto *buttons = new QHBoxLayout;
    auto *computer = makeButton(L("С компьютера…"), QStringLiteral("secondary"), QStringLiteral("file_open"),
                                [&dialog] { dialog.done(2); });
    computer->setIcon(vectorIcon(QStringLiteral("file_open"), palette.text));
    buttons->addWidget(computer);
    buttons->addStretch();
    auto *cancel = makeButton(L("Отмена"), QString(), QString(), [&dialog] { dialog.reject(); });
    auto *add = makeButton(L("Добавить в очередь"), QStringLiteral("primary"), QString(), [&dialog] { dialog.accept(); });
    add->setEnabled(false);
    buttons->addWidget(cancel);
    buttons->addWidget(add);
    layout->addLayout(buttons);

    // Choices survive filtering; they are added in library order.
    QStringList chosen;
    const QVector<QJsonObject> all = catalog->allItems();
    bool rebuilding = false;
    auto refresh = [&] {
        rebuilding = true;
        const QString query = filter->text().trimmed().toCaseFolded();
        QVector<QJsonObject> shown;
        for (const QJsonObject &item : all)
            if (query.isEmpty() || item.value(QStringLiteral("title")).toString().toCaseFolded().contains(query))
                shown << item;
        view->populate(shown);
        for (int i = 0; i < view->count(); ++i)
            if (chosen.contains(view->pathAt(i)))
                view->item(i)->setSelected(true);
        rebuilding = false;
        view->setVisible(!all.isEmpty());
        empty->setVisible(all.isEmpty());
    };
    auto updateButton = [&] {
        add->setEnabled(!chosen.isEmpty());
        add->setText(chosen.isEmpty() ? L("Добавить в очередь") : L("Добавить в очередь (%1)").arg(chosen.size()));
    };
    connect(view, &QListWidget::itemSelectionChanged, &dialog, [&] {
        if (rebuilding)
            return;
        for (int i = 0; i < view->count(); ++i) {
            const QString path = view->pathAt(i);
            const bool selected = view->item(i)->isSelected();
            if (selected && !chosen.contains(path))
                chosen << path;
            else if (!selected)
                chosen.removeAll(path);
        }
        updateButton();
    });
    connect(filter, &QLineEdit::textChanged, &dialog, [&] { refresh(); });
    refresh();
    const int result = dialog.exec();
    if (result == 2) {
        openDialog(true);
        return;
    }
    if (result != QDialog::Accepted || chosen.isEmpty())
        return;
    QStringList ordered;
    for (const QJsonObject &item : all) {
        const QString path = item.value(QStringLiteral("path")).toString();
        if (chosen.contains(path))
            ordered << path;
    }
    enqueue(ordered);
}
