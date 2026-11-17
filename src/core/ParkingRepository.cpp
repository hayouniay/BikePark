#include "core/ParkingRepository.h"

#include <QByteArray>
#include <QChar>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QRandomGenerator>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

#include <algorithm>
#include <array>
#include <utility>

namespace {

const QString kConnectionName = QStringLiteral("bikeparking");
constexpr int kSlotCount = 16;
constexpr int kMaxOwnerLength = 60;
constexpr int kMaxSlotNameLength = 6;
constexpr int kMinUsernameLength = 3;
constexpr int kMaxUsernameLength = 32;
constexpr int kMaxDisplayNameLength = 60;
constexpr int kPasswordIterations = 120000;
constexpr int kSaltBytes = 16;

// Timestamps are stored as UTC ISO 8601 text, which sorts correctly as a string.
QString timestamp(const QDateTime &when)
{
    return when.toUTC().toString(Qt::ISODate);
}

// Runs a prepared write and checks that exactly one row changed.
std::expected<void, QString> requireOneRow(QSqlQuery &query, const QString &message)
{
    if (!query.exec())
        return std::unexpected(query.lastError().text());
    if (query.numRowsAffected() != 1)
        return std::unexpected(message);
    return {};
}

// Older databases lack the owner, time and account columns. Add them in place so existing
// rows are kept. Bookings made before accounts existed have owner 0 and can only be released
// by an administrator.
std::expected<void, QString> addMissingColumns(QSqlDatabase &db)
{
    QSqlQuery info(db);
    if (!info.exec(QStringLiteral("PRAGMA table_info(cppbuzz_bikeparking)")))
        return std::unexpected(info.lastError().text());

    QStringList existing;
    while (info.next())
        existing << info.value(1).toString();

    const std::array<std::pair<QString, QString>, 3> columns{{
        {QStringLiteral("BookedBy"),
         QStringLiteral("ALTER TABLE cppbuzz_bikeparking ADD COLUMN BookedBy VARCHAR(60) NOT NULL DEFAULT ''")},
        {QStringLiteral("BookedAt"),
         QStringLiteral("ALTER TABLE cppbuzz_bikeparking ADD COLUMN BookedAt VARCHAR(32) NOT NULL DEFAULT ''")},
        {QStringLiteral("BookedUserId"),
         QStringLiteral("ALTER TABLE cppbuzz_bikeparking ADD COLUMN BookedUserId INTEGER NOT NULL DEFAULT 0")},
    }};

    for (const auto &[column, ddl] : columns) {
        if (existing.contains(column))
            continue;
        QSqlQuery alter(db);
        if (!alter.exec(ddl))
            return std::unexpected(alter.lastError().text());
    }
    return {};
}

bool isValidSlotName(const QString &name)
{
    if (name.isEmpty() || name.size() > kMaxSlotNameLength)
        return false;
    return std::all_of(name.cbegin(), name.cend(), [](QChar c) {
        return (c >= QLatin1Char('A') && c <= QLatin1Char('Z')) || (c >= QLatin1Char('0') && c <= QLatin1Char('9'));
    });
}

bool isValidUsername(const QString &name)
{
    if (name.size() < kMinUsernameLength || name.size() > kMaxUsernameLength)
        return false;
    return std::all_of(name.cbegin(), name.cend(), [](QChar c) {
        return (c >= QLatin1Char('a') && c <= QLatin1Char('z')) || (c >= QLatin1Char('0') && c <= QLatin1Char('9'))
               || c == QLatin1Char('.') || c == QLatin1Char('_') || c == QLatin1Char('-');
    });
}

// PBKDF2-HMAC-SHA256 with one 32-byte output block, built on Qt's HMAC so no extra library is needed.
QByteArray deriveKey(const QByteArray &password, const QByteArray &salt, int iterations)
{
    QByteArray block = salt;
    block.append(QByteArray::fromHex("00000001"));
    QByteArray u = QMessageAuthenticationCode::hash(block, password, QCryptographicHash::Sha256);
    QByteArray result = u;
    for (int i = 1; i < iterations; ++i) {
        u = QMessageAuthenticationCode::hash(u, password, QCryptographicHash::Sha256);
        for (qsizetype k = 0; k < result.size(); ++k)
            result[k] = static_cast<char>(result[k] ^ u[k]);
    }
    return result;
}

QByteArray randomSalt()
{
    QByteArray salt(kSaltBytes, Qt::Uninitialized);
    for (qsizetype i = 0; i < salt.size(); ++i)
        salt[i] = static_cast<char>(QRandomGenerator::global()->generate() & 0xFF);
    return salt;
}

// Compares without stopping at the first difference, so timing does not reveal the hash.
bool sameBytes(const QByteArray &a, const QByteArray &b)
{
    if (a.size() != b.size())
        return false;
    unsigned char difference = 0;
    for (qsizetype i = 0; i < a.size(); ++i)
        difference |= static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]);
    return difference == 0;
}

} // namespace

