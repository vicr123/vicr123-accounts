//
// Created by victor on 21/09/24.
//

#ifndef FIDOPROVISIONINGMETHOD_H
#define FIDOPROVISIONINGMETHOD_H
#include "tokenprovisioningmethod.h"

class FidoProvisioningMethod : public TokenProvisioningMethod {
        Q_OBJECT
    public:
        explicit FidoProvisioningMethod(AccountManager* parent);

        [[nodiscard]] QString tokenProvisioningMethod() const override;
        [[nodiscard]] ProvisionResult provision(QVariantMap options, TokenProvisioningManager::TokenProvisioningPurpose provisioningPurpose) const override;
        [[nodiscard]] bool available(quint64 userId, QString application, TokenProvisioningManager::TokenProvisioningPurpose provisioningPurpose) const override;
};

#endif // FIDOPROVISIONINGMETHOD_H
