/****************************************
 *
 *   INSERT-PROJECT-NAME-HERE - INSERT-GENERIC-NAME-HERE
 *   Copyright (C) 2021 Victor Tran
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * *************************************/
#include "useraccount.h"

#include "fido2.h"
#include "passwordreset.h"
#include "twofactor.h"
#include "user.h"
#include "utils.h"
#include <QCache>
#include <QSqlQuery>

struct UserAccountPrivate {
        quint64 id;

        User* user;
        TwoFactor* twoFactor;
        Fido2* fido2;

        static QCache<quint64, UserAccount> cachedAccounts;
};

QCache<quint64, UserAccount> UserAccountPrivate::cachedAccounts = QCache<quint64, UserAccount>(100);

UserAccount::UserAccount(quint64 id) :
    QObject(nullptr) {
    d = new UserAccountPrivate();
    d->id = id;

    d->user = new User(this);
    d->twoFactor = new TwoFactor(this);
    d->fido2 = new Fido2(this);
    new PasswordReset(this);

    Utils::accountsBus().registerObject(this->path().path(), this);
}

UserAccount::~UserAccount() {
    delete d;
}

UserAccount* UserAccount::accountForId(quint64 id) {
    if (UserAccountPrivate::cachedAccounts.contains(id)) return UserAccountPrivate::cachedAccounts.object(id);

    // Ensure the account exists
    QSqlQuery query;
    query.prepare("SELECT COUNT(*) FROM users WHERE id=:id");
    query.bindValue(":id", id);
    query.exec();
    query.next();
    if (query.value(0) == 0) return nullptr;

    auto* account = new UserAccount(id);
    UserAccountPrivate::cachedAccounts.insert(id, account);
    return account;
}

quint64 UserAccount::id() {
    return d->id;
}

QDBusObjectPath UserAccount::path() {
    return QDBusObjectPath(QStringLiteral("/com/vicr123/accounts/User%1").arg(d->id));
}

TwoFactor* UserAccount::twoFactor() {
    return d->twoFactor;
}

Fido2* UserAccount::fido2() {
    return d->fido2;
}
User* UserAccount::user() {
    return d->user;
}
