#include "core/ParkingRepository.h"

#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

#include <expected>
#include <optional>

namespace {

// Reports the error message (if any) and returns whether the call succeeded.
template <typename T>
bool succeeded(const std::expected<T, QString> &result)
{
    if (!result)
        qWarning("%s", qPrintable(result.error()));
    return result.has_value();
}

Slot findSlot(const QList<Slot> &rows, const QString &name)
{
    for (const Slot &slot : rows) {
        if (slot.name == name)
            return slot;
    }
    return {};
}

const QDateTime kNoon = QDateTime::fromString(QStringLiteral("2026-11-16T12:00:00Z"), Qt::ISODate);

// Creates the administrator, Alice and Bob. Returns false if any step failed.
bool createUsers(ParkingRepository &repo)
{
    if (!succeeded(repo.createAccount(QStringLiteral("admin"), QStringLiteral("Admin"),
                                      QStringLiteral("adminpass1"), true, std::nullopt)))
        return false;
    const auto admin = repo.signIn(QStringLiteral("admin"), QStringLiteral("adminpass1"));
    if (!succeeded(admin))
        return false;
    return succeeded(repo.createAccount(QStringLiteral("alice"), QStringLiteral("Alice"),
                                        QStringLiteral("alicepass1"), false, *admin))
           && succeeded(repo.createAccount(QStringLiteral("bob"), QStringLiteral("Bob"),
                                           QStringLiteral("bobpass123"), false, *admin));
}

std::optional<Account> signedIn(ParkingRepository &repo, const QString &username, const QString &password)
{
    const auto result = repo.signIn(username, password);
    if (!result)
        return std::nullopt;
    return *result;
}

// Builds a database in the format before accounts and owner columns existed.
void createLegacyDatabase(const QString &path)
{
    const QString connection = QStringLiteral("legacy-setup");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
        db.setDatabaseName(path);
        QVERIFY(db.open());
        QSqlQuery query(db);
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE cppbuzz_bikeparking (Id INTEGER PRIMARY KEY AUTOINCREMENT,"
            " Slot VARCHAR(20) NOT NULL UNIQUE, Available VARCHAR(1) NOT NULL DEFAULT 'Y')")));
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO cppbuzz_bikeparking (Slot, Available) VALUES ('A', 'N'), ('B', 'Y')")));
        db.close();
    }
    QSqlDatabase::removeDatabase(connection);
}

} // namespace

class RepositoryTest : public QObject
{
    Q_OBJECT

private:
    // Each test gets its own database file, named after the test.
    QString databasePath() const
    {
        return m_dir.filePath(QStringLiteral("%1.sqlite").arg(QTest::currentTestFunction()));
    }

    QTemporaryDir m_dir;

private slots:
    void initTestCase() { QVERIFY(m_dir.isValid()); }

    void freshDatabaseHasSixteenFreeSlots()
    {
        ParkingRepository repo(databasePath());
        QVERIFY(succeeded(repo.open()));

        const auto rows = repo.allSlots();
        QVERIFY(succeeded(rows));
        QCOMPARE(rows->size(), qsizetype(16));
        QCOMPARE(rows->first().name, QStringLiteral("A"));
        QCOMPARE(rows->last().name, QStringLiteral("P"));
        for (const Slot &slot : *rows)
            QVERIFY(!slot.booked);
    }

    void firstAccountIsAdministratorAndLaterOnesNeedAnAdministrator()
    {
        ParkingRepository repo(databasePath());
        QVERIFY(succeeded(repo.open()));
        const auto anyBefore = repo.hasAccounts();
        QVERIFY(succeeded(anyBefore));
        QVERIFY(!*anyBefore);

        QVERIFY(createUsers(repo));
        const auto admin = signedIn(repo, QStringLiteral("admin"), QStringLiteral("adminpass1"));
        const auto alice = signedIn(repo, QStringLiteral("alice"), QStringLiteral("alicepass1"));
        QVERIFY(admin.has_value());
        QVERIFY(alice.has_value());
        QVERIFY(admin->isAdmin);
        QVERIFY(!alice->isAdmin);

        // A member cannot create accounts.
        QVERIFY(!repo.createAccount(QStringLiteral("carol"), QStringLiteral("Carol"),
                                    QStringLiteral("carolpass1"), false, *alice));
    }

    void accountInputsAreChecked()
    {
        ParkingRepository repo(databasePath());
        QVERIFY(succeeded(repo.open()));
        QVERIFY(createUsers(repo));
        const auto admin = signedIn(repo, QStringLiteral("admin"), QStringLiteral("adminpass1"));
        QVERIFY(admin.has_value());

        QVERIFY(!repo.createAccount(QStringLiteral("x"), QStringLiteral("Xavier"), QStringLiteral("longenough"), false, *admin));
        QVERIFY(!repo.createAccount(QStringLiteral("carol"), QStringLiteral("Carol"), QStringLiteral("short"), false, *admin));
        QVERIFY(!repo.createAccount(QStringLiteral("alice"), QStringLiteral("Alice 2"), QStringLiteral("longenough"), false, *admin));
        QVERIFY(!repo.createAccount(QStringLiteral("carol"), QStringLiteral(""), QStringLiteral("longenough"), false, *admin));
    }

