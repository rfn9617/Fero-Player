#include "i18n.h"
#include "medialist.h"
#include "catalog.h"
#include "icons.h"
#include "thumbnails.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QHelpEvent>
#include <QToolTip>
#include <QScrollBar>
#include <QStyle>

namespace {
constexpr int RowHeight = 80;
}

QRect MediaDelegate::playRect(const QRect &row) const {
    return {row.right() - 92, row.center().y() - 17, 34, 34};
}

QRect MediaDelegate::moreRect(const QRect &row) const {
    return {row.right() - 48, row.center().y() - 18, 36, 36};
}

QSize MediaDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const {
    return {0, RowHeight};
}

namespace {
constexpr int NumberWidth = 28, PreviewWidth = 112, PreviewHeight = 63;

void paintPreview(QPainter *painter, const QRect &area, const QPixmap &preview, bool audio,
                  const RowColors &colors, int radius) {
    QPainterPath clip;
    clip.addRoundedRect(QRectF(area), radius, radius);
    painter->save();
    painter->setClipPath(clip);
    if (!preview.isNull()) {
        painter->drawPixmap(area, preview);
    } else {
        QLinearGradient fill(area.topLeft(), area.bottomRight());
        fill.setColorAt(0, QColor(colors.hover));
        fill.setColorAt(1, QColor(colors.tile));
        painter->fillRect(area, fill);
        const int icon = qMin(36, area.height() / 2);
        vectorIcon(audio ? QStringLiteral("audio") : QStringLiteral("video"), colors.muted)
            .paint(painter, QRect(area.center().x() - icon / 2, area.center().y() - icon / 2, icon, icon));
    }
    painter->restore();
}
} // namespace

void MediaDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    // Queue row: number · preview · title and details · duration · play · more.
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    const QRect row = option.rect.adjusted(0, 3, 0, -3);
    const bool hot = option.state & (QStyle::State_MouseOver | QStyle::State_Selected);
    const bool current = index.data(MediaRole::Current).toBool();
    if (hot || current) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(colors.hover));
        painter->drawRoundedRect(row, 12, 12);
    }
    int x = row.left() + 8;
    const int number = index.data(MediaRole::Number).toInt();
    QFont base = option.font;
    if (number > 0) {
        QFont numberFont = base;
        numberFont.setPixelSize(14);
        numberFont.setWeight(QFont::DemiBold);
        painter->setFont(numberFont);
        painter->setPen(QColor(current ? colors.accent : colors.muted));
        const QRect numberRect(x, row.top(), NumberWidth, row.height());
        if (current) {
            vectorIcon(QStringLiteral("play"), colors.accent)
                .paint(painter, QRect(numberRect.center().x() - 8, numberRect.center().y() - 8, 16, 16));
        } else {
            painter->drawText(numberRect, Qt::AlignCenter, QString::number(number));
        }
        x += NumberWidth + 10;
    }
    const QRect preview(x, row.center().y() - PreviewHeight / 2, PreviewWidth, PreviewHeight);
    const QString file = index.data(MediaRole::Path).toString();
    paintPreview(painter, preview, thumbnails ? thumbnails->get(file) : QPixmap(),
                 audioExtensions.contains(suffixOf(file)), colors, 8);
    const double progress = index.data(MediaRole::Progress).toDouble();
    if (progress > 0) {
        const QRect track(preview.left() + 6, preview.bottom() - 6, preview.width() - 12, 3);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(255, 255, 255, 70));
        painter->drawRoundedRect(track, 1.5, 1.5);
        painter->setBrush(QColor(colors.accent));
        painter->drawRoundedRect(QRect(track.left(), track.top(), int(track.width() * qMin(1.0, progress)), 3), 1.5, 1.5);
    }
    x = preview.right() + 16;
    const QRect play = playRect(row);
    const QString duration = index.data(MediaRole::Duration).toString();
    QFont small = base;
    small.setPixelSize(13);
    const int durationWidth = QFontMetrics(small).horizontalAdvance(duration) + 6;
    const QRect durationRect(play.left() - 18 - durationWidth, row.top(), durationWidth, row.height());
    const int textRight = durationRect.left() - 14;
    QFont titleFont = base;
    titleFont.setPixelSize(14);
    titleFont.setWeight(QFont::DemiBold);
    painter->setFont(titleFont);
    painter->setPen(QColor(current ? colors.accent : colors.text));
    painter->drawText(QRect(x, row.center().y() - 21, textRight - x, 20), Qt::AlignLeft | Qt::AlignVCenter,
                      QFontMetrics(titleFont).elidedText(index.data(MediaRole::Title).toString(), Qt::ElideRight, textRight - x));
    painter->setFont(small);
    painter->setPen(QColor(colors.muted));
    painter->drawText(QRect(x, row.center().y() + 2, textRight - x, 20), Qt::AlignLeft | Qt::AlignVCenter,
                      QFontMetrics(small).elidedText(index.data(MediaRole::Info).toString(), Qt::ElideMiddle, textRight - x));
    painter->drawText(durationRect, Qt::AlignVCenter | Qt::AlignRight, duration);
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(colors.accent));
    painter->drawEllipse(play);
    vectorIcon(QStringLiteral("play"), colors.accentText).paint(painter, play.adjusted(8, 8, -7, -8));
    vectorIcon(QStringLiteral("more"), colors.text).paint(painter, moreRect(row).adjusted(7, 7, -7, -7));
    painter->restore();
}

