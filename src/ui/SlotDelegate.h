#pragma once

#include <QHash>
#include <QPixmap>
#include <QSizeF>
#include <QStyledItemDelegate>

#include <array>
#include <cstddef>

// Paints each slot as a card: a bike photo when booked (with the booker's name),
// a bike placeholder when free, the slot name, and a status pill.
class SlotDelegate final : public QStyledItemDelegate
{
public:
    explicit SlotDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    // Scaled photos are cached per size and device pixel ratio, so painting does not rescale them.
    QPixmap scaledPhoto(std::size_t index, const QSizeF &logicalSize, qreal dpr) const;

    std::array<QPixmap, 6> m_photos;
    mutable QHash<QString, QPixmap> m_scaledPhotos;
};