    void signInRequiresCorrectPassword()
    {
        ParkingRepository repo(databasePath());
        QVERIFY(succeeded(repo.open()));
        QVERIFY(createUsers(repo));

        QVERIFY(!repo.signIn(QStringLiteral("alice"), QStringLiteral("wrongpass")));
        QVERIFY(!repo.signIn(QStringLiteral("nobody"), QStringLiteral("alicepass1")));
        // Usernames are not case-sensitive.
        QVERIFY(signedIn(repo, QStringLiteral("ALICE"), QStringLiteral("alicepass1")).has_value());
    }

    void bookRecordsOwnerAndTime()
    {
        ParkingRepository repo(databasePath());
        QVERIFY(succeeded(repo.open()));
        QVERIFY(createUsers(repo));
        const auto alice = signedIn(repo, QStringLiteral("alice"), QStringLiteral("alicepass1"));
        QVERIFY(alice.has_value());

        QVERIFY(succeeded(repo.book(QStringLiteral("B"), *alice, kNoon)));
        const auto rows = repo.allSlots();
        QVERIFY(succeeded(rows));
        const Slot slot = findSlot(*rows, QStringLiteral("B"));
        QVERIFY(slot.booked);
        QCOMPARE(slot.owner, QStringLiteral("Alice"));
        QCOMPARE(slot.ownerId, alice->id);
        QCOMPARE(slot.bookedAt, kNoon);
    }

    void bookingNeedsSignIn()
    {
        ParkingRepository repo(databasePath());
        QVERIFY(succeeded(repo.open()));
        QVERIFY(!repo.book(QStringLiteral("B"), Account{}, kNoon));
    }

    void bookingTakenSlotFails()
    {
        ParkingRepository repo(databasePath());
        QVERIFY(succeeded(repo.open()));
        QVERIFY(createUsers(repo));
        const auto alice = signedIn(repo, QStringLiteral("alice"), QStringLiteral("alicepass1"));
        const auto bob = signedIn(repo, QStringLiteral("bob"), QStringLiteral("bobpass123"));
        QVERIFY(alice.has_value() && bob.has_value());

        QVERIFY(succeeded(repo.book(QStringLiteral("B"), *alice, kNoon)));
        QVERIFY(!repo.book(QStringLiteral("B"), *bob, kNoon));
        const auto rows = repo.allSlots();
        QVERIFY(succeeded(rows));
        QCOMPARE(findSlot(*rows, QStringLiteral("B")).owner, QStringLiteral("Alice"));
    }

    void memberReleasesOnlyOwnBooking()
    {
        ParkingRepository repo(databasePath());
        QVERIFY(succeeded(repo.open()));
        QVERIFY(createUsers(repo));
        const auto alice = signedIn(repo, QStringLiteral("alice"), QStringLiteral("alicepass1"));
        const auto bob = signedIn(repo, QStringLiteral("bob"), QStringLiteral("bobpass123"));
        QVERIFY(alice.has_value() && bob.has_value());

        QVERIFY(succeeded(repo.book(QStringLiteral("B"), *alice, kNoon)));
        QVERIFY(!repo.cancel(QStringLiteral("B"), *bob));
        QVERIFY(succeeded(repo.cancel(QStringLiteral("B"), *alice)));
    }

    void administratorReleasesAnyBooking()
    {
        ParkingRepository repo(databasePath());
        QVERIFY(succeeded(repo.open()));
        QVERIFY(createUsers(repo));
        const auto alice = signedIn(repo, QStringLiteral("alice"), QStringLiteral("alicepass1"));
        const auto admin = signedIn(repo, QStringLiteral("admin"), QStringLiteral("adminpass1"));
        QVERIFY(alice.has_value() && admin.has_value());

        QVERIFY(succeeded(repo.book(QStringLiteral("B"), *alice, kNoon)));
        QVERIFY(succeeded(repo.cancel(QStringLiteral("B"), *admin)));
    }

    void cancellingFreeSlotFails()
    {
        ParkingRepository repo(databasePath());
        QVERIFY(succeeded(repo.open()));
        QVERIFY(createUsers(repo));
        const auto admin = signedIn(repo, QStringLiteral("admin"), QStringLiteral("adminpass1"));
        QVERIFY(admin.has_value());
        QVERIFY(!repo.cancel(QStringLiteral("C"), *admin));
    }