QRect MediaDelegate::textRect(const QRect &cell) const {
    // Title and details column of a queue row.
    const QRect row = cell.adjusted(0, 3, 0, -3);
    const int left = row.left() + 8 + NumberWidth + 10 + PreviewWidth + 16;
    return {left, row.center().y() - 22, qMax(0, playRect(row).left() - 110 - left), 44};
}

bool MediaDelegate::helpEvent(QHelpEvent *event, QAbstractItemView *view, const QStyleOptionViewItem &option,
                              const QModelIndex &index) {
    if (event->type() == QEvent::ToolTip && index.isValid()) {
        if (textRect(option.rect).contains(event->pos())) {
            QToolTip::showText(event->globalPos(), QDir::toNativeSeparators(index.data(MediaRole::Path).toString()), view);
            return true;
        }
        QToolTip::hideText();
        return true;   // nothing over the preview, buttons or empty space
    }
    return QStyledItemDelegate::helpEvent(event, view, option, index);
}

bool MediaDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option,
                                const QModelIndex &index) {
    if (event->type() == QEvent::MouseButtonRelease) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        const QRect row = option.rect.adjusted(0, 3, 0, -3);
        if (mouse->button() == Qt::LeftButton && playRect(row).contains(mouse->position().toPoint())) {
            emit playClicked(index.row());
            return true;
        }
        if (mouse->button() == Qt::LeftButton && moreRect(row).contains(mouse->position().toPoint())) {
            emit menuClicked(index.row(), mouse->globalPosition().toPoint());
            return true;
        }
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

MediaList::MediaList(bool queue, MediaDelegate *rows, QWidget *parent) : QListWidget(parent), isQueue(queue) {
    delegate = rows ? rows : new MediaDelegate(this);
    if (rows)
        rows->setParent(this);
    setItemDelegate(delegate);
    setMouseTracking(true);
    setUniformItemSizes(true);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setFrameShape(QFrame::NoFrame);
    viewport()->setAttribute(Qt::WA_Hover);
    if (queue) {
        setDragDropMode(QAbstractItemView::InternalMove);
        setDefaultDropAction(Qt::MoveAction);
    }
}

void MediaList::setColors(const RowColors &colors) {
    delegate->colors = colors;
    viewport()->update();
}

QString MediaList::pathAt(int row) const {
    const QListWidgetItem *entry = item(row);
    return entry ? entry->data(MediaRole::Path).toString() : QString();
}

void MediaList::populate(const QVector<QJsonObject> &records, int currentRow) {
    setUpdatesEnabled(false);
    clear();
    for (int i = 0; i < records.size(); ++i) {
        const QJsonObject &record = records[i];
        const QString path = record.value(QStringLiteral("path")).toString();
        const QFileInfo info(path);
        auto *entry = new QListWidgetItem;
        entry->setData(MediaRole::Path, path);
        entry->setData(MediaRole::Title, record.value(QStringLiteral("title")).toString(info.completeBaseName()));
        const QString suffix = info.suffix().toUpper().isEmpty() ? L("Файл") : info.suffix().toUpper();
        const qint64 position = Catalog::resumePosition(&record);
        entry->setData(MediaRole::Info, position
            ? L("%1  ·  Продолжить с %2").arg(suffix, formatTime(position))
            : QStringLiteral("%1  ·  %2").arg(suffix, info.dir().dirName()));
        const qint64 duration = qint64(record.value(QStringLiteral("duration")).toDouble());
        entry->setData(MediaRole::Duration, duration ? formatTime(duration) : QString());
        entry->setData(MediaRole::Number, isQueue ? i + 1 : 0);
        entry->setData(MediaRole::Current, isQueue && i == currentRow);
        entry->setData(MediaRole::Progress, duration > 0 && position > 0 ? double(position) / duration : 0.0);
        entry->setData(MediaRole::Detail, duration ? QStringLiteral("%1  ·  %2").arg(suffix, formatTime(duration)) : suffix);
        entry->setSizeHint(viewMode() == QListView::IconMode ? delegate->sizeHint(QStyleOptionViewItem(), QModelIndex())
                                                             : QSize(0, RowHeight));
        addItem(entry);
    }
    setUpdatesEnabled(true);
}

// ---- Tiles --------------------------------------------------------------------------

TileDelegate::TileDelegate(Thumbnails *previews, QObject *parent) : MediaDelegate(parent) {
    thumbnails = previews;
}

QSize TileDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const {
    return cell;
}

QRect TileDelegate::cardRect(const QRect &area) const {
    return area.adjusted(Gap / 2, Gap / 2, -Gap / 2, -Gap / 2);
}

QRect TileDelegate::thumbRect(const QRect &area) const {
    const QRect card = cardRect(area);
    return {card.x(), card.y(), card.width(), card.width() * 9 / 16};
}

QRect TileDelegate::textRect(const QRect &area) const {
    const QRect card = cardRect(area);
    const QRect thumb = thumbRect(area);
    return {card.left(), thumb.bottom() + 6, card.width(), card.bottom() - thumb.bottom() - 6};
}

QRect TileDelegate::tileMoreRect(const QRect &area) const {
    const QRect thumb = thumbRect(area);
    return {thumb.right() - 36, thumb.top() + 8, 28, 28};
}

void TileDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    const bool hot = (option.state & QStyle::State_MouseOver) && !pickMode;
    const bool picked = pickMode && (option.state & QStyle::State_Selected);
    const QRect card = cardRect(option.rect);
    const QRect thumb = thumbRect(option.rect);
    const QString path = index.data(MediaRole::Path).toString();
    const bool audio = audioExtensions.contains(suffixOf(path));

    QPainterPath clip;
    clip.addRoundedRect(QRectF(thumb), 12, 12);
    painter->setClipPath(clip);
    const QPixmap preview = thumbnails ? thumbnails->get(path) : QPixmap();
    if (!preview.isNull()) {
        painter->drawPixmap(thumb, preview);
    } else {
        QLinearGradient fill(thumb.topLeft(), thumb.bottomRight());
        fill.setColorAt(0, QColor(colors.hover));
        fill.setColorAt(1, QColor(colors.tile));
        painter->fillRect(thumb, fill);
        vectorIcon(audio ? QStringLiteral("audio") : QStringLiteral("video"), colors.muted)
            .paint(painter, QRect(thumb.center().x() - 18, thumb.center().y() - 18, 36, 36));
    }
    if (hot)
        painter->fillRect(thumb, QColor(0, 0, 0, 80));
    const double progress = index.data(MediaRole::Progress).toDouble();
    if (progress > 0) {
        const QRect track(thumb.left(), thumb.bottom() - 3, thumb.width(), 4);
        painter->fillRect(track, QColor(255, 255, 255, 70));
        painter->fillRect(QRect(track.left(), track.top(), int(track.width() * qMin(1.0, progress)), track.height()),
                          QColor(colors.accent));
    }
    painter->setClipping(false);
    if (pickMode && (option.state & QStyle::State_MouseOver) && !picked) {
        painter->setPen(QPen(QColor(colors.muted), 1.5));
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(QRectF(thumb).adjusted(0.75, 0.75, -0.75, -0.75), 12, 12);
    }
    if (picked) {
        painter->setPen(QPen(QColor(colors.accent), 3));
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(QRectF(thumb).adjusted(1.5, 1.5, -1.5, -1.5), 11, 11);
        const QRect badge(thumb.left() + 10, thumb.top() + 10, 26, 26);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(colors.accent));
        painter->drawEllipse(badge);
        QPen tick(QColor(colors.accentText), 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter->setPen(tick);
        const QPointF c = QRectF(badge).center();
        painter->drawPolyline(QPolygonF{QPointF(c.x() - 5.5, c.y() + 0.5), QPointF(c.x() - 1.5, c.y() + 4.5),
                                        QPointF(c.x() + 6, c.y() - 4)});
    }

    // Duration badge.
    const QString duration = index.data(MediaRole::Duration).toString();
    QFont small = option.font;
    small.setPixelSize(11);
    small.setWeight(QFont::DemiBold);
    if (!duration.isEmpty()) {
        painter->setFont(small);
        const int w = QFontMetrics(small).horizontalAdvance(duration) + 12;
        const QRect badge(thumb.right() - w - 8, thumb.bottom() - 26, w, 18);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0, 0, 0, 175));
        painter->drawRoundedRect(badge, 6, 6);
        painter->setPen(QColor(255, 255, 255));
        painter->drawText(badge, Qt::AlignCenter, duration);
    }
    if (hot) {
        const QRect play(thumb.center().x() - 24, thumb.center().y() - 24, 48, 48);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(colors.accent));
        painter->drawEllipse(play);
        vectorIcon(QStringLiteral("play"), colors.accentText).paint(painter, play.adjusted(13, 12, -11, -12));
        const QRect more = tileMoreRect(option.rect);
        painter->setBrush(QColor(0, 0, 0, 150));
        painter->drawEllipse(more);
        vectorIcon(QStringLiteral("more"), QStringLiteral("#ffffff")).paint(painter, more.adjusted(5, 5, -5, -5));
    }

    // Title and details.
    QFont title = option.font;
    title.setPixelSize(13);
    title.setWeight(QFont::DemiBold);
    painter->setFont(title);
    painter->setPen(QColor(colors.text));
    const QRect titleRect(card.left() + 2, thumb.bottom() + 8, card.width() - 4, 20);
    painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter,
                      QFontMetrics(title).elidedText(index.data(MediaRole::Title).toString(), Qt::ElideRight, titleRect.width()));
    QFont detail = option.font;
    detail.setPixelSize(12);
    painter->setFont(detail);
    painter->setPen(QColor(colors.muted));
    const QRect detailRect(card.left() + 2, titleRect.bottom() + 2, card.width() - 4, 18);
    const QString info = progress > 0 ? index.data(MediaRole::Info).toString() : index.data(MediaRole::Detail).toString();
    painter->drawText(detailRect, Qt::AlignLeft | Qt::AlignVCenter,
                      QFontMetrics(detail).elidedText(info, Qt::ElideRight, detailRect.width()));
    painter->restore();
}

