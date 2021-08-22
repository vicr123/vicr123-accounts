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

#include <QSqlQuery>
#include <QSqlDatabase>
#include "logger.h"
#include "utils.h"
#include "useraccount.h"

struct AccountManagerPrivate {

};

AccountManager::AccountManager() : QObject(nullptr) {
    d = new AccountManagerPrivate();

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


    Utils::sendTemplateEmail("verify", {email}, "en", {
        {"name", username},
        {"code", "583827"} //TODO
    });

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
    quint64 id = userIdByUsername(username);
    if (id == 0) {
        Utils::sendDbusError(Utils::NoAccount, message);
        return 0;
    }

    QSqlQuery userQuery;
    userQuery.prepare("SELECT * FROM users WHERE id=:id");
    userQuery.bindValue(":id", id);
    userQuery.exec();
    userQuery.next();

    //TODO: Check password resets

    //Ensure the password is correct
    QString passwordHash = userQuery.value("password").toString();
    if (passwordHash == "x") {
        Utils::sendDbusError(Utils::PasswordResetRequired, message);
        return 0;
    }
    if (passwordHash.startsWith("!")) {
        Utils::sendDbusError(Utils::DisabledAccount, message);
        return 0;
    }

    if (!Utils::verifyHashedPassword(password, passwordHash)) {
        Utils::sendDbusError(Utils::IncorrectPassword, message);
        return 0;
    }

    //Check TOTP
    QSqlQuery otpQuery;
    otpQuery.prepare("SELECT * FROM otp WHERE userid=:id");
    otpQuery.bindValue(":id", id);
    if (!otpQuery.exec()) {
        Utils::sendDbusError(Utils::QueryError, message);
        return 0;
    }

    if (otpQuery.next()) {
        if (otpQuery.value("enabled").toBool()) {
            if (!extraOptions.contains("otp")) {
                Utils::sendDbusError(Utils::TwoFactorRequired, message);
                return 0;
            }

            QString otpSecret = otpQuery.value("otpkey").toString();
            QString otpKey = extraOptions.value("otp").toString();
            if (!Utils::isValidOtpKey(otpKey, otpSecret)) {
                //Check the backup keys
                QSqlQuery backupsQuery;
                backupsQuery.prepare("SELECT * FROM otpbackup WHERE userid=:id");
                backupsQuery.bindValue(":id", id);
                backupsQuery.exec();

                bool foundValidBackupKey = false;
                while (backupsQuery.next()) {
                    QString backupKey = backupsQuery.value("backupkey").toString();
                    if (!backupsQuery.value("used").toBool() && backupKey == otpKey) {
                        QSqlQuery updateBackupQuery;
                        updateBackupQuery.prepare("UPDATE otpbackup SET used=:used WHERE userid=:id AND backupkey=:key");
                        updateBackupQuery.bindValue(":used", true);
                        updateBackupQuery.bindValue(":id", id);
                        updateBackupQuery.bindValue(":key", backupKey);
                        if (!updateBackupQuery.exec()) {
                            Utils::sendDbusError(Utils::QueryError, message);
                            return 0;
                        }

                        foundValidBackupKey = true;
                    }
                }

                if (!foundValidBackupKey) {
                    Utils::sendDbusError(Utils::TwoFactorRequired, message);
                    return 0;
                }
            }
        }
    }

    QString newToken = Utils::generateSalt().toBase64();

    QSqlQuery tokenInsertQuery;
    tokenInsertQuery.prepare("INSERT INTO tokens(userid, token, application) VALUES(:id, :token, :application)");
    tokenInsertQuery.bindValue(":id", id);
    tokenInsertQuery.bindValue(":token", newToken);
    tokenInsertQuery.bindValue(":application", application);
    if (!tokenInsertQuery.exec()) {
        Utils::sendDbusError(Utils::QueryError, message);
        return 0;
    }

    return newToken;
}

QDBusObjectPath AccountManager::UserForToken(QString token, const QDBusMessage& message) {
    QSqlQuery query;
    query.prepare("SELECT * FROM tokens WHERE token=:token");
    query.bindValue(":token", token);
    query.exec();

    if (!query.next()) {
        Utils::sendDbusError(Utils::NoAccount, message);
        return QDBusObjectPath("/");
    }

    quint64 id = query.value("userid").toULongLong();
    return UserById(id, message);
}
