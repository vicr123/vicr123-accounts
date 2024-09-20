//
// Created by victor on 20/09/24.
//

#ifndef TOKENPROVISIONINGMETHOD_H
#define TOKENPROVISIONINGMETHOD_H
#include "tokenprovisioningmanager.h"
#include "utils.h"

#include <QObject>

class AccountManager;
struct TokenProvisioningMethodPrivate;
class TokenProvisioningMethod : public QObject {
        Q_OBJECT
    public:
        struct ProvisionResult {
                quint64 userId;
                Utils::DBusError error = Utils::DBusError::NoError;
                QVariantMap options;
        };

        explicit TokenProvisioningMethod(AccountManager* parent);
        ~TokenProvisioningMethod() override;

        [[nodiscard]] virtual QString tokenProvisioningMethod() const = 0;
        [[nodiscard]] virtual ProvisionResult provision(QVariantMap parameters, TokenProvisioningManager::TokenProvisioningPurpose provisioningPurpose) const = 0;
        [[nodiscard]] virtual bool available(quint64 userId, QString application, TokenProvisioningManager::TokenProvisioningPurpose provisioningPurpose) const = 0;

    protected:
        [[nodiscard]] AccountManager* accountManager() const;

    private:
        TokenProvisioningMethodPrivate* d;
};

#endif // TOKENPROVISIONINGMETHOD_H
