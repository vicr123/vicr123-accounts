//
// Created by victor on 20/09/24.
//

#ifndef PASSWORDPROVISIONINGMETHOD_H
#define PASSWORDPROVISIONINGMETHOD_H
#include "tokenprovisioningmethod.h"

class AccountManager;
class PasswordProvisioningMethod : public TokenProvisioningMethod {
        Q_OBJECT
    public:
        explicit PasswordProvisioningMethod(AccountManager* parent);

        [[nodiscard]] QString tokenProvisioningMethod() const override;
        [[nodiscard]] ProvisionResult provision(QVariantMap options, TokenProvisioningManager::TokenProvisioningPurpose provisioningPurpose) const override;
        [[nodiscard]] bool available(quint64 userId, QString application, TokenProvisioningManager::TokenProvisioningPurpose provisioningPurpose) const override;
};

#endif // PASSWORDPROVISIONINGMETHOD_H