ParkingRepository::ParkingRepository(QString databasePath) : m_databasePath(std::move(databasePath))
{}

ParkingRepository::~ParkingRepository()
{
    {
        QSqlDatabase db = QSqlDatabase::database(kConnectionName, false);
        if (db.isOpen())
            db.close();
    }
    QSqlDatabase::removeDatabase(kConnectionName);
}

std::expected<void, QString> ParkingRepository::open()
{
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), kConnectionName);
    db.setDatabaseName(m_databasePath);
    if (!db.open())
        return std::unexpected(db.lastError().text());

    QSqlQuery query(db);
    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS cppbuzz_bikeparking ("
            " Id INTEGER PRIMARY KEY AUTOINCREMENT,"
            " Slot VARCHAR(20) NOT NULL UNIQUE,"
            " Available VARCHAR(1) NOT NULL DEFAULT 'Y' CHECK (Available IN ('Y', 'N')),"
            " BookedBy VARCHAR(60) NOT NULL DEFAULT '',"
            " BookedAt VARCHAR(32) NOT NULL DEFAULT '',"
            " BookedUserId INTEGER NOT NULL DEFAULT 0)")))
        return std::unexpected(query.lastError().text());

    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS cppbuzz_users ("
            " Id INTEGER PRIMARY KEY AUTOINCREMENT,"
            " Username VARCHAR(32) NOT NULL UNIQUE COLLATE NOCASE,"
            " DisplayName VARCHAR(60) NOT NULL,"
            " Role VARCHAR(10) NOT NULL CHECK (Role IN ('admin', 'user')),"
            " PasswordHash VARCHAR(128) NOT NULL,"
            " Salt VARCHAR(64) NOT NULL,"
            " Iterations INTEGER NOT NULL)")))
        return std::unexpected(query.lastError().text());

    if (auto migrated = addMissingColumns(db); !migrated)
        return std::unexpected(migrated.error());

    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM cppbuzz_bikeparking")) || !query.next())
        return std::unexpected(query.lastError().text());
    if (query.value(0).toInt() > 0)
        return {};

    // Fresh database: create one free slot per letter, A through P.
    if (!db.transaction())
        return std::unexpected(db.lastError().text());
    QSqlQuery seed(db);
    seed.prepare(QStringLiteral("INSERT INTO cppbuzz_bikeparking (Slot, Available) VALUES (?, 'Y')"));
    for (int i = 0; i < kSlotCount; ++i) {
        seed.addBindValue(QString(QChar('A' + i)));
        if (!seed.exec()) {
            db.rollback();
            return std::unexpected(seed.lastError().text());
        }
    }
    if (!db.commit())
        return std::unexpected(db.lastError().text());
    return {};
}

std::expected<bool, QString> ParkingRepository::hasAccounts() const
{
    QSqlQuery query(QSqlDatabase::database(kConnectionName));
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM cppbuzz_users")) || !query.next())
        return std::unexpected(query.lastError().text());
    return query.value(0).toInt() > 0;
}

