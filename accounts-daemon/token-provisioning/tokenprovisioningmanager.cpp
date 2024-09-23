//
// Created by victor on 20/09/24.
//

#include "tokenprovisioningmanager.h"

#include "fidoprovisioningmethod.h"
#include "passwordprovisioningmethod.h"
#include "qjsonwebtoken.h"
#include "tokenprovisioningmethod.h"

#include "../dbus/accountmanager.h"

#include <QRandomGenerator>
#include <QSqlQuery>

struct TokenProvisioningManagerPrivate {
        QList<TokenProvisioningMethod*> tokenProvisioningMethods;
        QString jwtSecret;
};

TokenProvisioningManager::TokenProvisioningManager(AccountManager* parent) :
    QObject(parent), d(new TokenProvisioningManagerPrivate) {
    d->tokenProvisioningMethods.append(new PasswordProvisioningMethod(parent));
    d->tokenProvisioningMethods.append(new FidoProvisioningMethod(parent));

    const auto characters = QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
    for (auto i = 0; i < 32; i++) {
        d->jwtSecret.append(characters.at(QRandomGenerator::global()->bounded(characters.length())));
    }
}

TokenProvisioningManager::~TokenProvisioningManager() {
    delete d;
}

TokenProvisioningManager::TokenProvisioningPurpose TokenProvisioningManager::purposeForString(QString purpose) {
    return QMap<QString, TokenProvisioningPurpose>({
                                                       {"login",               TokenProvisioningPurpose::LoginToken              },
                                                       {"accountModification", TokenProvisioningPurpose::AccountModificationToken},
    })
        .value(purpose, TokenProvisioningPurpose::Unknown);
}

TokenProvisioningManager::ProvisionResult TokenProvisioningManager::provision(QString method, TokenProvisioningPurpose provisioningPurpose, QString application, QVariantMap options) const {
    if (provisioningPurpose == TokenProvisioningPurpose::Unknown) {
        return {{}, Utils::DBusError::InvalidInput};
    }

    for (const auto tokenProvisioningMethod : d->tokenProvisioningMethods) {
        if (tokenProvisioningMethod->tokenProvisioningMethod() == method) {
            auto [userId, error, optionsResult] = tokenProvisioningMethod->provision(options, provisioningPurpose);
            if (error != Utils::NoError) {
                return {{}, error};
            }

            if (!optionsResult.isEmpty()) {
                return {optionsResult, Utils::NoError};
            }

            switch (provisioningPurpose) {
                case TokenProvisioningPurpose::LoginToken:
                    {
                        // Create a new user token and save it in the database
                        const QString newToken = Utils::generateSalt().toBase64();

                        QSqlQuery tokenInsertQuery;
                        tokenInsertQuery.prepare("INSERT INTO tokens(userid, token, application) VALUES(:id, :token, :application)");
                        tokenInsertQuery.bindValue(":id", userId);
                        tokenInsertQuery.bindValue(":token", newToken);
                        tokenInsertQuery.bindValue(":application", application);
                        if (!tokenInsertQuery.exec()) {
                            return {{}, Utils::QueryError};
                        }

                        return {{{"token", newToken}}, Utils::NoError};
                    }
                case TokenProvisioningPurpose::AccountModificationToken:
                    {
                        // Create a short-lived JWT that we can use to perform account modification actions
                        QJsonWebToken jwt;
                        jwt.setAlgorithmStr("HS512");
                        jwt.setSecret(d->jwtSecret);
                        jwt.appendClaim("sub", QString::number(userId));
                        jwt.appendClaim("exp", QString::number(QDateTime::currentDateTimeUtc().addSecs(60 * 60).toMSecsSinceEpoch())); // One hour
                        jwt.appendClaim("pur", QString::number(static_cast<int>(provisioningPurpose)));
                        return {
                            {{"token", jwt.getToken()}},
                            Utils::NoError};
                    }
                default:; // noop
            }
        }
    }
    return {{}, Utils::DBusError::InternalError};
}

QStringList TokenProvisioningManager::availableMethods(quint64 userId, QString application, TokenProvisioningManager::TokenProvisioningPurpose provisioningPurpose) const {
    QStringList methods;
    for (const auto tokenProvisioningMethod : d->tokenProvisioningMethods) {
        if (tokenProvisioningMethod->available(userId, application, provisioningPurpose)) {
            methods.append(tokenProvisioningMethod->tokenProvisioningMethod());
        }
    }
    return methods;
}

bool TokenProvisioningManager::verifyToken(QString token, quint64* userId, TokenProvisioningPurpose* provisioningPurpose) const {
    // First try to understand the token as a JWT
    auto jwt = QJsonWebToken::fromTokenAndSecret(token, d->jwtSecret);
    if (jwt.isValid()) {
        auto exp = jwt.claim("exp").toULongLong();
        if (exp < QDateTime::currentMSecsSinceEpoch()) {
            // JWT has expired
            return false;
        }

        bool userIdOk;
        auto tokenUserId = jwt.claim("sub").toInt(&userIdOk);
        if (!userIdOk) {
            return false;
        }
        *userId = tokenUserId;

        bool purposeOk;
        auto purpose = jwt.claim("pur").toInt(&purposeOk);
        if (!purposeOk) {
            return false;
        }
        *provisioningPurpose = static_cast<TokenProvisioningPurpose>(purpose);
        return true;
    }

    // Now read the database for tokens
    QSqlQuery query;
    query.prepare("SELECT * FROM tokens WHERE token=:token");
    query.bindValue(":token", token);
    query.exec();

    if (query.next()) {
        *userId = query.value("userid").toULongLong();
        *provisioningPurpose = TokenProvisioningPurpose::LoginToken;
        return true;
    }

    return false;
}