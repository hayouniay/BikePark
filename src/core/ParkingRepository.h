#pragma once

#include "core/Account.h"
#include "core/Slot.h"

#include <QDateTime>
#include <QList>
#include <QString>

#include <expected>
#include <optional>

// Bookings older than this are released automatically by releaseExpired().
inline constexpr int kBookingLimitHours = 12;
inline constexpr int kMinPasswordLength = 8;

// Owns the SQLite connection and enforces the rules: who may book, release, add and delete.
// Every operation reports failure through std::expected.
class ParkingRepository final
{
public:
    explicit ParkingRepository(QString databasePath);
    ~ParkingRepository();

    ParkingRepository(const ParkingRepository&) = delete;
    ParkingRepository& operator=(const ParkingRepository&) = delete;

    [[nodiscard]] std::expected<void, QString> open();

    // Accounts. The very first account needs no actor and always becomes an administrator.
    // After that, only an administrator can create accounts.
    [[nodiscard]] std::expected<bool, QString> hasAccounts() const;
    [[nodiscard]] std::expected<void, QString> createAccount(const QString& username,
                                                             const QString& displayName,
                                                             const QString& password,
                                                             bool isAdministrator,
                                                             const std::optional<Account>& actor);
    [[nodiscard]] std::expected<Account, QString> signIn(const QString& username,
                                                         const QString& password) const;

    // Slots. Members book free slots and release their own bookings; administrators can release any.
    [[nodiscard]] std::expected<QList<Slot>, QString> allSlots() const;
    [[nodiscard]] std::expected<void, QString> book(const QString& slotName, const Account& actor,
                                                    const QDateTime& when);
    [[nodiscard]] std::expected<void, QString> cancel(const QString& slotName, const Account& actor);
    [[nodiscard]] std::expected<int, QString> releaseExpired(const QDateTime& now, int maxHours);

    // Administrators only.
    [[nodiscard]] std::expected<void, QString> addSlot(const QString& slotName, const Account& actor);
    [[nodiscard]] std::expected<void, QString> removeSlot(const QString& slotName, const Account& actor);

private:
    QString m_databasePath;
};
