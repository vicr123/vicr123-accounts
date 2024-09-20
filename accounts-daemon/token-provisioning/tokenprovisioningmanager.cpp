//
// Created by victor on 20/09/24.
//

#include "tokenprovisioningmanager.h"

#include "fidoprovisioningmethod.h"
#include "passwordprovisioningmethod.h"
#include "tokenprovisioningmethod.h"

#include "../dbus/accountmanager.h"

#include <QSqlQuery>

struct TokenProvisioningManagerPrivate {
        QList<TokenProvisioningMethod*> tokenProvisioningMethods;
};

TokenProvisioningManager::TokenProvisioningManager(AccountManager* parent) :
    QObject(parent), d(new TokenProvisioningManagerPrivate) {
    d->tokenProvisioningMethods.append(new PasswordProvisioningMethod(parent));
    d->tokenProvisioningMethods.append(new FidoProvisioningMethod(parent));
}

TokenProvisioningManager::~TokenProvisioningManager() {
    delete d;
}

TokenProvisioningManager::ProvisionResult TokenProvisioningManager::provision(QString method, TokenProvisioningPurpose provisioningPurpose, QString application, QVariantMap options) const {
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
                    return {{}, Utils::DBusError::InternalError};
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