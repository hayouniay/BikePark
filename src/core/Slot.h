#pragma once

#include <QDateTime>
#include <QString>

struct Slot
{
    int id = 0;
    QString name;
    bool booked = false;
    QString owner;      // display name of the person who booked it; empty when unknown
    QDateTime bookedAt; // invalid when unknown (bookings made before times were recorded)
    int ownerId = 0;    // account id of the booker; 0 when unknown
};
