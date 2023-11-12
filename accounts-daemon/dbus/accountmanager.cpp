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
#include "useraccount.h"
#include "utils.h"
#include "validation.h"
#include "user.h"
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSqlDatabase>
#include <QSqlQuery>

#include "vicr123-accounts-fido/fido.h"

struct AccountManagerPrivate {
        Fido fido;
};

AccountManager::AccountManager() :
    QObject(nullptr) {
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
    if (username.isEmpty() || password.isEmpty() || application.isEmpty()) {
        Utils::sendDbusError(Utils::InvalidInput, message);
        return 0;
    }

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

    // Ensure the password is correct
    QString passwordHash = userQuery.value("password").toString();
    if (passwordHash.startsWith("!")) {
        Utils::sendDbusError(Utils::DisabledAccount, message);
        return 0;
    }

    auto* account = UserAccount::accountForId(id);

    // Now check for password resets
    bool havePasswordReset = false;
    {
        QSqlQuery resetQuery;
        resetQuery.prepare("SELECT * FROM passwordresets WHERE userid=:id");
        resetQuery.bindValue(":id", id);
        resetQuery.exec();
        while (resetQuery.next()) {
            if (resetQuery.value("expiry").toLongLong() > QDateTime::currentMSecsSinceEpoch()) {
                havePasswordReset = true;
                QString temporaryPassword = resetQuery.value("temporarypassword").toString();
                if (Utils::verifyHashedPassword(password, temporaryPassword)) {
                    if (!extraOptions.contains("newPassword")) {
                        Utils::sendDbusError(Utils::PasswordResetRequired, message);
                        return 0;
                    }

                    // Set the new password on this user account
                    QString newPassword = extraOptions.value("newPassword").toString();
                    auto error = account->user()->setPassword(newPassword);
                    if (error) {
                        Utils::sendDbusError(error, message);
                        return 0;
                    }

                    QSqlQuery deleteQuery;
                    deleteQuery.prepare("DELETE FROM passwordresets WHERE userid=:id");
                    deleteQuery.bindValue(":id", id);
                    deleteQuery.exec();

                    password = newPassword;
                }
            }
        }
    }

    if (passwordHash == "x") {
        // Check if there is already a pending password reset.
        // If there is already a pending password reset, tell the user that their password is incorrect instead.
        if (havePasswordReset) {
            Utils::sendDbusError(Utils::IncorrectPassword, message);
        } else {
            Utils::sendDbusError(Utils::PasswordResetRequestRequired, message);
        }
        return 0;
    }

    if (!Utils::verifyHashedPassword(password, passwordHash)) {
        Utils::sendDbusError(Utils::IncorrectPassword, message);
        return 0;
    }

    // Check TOTP
    QSqlQuery otpQuery;
    otpQuery.prepare("SELECT * FROM otp WHERE userid=:id");
    otpQuery.bindValue(":id", id);
    if (!otpQuery.exec()) {
        Utils::sendDbusError(Utils::QueryError, message);
        return 0;
    }

    if (otpQuery.next()) {
        if (otpQuery.value("enabled").toBool()) {
            if (!extraOptions.contains("otpToken")) {
                Utils::sendDbusError(Utils::TwoFactorRequired, message);
                return 0;
            }

            QString otpSecret = otpQuery.value("otpkey").toString();
            QString otpKey = extraOptions.value("otpToken").toString();
            if (!Utils::isValidOtpKey(otpKey, otpSecret)) {
                // Check the backup keys
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

                        UserAccount* account = UserAccount::accountForId(id);
                        account->twoFactor()->reloadBackupKeys();

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
    quint64 id = userIdByUsername(username);
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

    QStringList methods;
    methods.append("password");

    //Check if FIDO is set up
    QSqlQuery fidoQuery;
    fidoQuery.prepare("SELECT COUNT(*) FROM fido WHERE application=:application AND userid=:userid");
    fidoQuery.bindValue(":application", application);
    fidoQuery.bindValue(":userid", id);
    if (!fidoQuery.exec()) {
        Utils::sendDbusError(Utils::QueryError, message);
        return {};
    }

    fidoQuery.next();
    auto count = fidoQuery.value(0).toInt();
    if (count > 0) {
        methods.append("fido");
    }

    return methods;
}

QVariantMap AccountManager::ProvisionTokenByMethod(QString method, QString username, QString application, QVariantMap extraOptions, const QDBusMessage& message) {
    message.setDelayedReply(true);
    this->privateProvisionTokenByMethod(method, username, application, extraOptions, message).then([message](QVariantMap result) {
        if (!result.isEmpty()) {
            Utils::accountsBus().send(message.createReply(result));
        }
    });
    return {};
}

QDBusObjectPath AccountManager::CreateMailMessage(const QString& to, const QDBusMessage& message) {
    auto* mailMessage = new MailMessage(to);
    return mailMessage->path();
}

QCoro::Task<QVariantMap> AccountManager::privateProvisionTokenByMethod(QString method, QString username, QString application, QVariantMap extraOptions, const QDBusMessage& message) {
    quint64 id = userIdByUsername(username);
    if (id == 0) {
        Utils::sendDbusError(Utils::NoAccount, message);
        co_return {};
    }

    auto availableMethods = TokenProvisioningMethods(username, application, message);
    if (!availableMethods.contains(method)) {
        Utils::sendDbusError(Utils::InvalidInput, message);
        co_return {};
    }

    if (method == "password") {
        if (!extraOptions.contains("password") ) {
            Utils::sendDbusError(Utils::InvalidInput, message);
            co_return {};
        }

        auto token = ProvisionToken(username, extraOptions.value("password").toString(), application, extraOptions, message);
        co_return QVariantMap({
            {"token", token}
        });
    } else if (method == "fido") {
        if (extraOptions.contains("response")) {
            if (!extraOptions.contains("response") && !extraOptions.contains("pregetOptions") && !extraOptions.contains("expectOrigins")) {
                Utils::sendDbusError(Utils::InvalidInput, message);
                co_return {};
            }
            QJsonObject payload;
            payload.insert("existingCreds", QJsonArray::fromStringList(FidoUtils::FidoCredsForUser(id, application)));
            payload.insert("extraOrigins", QJsonArray::fromStringList(extraOptions.value("expectOrigins").toStringList()));
            payload.insert("response", extraOptions.value("response").toString());
            payload.insert("pregetOptions", extraOptions.value("pregetOptions").toString());

            auto preget = co_await d->fido.preGet(extraOptions.value("rpname").toString(), extraOptions.value("rpid").toString(), QJsonDocument(payload).toJson());
            auto output = QJsonDocument::fromJson(preget.toUtf8()).object();

            auto usedCred = output.value("usedCred").toString().toUtf8();
            auto newCred = output.value("newCred").toString().toUtf8();

            QSqlQuery fidoQuery;
            fidoQuery.prepare("UPDATE fido SET data=:newCred WHERE data=:oldCred AND userid=:id");
            fidoQuery.bindValue(":newCred", newCred);
            fidoQuery.bindValue(":oldCred", usedCred);
            fidoQuery.bindValue(":id", id);
            if (!fidoQuery.exec()) {
                Utils::sendDbusError(Utils::QueryError, message);
                co_return {};
            }

            //Provision a token
            auto token = ForceProvisionToken(id, application, message);
            co_return QVariantMap({
                {"token", token}
            });
        } else {
            if (!extraOptions.contains("rpname") && !extraOptions.contains("rpid")) {
                Utils::sendDbusError(Utils::InvalidInput, message);
                co_return {};
            }

            QJsonObject payload;
            payload.insert("existingCreds", QJsonArray::fromStringList(FidoUtils::FidoCredsForUser(id, application)));

            auto preget = co_await d->fido.preGet(extraOptions.value("rpname").toString(), extraOptions.value("rpid").toString(), QJsonDocument(payload).toJson());

            co_return QVariantMap({
                {"options", preget.toUtf8()}
            });
        }
    }

    Utils::sendDbusError(Utils::InvalidInput, message);
    co_return {};
}
