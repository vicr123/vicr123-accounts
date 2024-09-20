//
// Created by victor on 20/09/24.
//

#ifndef TOKENPROVISIONINGMANAGER_H
#define TOKENPROVISIONINGMANAGER_H

#include "utils.h"

#include <QObject>

class AccountManager;
struct TokenProvisioningManagerPrivate;
class TokenProvisioningManager : public QObject {
        Q_OBJECT
    public:
        struct ProvisionResult {
                QVariantMap result;
                Utils::DBusError error = Utils::DBusError::NoError;
        };

        enum class TokenProvisioningPurpose {
            LoginToken,
            AccountModificationToken
        };

        explicit TokenProvisioningManager(AccountManager* parent = nullptr);
        ~TokenProvisioningManager() override;

        [[nodiscard]] ProvisionResult provision(QString method, TokenProvisioningManager::TokenProvisioningPurpose provisioningPurpose, QString application, QVariantMap options) const;
        [[nodiscard]] QStringList availableMethods(quint64 userId, QString application, TokenProvisioningManager::TokenProvisioningPurpose provisioningPurpose) const;

    private:
        TokenProvisioningManagerPrivate* d;
};

#endif // TOKENPROVISIONINGMANAGER_H
