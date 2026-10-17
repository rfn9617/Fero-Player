#pragma once
#include <QJsonObject>
#include <QListWidget>
#include <QStyledItemDelegate>
#include <QVector>

// Lightweight media rows: painted by a delegate instead of one widget tree per
// row, so thousands of library entries cost almost nothing.
namespace MediaRole {
enum { Path = Qt::UserRole + 1, Title, Info, Duration, Number, Current, Progress, Detail };
}

class Thumbnails;

struct RowColors {
    QString text, muted, accent, accentText, hover, tile;
};

class MediaDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    RowColors colors;
    Thumbnails *thumbnails = nullptr;   // previews; optional
    bool pickMode = false;              // selection dialog: no play/menu buttons
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option,
                     const QModelIndex &index) override;
    // The file location appears only when the pointer is over the title text.
    bool helpEvent(QHelpEvent *event, QAbstractItemView *view, const QStyleOptionViewItem &option,
                   const QModelIndex &index) override;
signals:
    void playClicked(int row);
    void menuClicked(int row, const QPoint &globalPosition);
protected:
    virtual QRect textRect(const QRect &cell) const;
    QRect playRect(const QRect &row) const;
    QRect moreRect(const QRect &row) const;
};

// Card with a video preview, used by the library and recent pages.
class TileDelegate : public MediaDelegate {
    Q_OBJECT
public:
    TileDelegate(Thumbnails *previews, QObject *parent);
    QSize cell{240, 200};
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option,
                     const QModelIndex &index) override;
    static constexpr int Gap = 14, TextHeight = 54;
protected:
    QRect textRect(const QRect &cell) const override;
private:
    QRect cardRect(const QRect &cell) const;
    QRect thumbRect(const QRect &cell) const;
    QRect tileMoreRect(const QRect &cell) const;
};

class MediaList : public QListWidget {
    Q_OBJECT
public:
    explicit MediaList(bool queue, MediaDelegate *rows = nullptr, QWidget *parent = nullptr);
    void populate(const QVector<QJsonObject> &records, int currentRow = -1);
    void setColors(const RowColors &colors);
    QString pathAt(int row) const;
    MediaDelegate *rows() const { return delegate; }
    const bool isQueue;
private:
    MediaDelegate *delegate;
};

class TileView : public MediaList {
    Q_OBJECT
public:
    explicit TileView(Thumbnails *thumbnails, QWidget *parent = nullptr);
protected:
    void resizeEvent(QResizeEvent *event) override;
private:
    void layoutGrid();
    TileDelegate *tiles;
public:
    void setPickMode(bool pick);
};
