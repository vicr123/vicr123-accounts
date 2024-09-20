//
// Created by victor on 20/09/24.
//

#include "tokenprovisioningmethod.h"

#include "dbus/accountmanager.h"

struct TokenProvisioningMethodPrivate {
        AccountManager* accountManager;
};

TokenProvisioningMethod::TokenProvisioningMethod(AccountManager* parent) :
    QObject(parent), d(new TokenProvisioningMethodPrivate) {
}

TokenProvisioningMethod::~TokenProvisioningMethod() {
    delete d;
}

AccountManager* TokenProvisioningMethod::accountManager() const {
    return d->accountManager;
}