bool TileDelegate::editorEvent(QEvent *event, QAbstractItemModel *, const QStyleOptionViewItem &option,
                               const QModelIndex &index) {
    if (pickMode || event->type() != QEvent::MouseButtonRelease)
        return false;
    auto *mouse = static_cast<QMouseEvent *>(event);
    if (mouse->button() != Qt::LeftButton)
        return false;
    const QPoint point = mouse->position().toPoint();
    if (tileMoreRect(option.rect).contains(point)) {
        emit menuClicked(index.row(), mouse->globalPosition().toPoint());
        return true;
    }
    if (thumbRect(option.rect).contains(point)) {
        emit playClicked(index.row());
        return true;
    }
    return false;
}

TileView::TileView(Thumbnails *thumbnails, QWidget *parent)
    : MediaList(false, new TileDelegate(thumbnails, nullptr), parent) {
    tiles = static_cast<TileDelegate *>(rows());
    setViewMode(QListView::IconMode);
    setFlow(QListView::LeftToRight);
    setWrapping(true);
    setResizeMode(QListView::Adjust);
    setMovement(QListView::Static);
    setSpacing(0);
    setUniformItemSizes(true);
    setSelectionMode(QAbstractItemView::NoSelection);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    verticalScrollBar()->setSingleStep(40);
    if (thumbnails)
        connect(thumbnails, &Thumbnails::ready, viewport(), [this] { viewport()->update(); });
}

void TileView::resizeEvent(QResizeEvent *event) {
    MediaList::resizeEvent(event);
    layoutGrid();
}

void TileView::layoutGrid() {
    // Columns never depend on whether the scroll bar is shown: otherwise the bar
    // appearing changes the column count, which hides the bar again, and so on.
    const int bar = style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, this);
    const int full = viewport()->width() + (verticalScrollBar()->isVisible() ? bar : 0);
    const int available = qMax(200, full - bar - 2);
    const int columns = qMax(1, available / (220 + TileDelegate::Gap));
    const int cellWidth = available / columns;
    const int tileWidth = cellWidth - TileDelegate::Gap;
    const QSize cell(cellWidth, tileWidth * 9 / 16 + TileDelegate::TextHeight + TileDelegate::Gap);
    if (cell != tiles->cell) {
        tiles->cell = cell;
        setGridSize(cell);
        for (int i = 0; i < count(); ++i)
            item(i)->setSizeHint(cell);
        doItemsLayout();
    }
}

void TileView::setPickMode(bool pick) {
    tiles->pickMode = pick;
    setSelectionMode(pick ? QAbstractItemView::MultiSelection : QAbstractItemView::NoSelection);
    viewport()->update();
}
