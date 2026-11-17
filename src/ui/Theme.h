#pragma once

#include <QColor>

namespace Theme {

// Colours used when painting slot cards. Two sets exist: one for light and one for dark mode.
struct Palette
{
    QColor card;
    QColor border;
    QColor placeholder;
    QColor muted;
    QColor text;
    QColor free;
    QColor booked;
    QColor accent;
    QColor badge;
};

// The palette matching the system light or dark setting.
[[nodiscard]] const Palette& current();

} // namespace Theme
