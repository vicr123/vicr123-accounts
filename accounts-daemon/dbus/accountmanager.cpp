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
#include "accountmanager.h"

#include "fidoutils.h"
#include "logger.h"
#include "mailmessage.h"
#include "twofactor.h"
#include "user.h"
#include "useraccount.h"
#include "utils.h"
#include "validation.h"
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSqlDatabase>
#include <QSqlQuery>

#include "token-provisioning/tokenprovisioningmanager.h"

struct AccountManagerPrivate {
        TokenProvisioningManager* tokenProvisioningManager;
};

AccountManager::AccountManager() :
    QObject(nullptr) {
    d = new AccountManagerPrivate();
    d->tokenProvisioningManager = new TokenProvisioningManager(this);

    if (!Utils::accountsBus().registerObject("/com/vicr123/accounts", this, QDBusConnection::ExportScriptableContents)) {
        Logger::error() << "Could not register object on bus";
    }
}

AccountManager::~AccountManager() {
    delete d;
}

quint64 AccountManager::userIdByUsername(QString username) {
    QSqlQuery query;
    query.prepare("SELECT id FROM users WHERE username=:username");
    query.bindValue(":username", username);
    query.exec();
    if (!query.next()) {
        return 0;
    }

    return query.value("id").toULongLong();
}

QDBusObjectPath AccountManager::CreateUser(QString username, QString password, QString email, const QDBusMessage& message) {
    if (username.isEmpty() || password.isEmpty() || email.isEmpty()) {
        Utils::sendDbusError(Utils::InvalidInput, message);
        return QDBusObjectPath("/");
    }

    if (!Validation::validateUsername(username)) {
        Utils::sendDbusError(Utils::InvalidInput, message);
        return QDBusObjectPath("/");
    }

    if (!Validation::validatePassword(password)) {
        Utils::sendDbusError(Utils::InvalidInput, message);
        return QDBusObjectPath("/");
    }

    if (!Validation::validateEmailAddress(email)) {
        Utils::sendDbusError(Utils::InvalidInput, message);
        return QDBusObjectPath("/");
    }

    QSqlQuery query;
    query.prepare("INSERT INTO users(username, password, email) VALUES(:username, :password, :email) RETURNING id");
    query.bindValue(":username", username);
    query.bindValue(":password", Utils::generateHashedPassword(password));
    query.bindValue(":email", email);
    if (!query.exec()) {
        Utils::sendDbusError(Utils::QueryError, message);
        return QDBusObjectPath("/");
    }

    query.next();
    quint64 id = query.value(0).toULongLong();

    // Ignore the return value here: if the email doesn't get through they can request a new one later
    Utils::sendVerificationEmail(id);

    UserAccount* account = UserAccount::accountForId(id);
    if (!account) {
        Utils::sendDbusError(Utils::InternalError, message);
        return QDBusObjectPath("/");
    }

    return account->path();
}

QDBusObjectPath AccountManager::UserById(quint64 id, const QDBusMessage& message) {
    UserAccount* account = UserAccount::accountForId(id);
    if (!account) {
        Utils::sendDbusError(Utils::NoAccount, message);
        return QDBusObjectPath("/");
    }

    return account->path();
}

quint64 AccountManager::UserIdByUsername(QString username, const QDBusMessage& message) {
    quint64 id = userIdByUsername(username);
    if (id == 0) {
        Utils::sendDbusError(Utils::NoAccount, message);
        return 0;
    }

    return id;
}

QString AccountManager::ProvisionToken(QString username, QString password, QString application, QVariantMap extraOptions, const QDBusMessage& message) {
    QVariantMap options;
    options.insert("username", username);
    options.insert("password", password);
    options.insert(extraOptions);

    auto [result, error] = d->tokenProvisioningManager->provision("password", TokenProvisioningManager::TokenProvisioningPurpose::LoginToken, application, options);
    if (error != Utils::DBusError::NoError) {
        Utils::sendDbusError(error, message);
        return "";
    }

    return result.value("token").toString();
}

QString AccountManager::ForceProvisionToken(quint64 userId, QString application, const QDBusMessage& message) {
    if (application.isEmpty()) {
        Utils::sendDbusError(Utils::InvalidInput, message);
        return 0;
    }

    UserAccount* account = UserAccount::accountForId(userId);
    if (!account) {
        Utils::sendDbusError(Utils::NoAccount, message);
        return 0;
    }

    QString newToken = Utils::generateSalt().toBase64();

    QSqlQuery tokenInsertQuery;
    tokenInsertQuery.prepare("INSERT INTO tokens(userid, token, application) VALUES(:id, :token, :application)");
    tokenInsertQuery.bindValue(":id", userId);
    tokenInsertQuery.bindValue(":token", newToken);
    tokenInsertQuery.bindValue(":application", application);
    if (!tokenInsertQuery.exec()) {
        Utils::sendDbusError(Utils::QueryError, message);
        return 0;
    }

    return newToken;
}

