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

struct Fido2Private {
        UserAccount* parent;

        QJsonObject prepareCache;
        QString lastRpName;
        QString lastRpId;
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
    if (authenticatorAttachment > 2 || authenticatorAttachment < 0) {
        Utils::sendDbusError(Utils::InvalidInput, message);
        return "";
    }

    QStringList args = {
        "preregister",
        "--rpname", application,
        "--rpid", rp,
        "--username", d->parent->user()->username(),
        "--userid", QString::number(d->parent->id())
    };

    switch (authenticatorAttachment) {
        case 0:
            args.append({"--authattachment", "platform"});
            break;
        case 1:
            args.append({"--authattachment", "cross-platform"});
            break;
        default:
            break;
    }

    d->lastRpName = application;
    d->lastRpId = rp;

    QJsonObject payload;
    payload.insert("existingCreds", QJsonArray::fromStringList(FidoUtils::FidoCredsForUser(d->parent->id(), application)));

    QProcess fidoHelper;
    fidoHelper.start(Utils::fidoHelperPath(), args);
    fidoHelper.write(QJsonDocument(payload).toJson());
    fidoHelper.closeWriteChannel();
    fidoHelper.waitForFinished(-1);

    if (fidoHelper.error() != QProcess::UnknownError) {
        Utils::sendDbusError(Utils::FidoSupportUnavailable, message);
        return "";
    }

    if (fidoHelper.exitCode() != 0) {
        Utils::sendDbusError(Utils::InternalError, message);
        return "";
    }

    auto output = fidoHelper.readAllStandardOutput();

    d->prepareCache = QJsonDocument::fromJson(output).object();
    return QJsonDocument(d->prepareCache).toJson();
}

void Fido2::CompleteRegister(QString response, QStringList expectOrigins, QString keyName,
    const QDBusMessage& message) {

    QJsonParseError parseError;
    auto responseDoc = QJsonDocument::fromJson(response.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !responseDoc.isObject()) {
        Utils::sendDbusError(Utils::InvalidInput, message);
        return;
    }
    
    QStringList args = {
        "register",
        "--rpname", d->lastRpName,
        "--rpid", d->lastRpId,
        "--userid", QString::number(d->parent->id())
    };

    QJsonObject payload;
    payload.insert("preregisterOptions", d->prepareCache);
    payload.insert("response", responseDoc.object());
    payload.insert("expectOrigins", QJsonArray::fromStringList(expectOrigins));

    QProcess fidoHelper;
    fidoHelper.start(Utils::fidoHelperPath(), args);
    fidoHelper.write(QJsonDocument(payload).toJson());
    fidoHelper.closeWriteChannel();
    fidoHelper.waitForFinished(-1);

    if (fidoHelper.error() != QProcess::UnknownError) {
        Utils::sendDbusError(Utils::FidoSupportUnavailable, message);
        return;
    }

    if (fidoHelper.exitCode() != 0) {
        Utils::sendDbusError(Utils::InternalError, message);
        return;
    }

    auto output = fidoHelper.readAllStandardOutput();

    QSqlQuery query;
    query.prepare("INSERT INTO fido(userid, data, name, application) VALUES(:userid, :data, :name, :application)");
    query.bindValue(":userid", d->parent->id());
    query.bindValue(":data", output);
    query.bindValue(":name", keyName);
    query.bindValue(":application", d->lastRpName);

    if (!query.exec()) {
        Utils::sendDbusError(Utils::QueryError, message);
        return;
    }

    if (d->parent->user()->verified()) {
        Utils::sendTemplateEmail("fido-new-key", {d->parent->user()->email()}, d->parent->user()->locale(), {
                                             {"user", d->parent->user()->username()},
                                             {"key", keyName},
                                             {"application", d->lastRpName}
                                         });
    }
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
