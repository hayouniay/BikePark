#include "core/ParkingRepository.h"
#include "ui/AccountDialogs.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIODevice>
#include <QMessageBox>
#include <QStandardPaths>
#include <QStringList>
#include <QStyleHints>

#include <cstdlib>
#include <optional>

namespace {

// On first use of a database path, copy the database bundled with the original project so
// existing bookings are kept. After that, the file at that path is used as it is.
void seedDatabaseIfMissing(const QString &path)
{
    if (QFile::exists(path))
        return;

    QFile source(QStringLiteral(":/data/cppbuzz_bikeparking.sqlite"));
    QFile target(path);
    if (source.open(QIODevice::ReadOnly) && target.open(QIODevice::WriteOnly))
        target.write(source.readAll());
}

// Loads the light or dark stylesheet to match the system setting.
void applyStyle(QApplication &app)
{
    const bool dark = QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    QFile styleFile(dark ? QStringLiteral(":/style/style_dark.qss") : QStringLiteral(":/style/style.qss"));
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text))
        app.setStyleSheet(QString::fromUtf8(styleFile.readAll()));
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("cppbuzz"));
    QApplication::setApplicationName(QStringLiteral("BikeParking"));
    QApplication::setApplicationDisplayName(QStringLiteral("Bike Parking"));
    QApplication::setApplicationVersion(QStringLiteral(BIKEPARKING_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Book and release bike parking slots."));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption databaseOption(
        QStringList{QStringLiteral("d"), QStringLiteral("database")},
        QStringLiteral("Use the SQLite file at <path>, for example one on a shared drive so "
                       "several users see the same slots."),
        QStringLiteral("path"));
    parser.addOption(databaseOption);
    parser.process(app);

    QString dbPath;
    if (parser.isSet(databaseOption)) {
        dbPath = QFileInfo(parser.value(databaseOption)).absoluteFilePath();
    } else {
        const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        dbPath = dataDir + QStringLiteral("/cppbuzz_bikeparking.sqlite");
    }
    QDir().mkpath(QFileInfo(dbPath).absolutePath());
    seedDatabaseIfMissing(dbPath);

    applyStyle(app);
    QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, &app,
                     [&app] { applyStyle(app); });

    ParkingRepository repository(dbPath);
    if (const auto opened = repository.open(); !opened) {
        QMessageBox::critical(nullptr, QStringLiteral("Bike Parking"),
                              QStringLiteral("Cannot open the database:\n%1\n\n%2").arg(dbPath, opened.error()));
        return EXIT_FAILURE;
    }

    const auto anyAccounts = repository.hasAccounts();
    if (!anyAccounts) {
        QMessageBox::critical(nullptr, QStringLiteral("Bike Parking"),
                              QStringLiteral("Cannot read accounts:\n%1").arg(anyAccounts.error()));
        return EXIT_FAILURE;
    }

    // First run: create the administrator before anyone can sign in.
    if (!*anyAccounts) {
        AccountDialog setup(repository, std::nullopt);
        if (setup.exec() != QDialog::Accepted)
            return EXIT_SUCCESS;
    }

    LoginDialog login(repository);
    if (login.exec() != QDialog::Accepted)
        return EXIT_SUCCESS;

    MainWindow window(repository, login.account());
    window.show();
    return app.exec();
}
