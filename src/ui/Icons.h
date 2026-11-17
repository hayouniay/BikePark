#pragma once

#include <QColor>
#include <QIcon>
#include <QRectF>

class QPainter;

// Vector icons and illustrations drawn with QPainter, so the app needs no image files
// for its UI chrome and every icon stays sharp at any size or DPI.
namespace Icons {

// Painting primitives, usable directly on a card or in any widget.
void paintParking(QPainter &painter, const QRectF &rect, const QColor &color);
void paintBike(QPainter &painter, const QRectF &rect, const QColor &color);
void paintCheck(QPainter &painter, const QRectF &rect, const QColor &color);
void paintCross(QPainter &painter, const QRectF &rect, const QColor &color);
void paintRefresh(QPainter &painter, const QRectF &rect, const QColor &color);
void paintPlus(QPainter &painter, const QRectF &rect, const QColor &color);
void paintTrash(QPainter &painter, const QRectF &rect, const QColor &color);

// QIcon factories for actions and buttons (several sizes baked in).
QIcon parking(const QColor &color);
QIcon bike(const QColor &color);
QIcon book(const QColor &color);
QIcon release(const QColor &color);
QIcon refresh(const QColor &color);
QIcon add(const QColor &color);
QIcon trash(const QColor &color);

} // namespace Icons