QDBusObjectPath AccountManager::UserForToken(QString token, const QDBusMessage& message) {
    quint64 tokenUser;
    TokenProvisioningManager::TokenProvisioningPurpose tokenPurpose;
    auto ok = d->tokenProvisioningManager->verifyToken(token, &tokenUser, &tokenPurpose);
    if (!ok) {
        Utils::sendDbusError(Utils::NoAccount, message);
        return QDBusObjectPath("/");
    }

    if (tokenPurpose != TokenProvisioningManager::TokenProvisioningPurpose::LoginToken) {
        Utils::sendDbusError(Utils::NoAccount, message);
        return QDBusObjectPath("/");
    }

    return UserById(tokenUser, message);
}

QDBusObjectPath AccountManager::UserForTokenWithPurpose(QString token, QString expectedTokenPurpose, const QDBusMessage& message) {
    quint64 tokenUser;
    TokenProvisioningManager::TokenProvisioningPurpose tokenPurpose;
    auto ok = d->tokenProvisioningManager->verifyToken(token, &tokenUser, &tokenPurpose);
    if (!ok) {
        Utils::sendDbusError(Utils::NoAccount, message);
        return QDBusObjectPath("/");
    }

    if (tokenPurpose != d->tokenProvisioningManager->purposeForString(expectedTokenPurpose)) {
        Utils::sendDbusError(Utils::NoAccount, message);
        return QDBusObjectPath("/");
    }

    return UserById(tokenUser, message);
}

QList<quint64> AccountManager::AllUsers(const QDBusMessage& message) {
    QSqlQuery query;
    query.prepare("SELECT * FROM users");

    if (!query.exec()) {
        Utils::sendDbusError(Utils::QueryError, message);
        return {};
    }

    QList<quint64> users;
    while (query.next()) {
        users.append(query.value("id").toULongLong());
    }
    return users;
}

QStringList AccountManager::TokenProvisioningMethods(QString username, QString application, const QDBusMessage& message) {
    const quint64 id = userIdByUsername(username);
    if (id == 0) {
        Utils::sendDbusError(Utils::NoAccount, message);
        return {};
    }

    QSqlQuery userQuery;
    userQuery.prepare("SELECT * FROM users WHERE id=:id");
    userQuery.bindValue(":id", id);
    userQuery.exec();
    userQuery.next();

    // Ensure the account is not disabled
    QString passwordHash = userQuery.value("password").toString();
    if (passwordHash.startsWith("!")) {
        Utils::sendDbusError(Utils::DisabledAccount, message);
        return {};
    }

    return d->tokenProvisioningManager->availableMethods(id, application, TokenProvisioningManager::TokenProvisioningPurpose::LoginToken);
}

QStringList AccountManager::TokenProvisioningMethodsWithPurpose(QString username, QString purpose, QString application, const QDBusMessage& message) {
    const quint64 id = userIdByUsername(username);
    if (id == 0) {
        Utils::sendDbusError(Utils::NoAccount, message);
        return {};
    }

    QSqlQuery userQuery;
    userQuery.prepare("SELECT * FROM users WHERE id=:id");
    userQuery.bindValue(":id", id);
    userQuery.exec();
    userQuery.next();

    // Ensure the account is not disabled
    QString passwordHash = userQuery.value("password").toString();
    if (passwordHash.startsWith("!")) {
        Utils::sendDbusError(Utils::DisabledAccount, message);
        return {};
    }

    return d->tokenProvisioningManager->availableMethods(id, application, d->tokenProvisioningManager->purposeForString(purpose));
}

QVariantMap AccountManager::ProvisionTokenByMethod(QString method, QString username, QString application, QVariantMap extraOptions, const QDBusMessage& message) {
    QVariantMap options;
    options.insert("username", username);
    options.insert("application", application);
    options.insert(extraOptions);

    auto [result, error] = d->tokenProvisioningManager->provision(method, d->tokenProvisioningManager->purposeForString(extraOptions.value("purpose", "login").toString()), application, options);
    if (error != Utils::DBusError::NoError) {
        Utils::sendDbusError(error, message);
        return {};
    }

    return result;
}

QDBusObjectPath AccountManager::CreateMailMessage(const QString& to, const QDBusMessage& message) {
    auto* mailMessage = new MailMessage(to);
    return mailMessage->path();
}
