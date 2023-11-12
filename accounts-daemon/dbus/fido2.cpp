/****************************************
 *
 *   INSERT-PROJECT-NAME-HERE - INSERT-GENERIC-NAME-HERE
 *   Copyright (C) 2022 Victor Tran
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
#include "fido2.h"
#include "useraccount.h"
#include "user.h"

#include "fidoutils.h"
#include "utils.h"
#include <QDBusMetaType>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSqlQuery>

#include "vicr123-accounts-fido/fido.h"

struct Fido2Private {
        UserAccount* parent;

        QJsonObject prepareCache;
        QString lastRpName;
        QString lastRpId;

        Fido fido;
};

QDBusArgument& operator<<(QDBusArgument& argument, const Fido2::Fido2Key& key) {
    argument.beginStructure();
    argument << key.id << key.application << key.name;
    argument.endStructure();
    return argument;
}

const QDBusArgument& operator>>(const QDBusArgument& argument, Fido2::Fido2Key& key) {
    argument.beginStructure();
    argument >> key.id >> key.application >> key.name;
    argument.endStructure();
    return argument;
}

Fido2::Fido2(UserAccount* parent) :
    QDBusAbstractAdaptor{parent} {
    qDBusRegisterMetaType<Fido2Key>();
    qDBusRegisterMetaType<QList<Fido2Key>>();

    d = new Fido2Private();
    d->parent = parent;
}

Fido2::~Fido2() {
    delete d;
}

QString Fido2::PrepareRegister(QString application, QString rp, int authenticatorAttachment, const QDBusMessage& message) {
    message.setDelayedReply(true);
    this->privatePrepareRegister(application, rp, authenticatorAttachment, message).then([message](QString retval) {
        if (!retval.isEmpty()) {
            Utils::accountsBus().send(message.createReply(retval));
        }
    });

    return {};
}

void Fido2::CompleteRegister(QString response, QStringList expectOrigins, QString keyName, const QDBusMessage& message) {
    message.setDelayedReply(true);
    this->privateCompleteRegister(response, expectOrigins, keyName, message).then([message]() {
        Utils::accountsBus().send(message.createReply());
    });
}

void Fido2::DeleteKey(int id, const QDBusMessage& message) {
    QSqlQuery query;
    query.prepare("DELETE FROM fido WHERE userid=:userid AND id=:id RETURNING name, application");
    query.bindValue(":userid", d->parent->id());
    query.bindValue(":id", id);

    if (!query.exec()) {
        Utils::sendDbusError(Utils::QueryError, message);
        return;
    }

    query.next();
    auto keyName = query.value("name").toString();
    auto application = query.value("application").toString();

    if (d->parent->user()->verified()) {
        Utils::sendTemplateEmail("fido-remove-key", {d->parent->user()->email()}, d->parent->user()->locale(), {
                                             {"user", d->parent->user()->username()},
                                             {"key", keyName},
                                             {"application", application}
                                         });
    }
}

QList<Fido2::Fido2Key> Fido2::GetKeys(const QDBusMessage& message) {
    QSqlQuery query;
    query.prepare("SELECT id, name, application FROM fido WHERE userid=:userid");
    query.bindValue(":userid", d->parent->id());
    if (!query.exec()) {
        Utils::sendDbusError(Utils::QueryError, message);
        return {};
    }

    QList<Fido2Key> keys;
    while (query.next()) {
        Fido2Key key;
        key.id = query.value("id").toInt();
        key.name = query.value("name").toString();
        key.application = query.value("application").toString();
        keys.append(key);
    }
    return keys;
}

QCoro::Task<QString> Fido2::privatePrepareRegister(QString application, QString rp, int authenticatorAttachment, const QDBusMessage& message) {
    if (authenticatorAttachment > 2 || authenticatorAttachment < 0) {
        Utils::sendDbusError(Utils::InvalidInput, message);
        co_return "";
    }

    QString authenticatorAttachmentString;
    switch (authenticatorAttachment) {
        case 0:
            authenticatorAttachmentString = "platform";
        break;
        case 1:
            authenticatorAttachmentString = "cross-platform";
        break;
        default:
            break;
    }

    d->lastRpName = application;
    d->lastRpId = rp;

    QJsonObject payload;
    payload.insert("existingCreds", QJsonArray::fromStringList(FidoUtils::FidoCredsForUser(d->parent->id(), application)));

    auto preregisterOutput = co_await d->fido.preRegisterPasskey(application, rp, d->parent->user()->username(), QString::number(d->parent->id()), authenticatorAttachmentString, QJsonDocument(payload).toJson());

    d->prepareCache = QJsonDocument::fromJson(preregisterOutput.toUtf8()).object();
    auto retval = QJsonDocument(d->prepareCache).toJson();
    co_return retval;
}

QCoro::Task<> Fido2::privateCompleteRegister(QString response, QStringList expectOrigins, QString keyName, const QDBusMessage& message) {
    QJsonParseError parseError;
    auto responseDoc = QJsonDocument::fromJson(response.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !responseDoc.isObject()) {
        Utils::sendDbusError(Utils::InvalidInput, message);
        co_return;
    }

    QJsonObject payload;
    payload.insert("preregisterOptions", d->prepareCache);
    payload.insert("response", responseDoc.object());
    payload.insert("expectOrigins", QJsonArray::fromStringList(expectOrigins));

    auto securityKey = co_await d->fido.registerPasskey(d->lastRpName, d->lastRpId, QString::number(d->parent->id()), QJsonDocument(payload).toJson());

    QSqlQuery query;
    query.prepare("INSERT INTO fido(userid, data, name, application) VALUES(:userid, :data, :name, :application)");
    query.bindValue(":userid", d->parent->id());
    query.bindValue(":data", securityKey.toJson());
    query.bindValue(":name", keyName);
    query.bindValue(":application", d->lastRpName);

    if (!query.exec()) {
        Utils::sendDbusError(Utils::QueryError, message);
        co_return;
    }

    if (d->parent->user()->verified()) {
        Utils::sendTemplateEmail("fido-new-key", {d->parent->user()->email()}, d->parent->user()->locale(), {
                                             {"user", d->parent->user()->username()},
                                             {"key", keyName},
                                             {"application", d->lastRpName}
                                         });
    }
}
