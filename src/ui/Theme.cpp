#include "ui/Theme.h"

#include <QGuiApplication>
#include <QStyleHints>

namespace Theme {

namespace {

const Palette kLight{
    QColor(255, 255, 255), // card
    QColor("#e3e8ef"),     // border
    QColor("#eef2f7"),     // placeholder
    QColor("#8a96a6"),     // muted
    QColor("#1f2933"),     // text
    QColor("#2e7d32"),     // free pill
    QColor("#c62828"),     // booked pill
    QColor("#2563eb"),     // accent
    QColor(15, 23, 42, 190), // owner badge
};

const Palette kDark{
    QColor("#1e293b"),
    QColor("#334155"),
    QColor("#0f172a"),
    QColor("#94a3b8"),
    QColor("#e2e8f0"),
    QColor("#16a34a"),
    QColor("#dc2626"),
    QColor("#60a5fa"),
    QColor(2, 6, 23, 200),
};

} // namespace

const Palette& current()
{
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark ? kDark : kLight;
}

} // namespace Theme
