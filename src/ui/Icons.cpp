#include "ui/Icons.h"

#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPolygonF>

#include <initializer_list>

namespace {

using DrawFn = void (*)(QPainter &, const QRectF &, const QColor &);

// Point in a unit square mapped onto the given rectangle.
QPointF at(const QRectF &r, qreal x, qreal y)
{
    return {r.left() + x * r.width(), r.top() + y * r.height()};
}

QIcon makeIcon(DrawFn draw, const QColor &color)
{
    QIcon icon;
    for (const int size : {16, 20, 24, 32, 48}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        {
            QPainter painter(&pixmap);
            painter.setRenderHint(QPainter::Antialiasing);
            draw(painter, QRectF(0, 0, size, size), color);
        }
        icon.addPixmap(pixmap);
    }
    return icon;
}

} // namespace

namespace Icons {

void paintParking(QPainter &painter, const QRectF &rect, const QColor &color)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawRoundedRect(rect, rect.width() * 0.24, rect.width() * 0.24);

    QFont font = painter.font();
    font.setBold(true);
    font.setPixelSize(qRound(rect.height() * 0.62));
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(rect, Qt::AlignCenter, QStringLiteral("P"));
    painter.restore();
}

void paintBike(QPainter &painter, const QRectF &rect, const QColor &color)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(color, rect.width() * 0.08, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);

    const qreal wheel = rect.width() * 0.2;
    const QPointF rear = at(rect, 0.25, 0.68);
    const QPointF front = at(rect, 0.76, 0.68);
    const QPointF hub = at(rect, 0.5, 0.68);
    const QPointF seat = at(rect, 0.4, 0.36);
    const QPointF head = at(rect, 0.68, 0.36);

    painter.drawEllipse(rear, wheel, wheel);
    painter.drawEllipse(front, wheel, wheel);
    painter.drawLine(rear, hub);
    painter.drawLine(hub, seat);
    painter.drawLine(seat, rear);
    painter.drawLine(hub, head);
    painter.drawLine(head, front);
    painter.drawLine(seat, at(rect, 0.32, 0.36));   // saddle
    painter.drawLine(head, at(rect, 0.74, 0.27));   // handlebar
    painter.restore();
}

void paintCheck(QPainter &painter, const QRectF &rect, const QColor &color)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(color, rect.width() * 0.13, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);

    QPainterPath path;
    path.moveTo(at(rect, 0.2, 0.52));
    path.lineTo(at(rect, 0.42, 0.72));
    path.lineTo(at(rect, 0.8, 0.3));
    painter.drawPath(path);
    painter.restore();
}

void paintCross(QPainter &painter, const QRectF &rect, const QColor &color)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(color, rect.width() * 0.13, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(at(rect, 0.28, 0.28), at(rect, 0.72, 0.72));
    painter.drawLine(at(rect, 0.72, 0.28), at(rect, 0.28, 0.72));
    painter.restore();
}

void paintRefresh(QPainter &painter, const QRectF &rect, const QColor &color)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(color, rect.width() * 0.12, Qt::SolidLine, Qt::RoundCap));
    painter.setBrush(Qt::NoBrush);

    const qreal inset = rect.width() * 0.18;
    painter.drawArc(rect.adjusted(inset, inset, -inset, -inset), 30 * 16, 290 * 16);

    // Arrowhead at the end of the arc, pointing in the direction of travel.
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    QPolygonF arrow;
    arrow << at(rect, 0.835, 0.599) << at(rect, 0.822, 0.770) << at(rect, 0.668, 0.642);
    painter.drawPolygon(arrow);
    painter.restore();
}

void paintPlus(QPainter &painter, const QRectF &rect, const QColor &color)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(color, rect.width() * 0.13, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(at(rect, 0.5, 0.2), at(rect, 0.5, 0.8));
    painter.drawLine(at(rect, 0.2, 0.5), at(rect, 0.8, 0.5));
    painter.restore();
}

void paintTrash(QPainter &painter, const QRectF &rect, const QColor &color)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(color, rect.width() * 0.11, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawLine(at(rect, 0.2, 0.3), at(rect, 0.8, 0.3));           // lid
    painter.drawLine(at(rect, 0.4, 0.3), at(rect, 0.4, 0.18));          // handle
    painter.drawLine(at(rect, 0.6, 0.3), at(rect, 0.6, 0.18));
    painter.drawLine(at(rect, 0.4, 0.18), at(rect, 0.6, 0.18));
    QPainterPath bin;
    bin.moveTo(at(rect, 0.28, 0.3));
    bin.lineTo(at(rect, 0.34, 0.84));
    bin.lineTo(at(rect, 0.66, 0.84));
    bin.lineTo(at(rect, 0.72, 0.3));
    painter.drawPath(bin);
    painter.restore();
}

QIcon parking(const QColor &color) { return makeIcon(paintParking, color); }
QIcon bike(const QColor &color) { return makeIcon(paintBike, color); }
QIcon book(const QColor &color) { return makeIcon(paintCheck, color); }
QIcon release(const QColor &color) { return makeIcon(paintCross, color); }
QIcon refresh(const QColor &color) { return makeIcon(paintRefresh, color); }
QIcon add(const QColor &color) { return makeIcon(paintPlus, color); }
QIcon trash(const QColor &color) { return makeIcon(paintTrash, color); }

} // namespace Icons