std::expected<void, QString> ParkingRepository::createAccount(const QString &username, const QString &displayName,
                                                              const QString &password, bool isAdministrator,
                                                              const std::optional<Account> &actor)
{
    const std::expected<bool, QString> anyAccounts = hasAccounts();
    if (!anyAccounts)
        return std::unexpected(anyAccounts.error());
    if (*anyAccounts && !(actor && actor->isAdmin))
        return std::unexpected(QStringLiteral("Only an administrator can create accounts."));

    const QString user = username.trimmed().toLower();
    if (!isValidUsername(user))
        return std::unexpected(QStringLiteral("Usernames are 3 to %1 letters, digits, dots, dashes or underscores.")
                                   .arg(kMaxUsernameLength));

    const QString name = displayName.trimmed();
    if (name.isEmpty() || name.size() > kMaxDisplayNameLength)
        return std::unexpected(QStringLiteral("Enter a name of up to %1 characters.").arg(kMaxDisplayNameLength));

    if (password.size() < kMinPasswordLength)
        return std::unexpected(QStringLiteral("Passwords need at least %1 characters.").arg(kMinPasswordLength));

    // The first account is always an administrator, so the app can never be left without one.
    const bool admin = !*anyAccounts || isAdministrator;
    const QByteArray salt = randomSalt();
    const QByteArray key = deriveKey(password.toUtf8(), salt, kPasswordIterations);

    QSqlQuery query(QSqlDatabase::database(kConnectionName));
    query.prepare(QStringLiteral(
        "INSERT INTO cppbuzz_users (Username, DisplayName, Role, PasswordHash, Salt, Iterations) "
        "VALUES (:user, :name, :role, :hash, :salt, :iterations)"));
    query.bindValue(QStringLiteral(":user"), user);
    query.bindValue(QStringLiteral(":name"), name);
    query.bindValue(QStringLiteral(":role"), admin ? QStringLiteral("admin") : QStringLiteral("user"));
    query.bindValue(QStringLiteral(":hash"), QString::fromLatin1(key.toHex()));
    query.bindValue(QStringLiteral(":salt"), QString::fromLatin1(salt.toHex()));
    query.bindValue(QStringLiteral(":iterations"), kPasswordIterations);
    if (!query.exec()) {
        if (query.lastError().text().contains(QStringLiteral("UNIQUE")))
            return std::unexpected(QStringLiteral("The username %1 is already taken.").arg(user));
        return std::unexpected(query.lastError().text());
    }
    return {};
}

std::expected<Account, QString> ParkingRepository::signIn(const QString &username, const QString &password) const
{
    const QString failure = QStringLiteral("Wrong username or password.");

    QSqlQuery query(QSqlDatabase::database(kConnectionName));
    query.prepare(QStringLiteral(
        "SELECT Id, Username, DisplayName, Role, PasswordHash, Salt, Iterations "
        "FROM cppbuzz_users WHERE Username = :user"));
    query.bindValue(QStringLiteral(":user"), username.trimmed().toLower());
    if (!query.exec())
        return std::unexpected(query.lastError().text());
    if (!query.next())
        return std::unexpected(failure);

    const QByteArray storedKey = QByteArray::fromHex(query.value(4).toString().toLatin1());
    const QByteArray salt = QByteArray::fromHex(query.value(5).toString().toLatin1());
    const QByteArray key = deriveKey(password.toUtf8(), salt, query.value(6).toInt());
    if (!sameBytes(key, storedKey))
        return std::unexpected(failure);

    Account account;
    account.id = query.value(0).toInt();
    account.username = query.value(1).toString();
    account.displayName = query.value(2).toString();
    account.isAdmin = query.value(3).toString() == QStringLiteral("admin");
    return account;
}

std::expected<QList<Slot>, QString> ParkingRepository::allSlots() const
{
    QSqlQuery query(QSqlDatabase::database(kConnectionName));
    if (!query.exec(QStringLiteral("SELECT Id, Slot, Available, BookedBy, BookedAt, BookedUserId "
                                   "FROM cppbuzz_bikeparking ORDER BY length(Slot), Slot")))
        return std::unexpected(query.lastError().text());

    QList<Slot> result;
    while (query.next()) {
        result.append(Slot{query.value(0).toInt(), query.value(1).toString(),
                           query.value(2).toString() == QStringLiteral("N"), query.value(3).toString(),
                           QDateTime::fromString(query.value(4).toString(), Qt::ISODate),
                           query.value(5).toInt()});
    }
    return result;
}

