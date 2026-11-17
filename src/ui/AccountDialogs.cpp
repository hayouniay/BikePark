#include "ui/AccountDialogs.h"

#include "core/ParkingRepository.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <utility>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

QLabel *makeMessageLabel(QWidget *parent)
{
    auto *label = new QLabel(parent);
    label->setObjectName(QStringLiteral("formMessage"));
    label->setWordWrap(true);
    return label;
}

} // namespace

// ---------------------------------------------------------------- LoginDialog

LoginDialog::LoginDialog(ParkingRepository &repository, QWidget *parent)
    : QDialog(parent), m_repository(repository)
{
    setWindowTitle(tr("Sign in · Bike Parking"));
    setModal(true);
    setMinimumWidth(360);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 22, 24, 18);
    layout->setSpacing(14);

    auto *title = new QLabel(tr("Sign in"), this);
    title->setObjectName(QStringLiteral("title"));
    auto *intro = new QLabel(tr("Members can book and release their own slots. Administrators also manage slots and accounts."), this);
    intro->setObjectName(QStringLiteral("hint"));
    intro->setWordWrap(true);
    layout->addWidget(title);
    layout->addWidget(intro);

    auto *form = new QFormLayout;
    form->setSpacing(10);
    m_username = new QLineEdit(this);
    m_username->setPlaceholderText(tr("Username"));
    m_password = new QLineEdit(this);
    m_password->setEchoMode(QLineEdit::Password);
    m_password->setPlaceholderText(tr("Password"));
    form->addRow(tr("Username"), m_username);
    form->addRow(tr("Password"), m_password);
    layout->addLayout(form);

    m_message = makeMessageLabel(this);
    layout->addWidget(m_message);

    auto *buttons = new QDialogButtonBox(this);
    auto *signIn = buttons->addButton(tr("Sign in"), QDialogButtonBox::AcceptRole);
    signIn->setObjectName(QStringLiteral("primary"));
    auto *cancel = buttons->addButton(tr("Quit"), QDialogButtonBox::RejectRole);
    cancel->setObjectName(QStringLiteral("secondary"));
    connect(buttons, &QDialogButtonBox::accepted, this, &LoginDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    m_username->setFocus();
}

void LoginDialog::accept()
{
    const auto result = m_repository.signIn(m_username->text(), m_password->text());
    if (!result) {
        m_message->setText(result.error());
        m_password->clear();
        m_password->setFocus();
        return;
    }
    m_account = *result;
    QDialog::accept();
}

// --------------------------------------------------------------- AccountDialog

AccountDialog::AccountDialog(ParkingRepository &repository, std::optional<Account> actor, QWidget *parent)
    : QDialog(parent), m_repository(repository), m_actor(std::move(actor))
{
    const bool setup = !m_actor.has_value();
    setWindowTitle(setup ? tr("Create the administrator · Bike Parking") : tr("New user account · Bike Parking"));
    setModal(true);
    setMinimumWidth(400);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 22, 24, 18);
    layout->setSpacing(14);

    auto *title = new QLabel(setup ? tr("Create the administrator") : tr("New user account"), this);
    title->setObjectName(QStringLiteral("title"));
    auto *intro = new QLabel(setup
                                 ? tr("No accounts exist yet. This first account manages slots and creates the other accounts.")
                                 : tr("Choose a username, a name shown on bookings, and a password of at least %1 characters.")
                                       .arg(kMinPasswordLength),
                             this);
    intro->setObjectName(QStringLiteral("hint"));
    intro->setWordWrap(true);
    layout->addWidget(title);
    layout->addWidget(intro);

    auto *form = new QFormLayout;
    form->setSpacing(10);
    m_username = new QLineEdit(this);
    m_username->setPlaceholderText(tr("e.g. alice"));
    m_displayName = new QLineEdit(this);
    m_displayName->setPlaceholderText(tr("Name shown on bookings"));
    m_password = new QLineEdit(this);
    m_password->setEchoMode(QLineEdit::Password);
    m_confirm = new QLineEdit(this);
    m_confirm->setEchoMode(QLineEdit::Password);
    form->addRow(tr("Username"), m_username);
    form->addRow(tr("Display name"), m_displayName);
    form->addRow(tr("Password"), m_password);
    form->addRow(tr("Repeat password"), m_confirm);
    layout->addLayout(form);

    m_administrator = new QCheckBox(tr("Administrator (can manage slots and accounts)"), this);
    m_administrator->setChecked(setup);
    m_administrator->setEnabled(!setup);
    layout->addWidget(m_administrator);

    m_message = makeMessageLabel(this);
    layout->addWidget(m_message);

    auto *buttons = new QDialogButtonBox(this);
    auto *create = buttons->addButton(setup ? tr("Create account") : tr("Create"), QDialogButtonBox::AcceptRole);
    create->setObjectName(QStringLiteral("primary"));
    auto *cancel = buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
    cancel->setObjectName(QStringLiteral("secondary"));
    connect(buttons, &QDialogButtonBox::accepted, this, &AccountDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    m_username->setFocus();
}

void AccountDialog::accept()
{
    if (m_password->text() != m_confirm->text()) {
        m_message->setText(tr("The two passwords do not match."));
        return;
    }

    const bool administrator = m_administrator->isChecked();
    const auto result = m_repository.createAccount(m_username->text(), m_displayName->text(),
                                                   m_password->text(), administrator, m_actor);
    if (!result) {
        m_message->setText(result.error());
        return;
    }
    QDialog::accept();
}
