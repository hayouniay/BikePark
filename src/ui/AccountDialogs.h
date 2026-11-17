#pragma once

#include "core/Account.h"

#include <QDialog>

#include <optional>

class ParkingRepository;
class QCheckBox;
class QLabel;
class QLineEdit;

// Sign-in form. accept() only closes the dialog when the username and password are correct.
class LoginDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(ParkingRepository &repository, QWidget *parent = nullptr);

    [[nodiscard]] Account account() const { return m_account; }

protected:
    void accept() override;

private:
    ParkingRepository &m_repository;
    QLineEdit *m_username = nullptr;
    QLineEdit *m_password = nullptr;
    QLabel *m_message = nullptr;
    Account m_account;
};

// Creates an account. With no actor it creates the first administrator (setup);
// with an actor it creates a member or, if ticked, another administrator.
class AccountDialog final : public QDialog
{
    Q_OBJECT

public:
    AccountDialog(ParkingRepository &repository, std::optional<Account> actor, QWidget *parent = nullptr);

protected:
    void accept() override;

private:
    ParkingRepository &m_repository;
    std::optional<Account> m_actor;
    QLineEdit *m_username = nullptr;
    QLineEdit *m_displayName = nullptr;
    QLineEdit *m_password = nullptr;
    QLineEdit *m_confirm = nullptr;
    QCheckBox *m_administrator = nullptr;
    QLabel *m_message = nullptr;
};