std::expected<void, QString> ParkingRepository::book(const QString &slotName, const Account &actor,
                                                     const QDateTime &when)
{
    if (actor.id <= 0)
        return std::unexpected(QStringLiteral("Sign in to book a slot."));

    // One conditional UPDATE: the WHERE clause checks the slot is still free, so two clients
    // can never both book it.
    QSqlQuery query(QSqlDatabase::database(kConnectionName));
    query.prepare(QStringLiteral("UPDATE cppbuzz_bikeparking "
                                 "SET Available = 'N', BookedBy = :owner, BookedAt = :when, BookedUserId = :uid "
                                 "WHERE Slot = :slot AND Available = 'Y'"));
    query.bindValue(QStringLiteral(":owner"), actor.displayName.left(kMaxOwnerLength));
    query.bindValue(QStringLiteral(":when"), timestamp(when));
    query.bindValue(QStringLiteral(":uid"), actor.id);
    query.bindValue(QStringLiteral(":slot"), slotName);
    return requireOneRow(query, QStringLiteral("Slot %1 is not available. It may already be booked.").arg(slotName));
}

std::expected<void, QString> ParkingRepository::cancel(const QString &slotName, const Account &actor)
{
    if (actor.id <= 0)
        return std::unexpected(QStringLiteral("Sign in to release a booking."));

    // Members can only release their own bookings; administrators can release any.
    QSqlQuery query(QSqlDatabase::database(kConnectionName));
    query.prepare(QStringLiteral("UPDATE cppbuzz_bikeparking "
                                 "SET Available = 'Y', BookedBy = '', BookedAt = '', BookedUserId = 0 "
                                 "WHERE Slot = :slot AND Available = 'N' AND (:admin = 1 OR BookedUserId = :uid)"));
    query.bindValue(QStringLiteral(":slot"), slotName);
    query.bindValue(QStringLiteral(":admin"), actor.isAdmin ? 1 : 0);
    query.bindValue(QStringLiteral(":uid"), actor.id);
    return requireOneRow(query, QStringLiteral("Slot %1 is not booked, or it was booked by someone else.")
                                    .arg(slotName));
}

std::expected<int, QString> ParkingRepository::releaseExpired(const QDateTime &now, int maxHours)
{
    // Bookings with no timestamp (made before times were recorded) never expire.
    QSqlQuery query(QSqlDatabase::database(kConnectionName));
    query.prepare(QStringLiteral("UPDATE cppbuzz_bikeparking "
                                 "SET Available = 'Y', BookedBy = '', BookedAt = '', BookedUserId = 0 "
                                 "WHERE Available = 'N' AND BookedAt <> '' AND BookedAt < :cutoff"));
    query.bindValue(QStringLiteral(":cutoff"), timestamp(now.addSecs(-static_cast<qint64>(maxHours) * 3600)));
    if (!query.exec())
        return std::unexpected(query.lastError().text());
    return static_cast<int>(query.numRowsAffected());
}

std::expected<void, QString> ParkingRepository::addSlot(const QString &slotName, const Account &actor)
{
    if (!actor.isAdmin)
        return std::unexpected(QStringLiteral("Only administrators can add slots."));

    const QString name = slotName.trimmed().toUpper();
    if (!isValidSlotName(name))
        return std::unexpected(QStringLiteral("Slot names are 1 to %1 letters or digits, such as B or 12.")
                                   .arg(kMaxSlotNameLength));

    QSqlQuery query(QSqlDatabase::database(kConnectionName));
    query.prepare(QStringLiteral("INSERT INTO cppbuzz_bikeparking (Slot, Available) VALUES (:slot, 'Y')"));
    query.bindValue(QStringLiteral(":slot"), name);
    if (!query.exec()) {
        if (query.lastError().text().contains(QStringLiteral("UNIQUE")))
            return std::unexpected(QStringLiteral("Slot %1 already exists.").arg(name));
        return std::unexpected(query.lastError().text());
    }
    return {};
}

std::expected<void, QString> ParkingRepository::removeSlot(const QString &slotName, const Account &actor)
{
    if (!actor.isAdmin)
        return std::unexpected(QStringLiteral("Only administrators can delete slots."));

    QSqlQuery query(QSqlDatabase::database(kConnectionName));
    query.prepare(QStringLiteral("DELETE FROM cppbuzz_bikeparking WHERE Slot = :slot AND Available = 'Y'"));
    query.bindValue(QStringLiteral(":slot"), slotName);
    return requireOneRow(query, QStringLiteral("Slot %1 is booked or does not exist. Release its booking before "
                                               "deleting it.")
                                    .arg(slotName));
}
