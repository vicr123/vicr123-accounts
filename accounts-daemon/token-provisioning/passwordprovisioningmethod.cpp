//
// Created by victor on 20/09/24.
//

#include "passwordprovisioningmethod.h"

#include "dbus/accountmanager.h"
#include "dbus/twofactor.h"
#include "dbus/user.h"
#include "dbus/useraccount.h"

#include <QDateTime>
#include <QSqlQuery>

struct PasswordProvisioningMethodPrivate {
};

PasswordProvisioningMethod::PasswordProvisioningMethod(AccountManager* parent) :
    TokenProvisioningMethod(parent) {
}

QString PasswordProvisioningMethod::tokenProvisioningMethod() const {
    return QStringLiteral("password");
}

TokenProvisioningMethod::ProvisionResult PasswordProvisioningMethod::provision(QVariantMap options, TokenProvisioningManager::TokenProvisioningPurpose provisioningPurpose) const {
    const auto username = options.value("username").toString();
    auto password = options.value("password").toString();

    if (username.isEmpty() || password.isEmpty()) {
        return {0, Utils::InvalidInput};
    }

    const auto id = accountManager()->userIdByUsername(username);
    if (id == 0) {
        return {0, Utils::NoAccount};
    }

    QSqlQuery userQuery;
    userQuery.prepare("SELECT * FROM users WHERE id=:id");
    userQuery.bindValue(":id", id);
    userQuery.exec();
    userQuery.next();

    // Ensure the password is correct
    const auto passwordHash = userQuery.value("password").toString();
    if (passwordHash.startsWith("!")) {
        return {0, Utils::DisabledAccount};
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
                const auto temporaryPassword = resetQuery.value("temporarypassword").toString();
                if (Utils::verifyHashedPassword(password, temporaryPassword)) {
                    if (!options.contains("newPassword")) {
                        return {0, Utils::PasswordResetRequired};
                    }

                    // Set the new password on this user account
                    const auto newPassword = options.value("newPassword").toString();
                    if (const auto error = account->user()->setPassword(newPassword)) {
                        return {0, error};
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
            return {0, Utils::IncorrectPassword};
        }

        return {0, Utils::PasswordResetRequired};
    }

    if (!Utils::verifyHashedPassword(password, passwordHash)) {
        return {0, Utils::IncorrectPassword};
    }

    // Check TOTP if we're doing this to log in
    if (provisioningPurpose == TokenProvisioningManager::TokenProvisioningPurpose::LoginToken) {
        QSqlQuery otpQuery;
        otpQuery.prepare("SELECT * FROM otp WHERE userid=:id");
        otpQuery.bindValue(":id", id);
        if (!otpQuery.exec()) {
            return {0, Utils::QueryError};
        }

        if (otpQuery.next()) {
            if (otpQuery.value("enabled").toBool()) {
                if (!options.contains("otpToken")) {
                    return {0, Utils::TwoFactorRequired};
                }

                const auto otpSecret = otpQuery.value("otpkey").toString();
                const auto otpKey = options.value("otpToken").toString();
                if (!Utils::isValidOtpKey(otpKey, otpSecret)) {
                    // Check the backup keys
                    QSqlQuery backupsQuery;
                    backupsQuery.prepare("SELECT * FROM otpbackup WHERE userid=:id");
                    backupsQuery.bindValue(":id", id);
                    backupsQuery.exec();

                    bool foundValidBackupKey = false;
                    while (backupsQuery.next()) {
                        auto backupKey = backupsQuery.value("backupkey").toString();
                        if (!backupsQuery.value("used").toBool() && backupKey == otpKey) {
                            QSqlQuery updateBackupQuery;
                            updateBackupQuery.prepare("UPDATE otpbackup SET used=:used WHERE userid=:id AND backupkey=:key");
                            updateBackupQuery.bindValue(":used", true);
                            updateBackupQuery.bindValue(":id", id);
                            updateBackupQuery.bindValue(":key", backupKey);
                            if (!updateBackupQuery.exec()) {
                                return {0, Utils::QueryError};
                            }

                            account->twoFactor()->reloadBackupKeys();

                            foundValidBackupKey = true;
                        }
                    }

                    if (!foundValidBackupKey) {
                        return {0, Utils::TwoFactorRequired};
                    }
                }
            }
        }
    }

    return {id, Utils::NoError};
}

bool PasswordProvisioningMethod::available(quint64 userId, QString application, TokenProvisioningManager::TokenProvisioningPurpose provisioningPurpose) const {
    // This method is always available
    // TODO: For account modification this method should not be available
    return true;
}