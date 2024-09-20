//
// Created by victor on 21/09/24.
//

#include "fidoprovisioningmethod.h"

#include "dbus/accountmanager.h"
#include "fidoutils.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSqlQuery>

FidoProvisioningMethod::FidoProvisioningMethod(AccountManager* parent) :
    TokenProvisioningMethod(parent) {
}

QString FidoProvisioningMethod::tokenProvisioningMethod() const {
    return QStringLiteral("fido");
}

TokenProvisioningMethod::ProvisionResult FidoProvisioningMethod::provision(QVariantMap options, TokenProvisioningManager::TokenProvisioningPurpose provisioningPurpose) const {
    const auto username = options.value("username").toString();
    const auto application = options.value("application").toString();
    if (application.isEmpty() || username.isEmpty() || application.isEmpty()) {
        return {0, Utils::InvalidInput};
    }

    quint64 id = accountManager()->userIdByUsername(username);
    if (id == 0) {
        return {0, Utils::NoAccount};
    }

    if (options.contains("response")) {
        if (!options.contains("response") && !options.contains("pregetOptions") && !options.contains("expectOrigins")) {
            return {0, Utils::InvalidInput};
        }

        QStringList args = {
            "preget",
            "--rpname", options.value("rpname").toString(),
            "--rpid", options.value("rpid").toString()};

        QJsonObject payload;
        payload.insert("existingCreds", QJsonArray::fromStringList(FidoUtils::FidoCredsForUser(id, application)));
        payload.insert("extraOrigins", QJsonArray::fromStringList(options.value("expectOrigins").toStringList()));
        payload.insert("response", options.value("response").toString());
        payload.insert("pregetOptions", options.value("pregetOptions").toString());

        QProcess fidoHelper;
        fidoHelper.start(Utils::fidoHelperPath(), args);
        fidoHelper.write(QJsonDocument(payload).toJson());
        fidoHelper.closeWriteChannel();
        fidoHelper.waitForFinished(-1);

        if (fidoHelper.error() != QProcess::UnknownError) {
            return {0, Utils::FidoSupportUnavailable};
        }

        if (fidoHelper.exitCode() != 0) {
            return {0, Utils::InternalError};
        }

        auto outputStr = fidoHelper.readAllStandardOutput();
        auto output = QJsonDocument::fromJson(outputStr).object();

        auto usedCred = output.value("usedCred").toString().toUtf8();
        auto newCred = output.value("newCred").toString().toUtf8();

        QSqlQuery fidoQuery;
        fidoQuery.prepare("UPDATE fido SET data=:newCred WHERE data=:oldCred AND userid=:id");
        fidoQuery.bindValue(":newCred", newCred);
        fidoQuery.bindValue(":oldCred", usedCred);
        fidoQuery.bindValue(":id", id);
        if (!fidoQuery.exec()) {
            return {0, Utils::QueryError};
        }

        // Provision a token
        return {id, Utils::NoError};
    } else {
        if (!options.contains("rpname") && !options.contains("rpid")) {
            return {0, Utils::InvalidInput};
        }

        QStringList args = {
            "preget",
            "--rpname", options.value("rpname").toString(),
            "--rpid", options.value("rpid").toString()};

        QJsonObject payload;
        payload.insert("existingCreds", QJsonArray::fromStringList(FidoUtils::FidoCredsForUser(id, application)));

        QProcess fidoHelper;
        fidoHelper.start(Utils::fidoHelperPath(), args);
        fidoHelper.write(QJsonDocument(payload).toJson());
        fidoHelper.closeWriteChannel();
        fidoHelper.waitForFinished(-1);

        if (fidoHelper.error() != QProcess::UnknownError) {
            return {0, Utils::FidoSupportUnavailable};
        }

        if (fidoHelper.exitCode() != 0) {
            return {0, Utils::InternalError};
        }

        auto output = fidoHelper.readAllStandardOutput();
        return {
            0, Utils::NoError, QVariantMap({{"options", output}}
              )
        };
    }
}

bool FidoProvisioningMethod::available(quint64 userId, QString application, TokenProvisioningManager::TokenProvisioningPurpose provisioningPurpose) const {
    // Check if FIDO is set up
    QSqlQuery fidoQuery;
    fidoQuery.prepare("SELECT COUNT(*) FROM fido WHERE application=:application AND userid=:userid");
    fidoQuery.bindValue(":application", application);
    fidoQuery.bindValue(":userid", userId);
    if (!fidoQuery.exec()) {
        return false;
    }

    fidoQuery.next();
    const auto count = fidoQuery.value(0).toInt();
    return count > 0;
}