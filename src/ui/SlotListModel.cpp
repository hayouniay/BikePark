#include "ui/SlotListModel.h"

#include <QCoreApplication>
#include <QLocale>

#include <algorithm>
#include <utility>

SlotListModel::SlotListModel(QObject* parent) : QAbstractListModel(parent) {}

int SlotListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_slots.size());
}

QVariant SlotListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_slots.size()))
        return {};

    const Slot& slot = m_slots.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case SlotRole:
        return slot.name;
    case BookedRole:
        return slot.booked;
    case IdRole:
        return slot.id;
    case OwnerRole:
        return slot.owner;
    case Qt::ToolTipRole:
    case Qt::AccessibleTextRole:
        return QStringLiteral("Slot %1: %2").arg(slot.name, describeSlot(slot));
    default:
        return {};
    }
}

void SlotListModel::setSlots(QList<Slot> newSlots)
{
    beginResetModel();
    m_slots = std::move(newSlots);
    endResetModel();
}

const Slot* SlotListModel::slotAt(int row) const
{
    return (row >= 0 && row < static_cast<int>(m_slots.size())) ? &m_slots.at(row) : nullptr;
}

int SlotListModel::bookedCount() const
{
    return static_cast<int>(
        std::ranges::count_if(m_slots, [](const Slot& slot) { return slot.booked; }));
}

QString describeSlot(const Slot& slot)
{
    using Q = QCoreApplication;
    if (!slot.booked)
        return Q::translate("SlotListModel", "Available");

    const QString who = slot.owner.isEmpty() ? Q::translate("SlotListModel", "unknown") : slot.owner;
    if (!slot.bookedAt.isValid())
        return Q::translate("SlotListModel", "Booked by %1").arg(who);

    const QString when = QLocale().toString(slot.bookedAt.toLocalTime(), QLocale::ShortFormat);
    return Q::translate("SlotListModel", "Booked by %1 on %2").arg(who, when);
}
