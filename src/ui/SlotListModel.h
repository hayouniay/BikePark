#pragma once

#include "core/Slot.h"

#include <QAbstractListModel>
#include <QList>

class SlotListModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role
    {
        SlotRole = Qt::UserRole + 1,
        BookedRole,
        IdRole,
        OwnerRole,
    };

    explicit SlotListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    void setSlots(QList<Slot> newSlots);
    [[nodiscard]] const Slot* slotAt(int row) const;
    [[nodiscard]] int bookedCount() const;

private:
    QList<Slot> m_slots;
};

// One-line human description, e.g. "Booked by Alice on 16/11/2026, 10:00" or "Available".
[[nodiscard]] QString describeSlot(const Slot& slot);