    void releaseExpiredReleasesOnlyOldBookings()
    {
        ParkingRepository repo(databasePath());
        QVERIFY(succeeded(repo.open()));
        QVERIFY(createUsers(repo));
        const auto alice = signedIn(repo, QStringLiteral("alice"), QStringLiteral("alicepass1"));
        QVERIFY(alice.has_value());

        QVERIFY(succeeded(repo.book(QStringLiteral("C"), *alice, kNoon.addDays(-1))));
        QVERIFY(succeeded(repo.book(QStringLiteral("D"), *alice, kNoon.addSecs(-3600))));

        const auto released = repo.releaseExpired(kNoon, 12);
        QVERIFY(succeeded(released));
        QCOMPARE(*released, 1);

        const auto rows = repo.allSlots();
        QVERIFY(succeeded(rows));
        QVERIFY(!findSlot(*rows, QStringLiteral("C")).booked);
        QVERIFY(findSlot(*rows, QStringLiteral("D")).booked);
    }

    void olderDatabaseIsMigratedAndKeepsBookings()
    {
        const QString path = databasePath();
        createLegacyDatabase(path);

        ParkingRepository repo(path);
        QVERIFY(succeeded(repo.open()));
        QVERIFY(createUsers(repo));

        const auto rows = repo.allSlots();
        QVERIFY(succeeded(rows));
        const Slot legacy = findSlot(*rows, QStringLiteral("A"));
        QVERIFY(legacy.booked);
        QVERIFY(legacy.owner.isEmpty());
        QCOMPARE(legacy.ownerId, 0);
        QVERIFY(!legacy.bookedAt.isValid());

        // Legacy bookings have no time, so the time limit never releases them.
        const auto released = repo.releaseExpired(kNoon, 12);
        QVERIFY(succeeded(released));
        QCOMPARE(*released, 0);

        // No member can release a booking without an owner; an administrator can.
        const auto alice = signedIn(repo, QStringLiteral("alice"), QStringLiteral("alicepass1"));
        const auto admin = signedIn(repo, QStringLiteral("admin"), QStringLiteral("adminpass1"));
        QVERIFY(alice.has_value() && admin.has_value());
        QVERIFY(!repo.cancel(QStringLiteral("A"), *alice));
        QVERIFY(succeeded(repo.cancel(QStringLiteral("A"), *admin)));
    }

    void addSlotIsAdministratorOnly()
    {
        ParkingRepository repo(databasePath());
        QVERIFY(succeeded(repo.open()));
        QVERIFY(createUsers(repo));
        const auto alice = signedIn(repo, QStringLiteral("alice"), QStringLiteral("alicepass1"));
        const auto admin = signedIn(repo, QStringLiteral("admin"), QStringLiteral("adminpass1"));
        QVERIFY(alice.has_value() && admin.has_value());

        QVERIFY(!repo.addSlot(QStringLiteral("Q"), *alice));
        QVERIFY(succeeded(repo.addSlot(QStringLiteral(" q1 "), *admin)));

        const auto rows = repo.allSlots();
        QVERIFY(succeeded(rows));
        QCOMPARE(rows->size(), qsizetype(17));
        QVERIFY(findSlot(*rows, QStringLiteral("Q1")).id > 0);
        QVERIFY(!repo.addSlot(QStringLiteral("Q1"), *admin));
    }

    void addSlotRejectsInvalidNames()
    {
        ParkingRepository repo(databasePath());
        QVERIFY(succeeded(repo.open()));
        QVERIFY(createUsers(repo));
        const auto admin = signedIn(repo, QStringLiteral("admin"), QStringLiteral("adminpass1"));
        QVERIFY(admin.has_value());

        QVERIFY(!repo.addSlot(QStringLiteral(""), *admin));
        QVERIFY(!repo.addSlot(QStringLiteral("ABCDEFG"), *admin));
        QVERIFY(!repo.addSlot(QStringLiteral("A-B"), *admin));
        QVERIFY(!repo.addSlot(QStringLiteral("A B"), *admin));
    }

    void removeSlotRules()
    {
        ParkingRepository repo(databasePath());
        QVERIFY(succeeded(repo.open()));
        QVERIFY(createUsers(repo));
        const auto alice = signedIn(repo, QStringLiteral("alice"), QStringLiteral("alicepass1"));
        const auto admin = signedIn(repo, QStringLiteral("admin"), QStringLiteral("adminpass1"));
        QVERIFY(alice.has_value() && admin.has_value());

        QVERIFY(succeeded(repo.addSlot(QStringLiteral("Z"), *admin)));
        QVERIFY(!repo.removeSlot(QStringLiteral("Z"), *alice));       // members cannot delete
        QVERIFY(succeeded(repo.removeSlot(QStringLiteral("Z"), *admin)));

        const auto rows = repo.allSlots();
        QVERIFY(succeeded(rows));
        QCOMPARE(rows->size(), qsizetype(16));

        QVERIFY(succeeded(repo.book(QStringLiteral("B"), *alice, kNoon)));
        QVERIFY(!repo.removeSlot(QStringLiteral("B"), *admin));       // booked slots cannot be deleted
        QVERIFY(!repo.removeSlot(QStringLiteral("NOPE"), *admin));
    }
};

QTEST_GUILESS_MAIN(RepositoryTest)
#include "tst_repository.moc"
