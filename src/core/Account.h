#pragma once

#include <QString>

// A signed-in user. id 0 means "nobody" (not signed in).
struct Account
{
    int id = 0;
    QString username;
    QString displayName;
    bool isAdmin = false;
};
