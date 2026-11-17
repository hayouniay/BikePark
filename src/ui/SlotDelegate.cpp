#include "ui/SlotDelegate.h"

#include "ui/Icons.h"
#include "ui/Theme.h"
#include "ui/SlotListModel.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QPaintDevice>

#include <algorithm>

namespace {

constexpr int kCardWidth = 200;
constexpr int kCardHeight = 180;
constexpr double kMargin = 8.0;
constexpr double kRadius = 14.0;
constexpr double kFooterHeight = 46.0;
constexpr double kOwnerBadgeHeight = 26.0;


} // namespace

SlotDelegate::SlotDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
    for (std::size_t i = 0; i < m_photos.size(); ++i)
        m_photos[i].load(QStringLiteral(":/images/bike%1.jpg").arg(static_cast<int>(i) + 1));
}

QSize SlotDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const
{
    return {kCardWidth, kCardHeight};
}

QPixmap SlotDelegate::scaledPhoto(std::size_t index, const QSizeF &logicalSize, qreal dpr) const
{
    const QSize pixels = (logicalSize * dpr).toSize();
    const QString key = QStringLiteral("%1:%2x%3").arg(index).arg(pixels.width()).arg(pixels.height());

    if (const auto cached = m_scaledPhotos.constFind(key); cached != m_scaledPhotos.constEnd())
        return cached.value();

    QPixmap scaled = m_photos[index].scaled(pixels, Qt::KeepAspectRatioByExpanding,
                                            Qt::SmoothTransformation);
    scaled.setDevicePixelRatio(dpr);
    m_scaledPhotos.insert(key, scaled);
    return scaled;
}

void SlotDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                         const QModelIndex &index) const
{
    const bool booked = index.data(SlotListModel::BookedRole).toBool();
    const int id = index.data(SlotListModel::IdRole).toInt();
    const QString name = index.data(SlotListModel::SlotRole).toString();
    const QString owner = index.data(SlotListModel::OwnerRole).toString();
    const bool selected = option.state.testFlag(QStyle::State_Selected);
    const bool hovered = option.state.testFlag(QStyle::State_MouseOver);
    const qreal dpr = painter->device() ? painter->device()->devicePixelRatioF() : 1.0;
    const Theme::Palette &palette = Theme::current();

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::SmoothPixmapTransform);

    const QRectF card = QRectF(option.rect).adjusted(kMargin, kMargin, -kMargin, -kMargin);
    QPainterPath cardPath;
    cardPath.addRoundedRect(card, kRadius, kRadius);
    painter->fillPath(cardPath, palette.card);

    // Photo area, clipped to the rounded card so the corners stay clean.
    const QRectF photoArea(card.left(), card.top(), card.width(), card.height() - kFooterHeight);
    painter->setClipPath(cardPath);
    painter->setClipRect(photoArea, Qt::IntersectClip);
    if (booked) {
        const auto photoIndex = static_cast<std::size_t>(std::max(id - 1, 0)) % m_photos.size();
        const QPixmap photo = scaledPhoto(photoIndex, photoArea.size(), dpr);
        const QSizeF shown = photo.deviceIndependentSize();
        const QPointF offset((photoArea.width() - shown.width()) / 2.0,
                             (photoArea.height() - shown.height()) / 2.0);
        painter->drawPixmap(photoArea.topLeft() + offset, photo);

        if (!owner.isEmpty()) {
            const QRectF badge(photoArea.left(), photoArea.bottom() - kOwnerBadgeHeight,
                               photoArea.width(), kOwnerBadgeHeight);
            painter->fillRect(badge, palette.badge);
            painter->setPen(Qt::white);
            const QFontMetrics metrics(option.font);
            const QString shownOwner =
                metrics.elidedText(owner, Qt::ElideRight, qRound(badge.width()) - 20);
            painter->drawText(badge.adjusted(10, 0, -10, 0), Qt::AlignVCenter | Qt::AlignLeft,
                              shownOwner);
        }
    } else {
        painter->fillRect(photoArea, palette.placeholder);
        const qreal iconSize = 54.0;
        const QPointF center = photoArea.center();
        Icons::paintBike(*painter,
                         QRectF(center.x() - iconSize / 2.0, center.y() - iconSize / 2.0 - 8.0,
                                iconSize, iconSize),
                         palette.muted);
        painter->setPen(palette.muted);
        painter->drawText(QRectF(photoArea.left(), center.y() + iconSize / 2.0 - 6.0,
                                 photoArea.width(), 20.0),
                          Qt::AlignHCenter | Qt::AlignTop, QStringLiteral("Available"));
    }
    painter->setClipping(false);

    // Border: accent when selected, subtle highlight on hover.
    QPen border(palette.border, 1.0);
    if (hovered)
        border.setColor(palette.accent.lighter(140));
    if (selected)
        border = QPen(palette.accent, 2.5);
    painter->setPen(border);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(cardPath);

    // Footer: slot name on the left.
    QFont nameFont = option.font;
    nameFont.setBold(true);
    painter->setFont(nameFont);
    painter->setPen(palette.text);
    const QRectF footer(card.left() + 14, card.bottom() - kFooterHeight, card.width() - 100,
                        kFooterHeight);
    painter->drawText(footer, Qt::AlignVCenter | Qt::AlignLeft,
                      QStringLiteral("Slot %1").arg(name));

    // Footer: status pill on the right.
    const QRectF pill(card.right() - 84, card.bottom() - kFooterHeight / 2.0 - 11, 70, 22);
    painter->setPen(Qt::NoPen);
    painter->setBrush(booked ? palette.booked : palette.free);
    painter->drawRoundedRect(pill, 11, 11);

    QFont pillFont = option.font;
    pillFont.setBold(true);
    painter->setFont(pillFont);
    painter->setPen(Qt::white);
    painter->drawText(pill, Qt::AlignCenter,
                      booked ? QStringLiteral("BOOKED") : QStringLiteral("FREE"));

    painter->restore();
}
