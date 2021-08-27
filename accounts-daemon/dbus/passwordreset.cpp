/****************************************
 *
 *   INSERT-PROJECT-NAME-HERE - INSERT-GENERIC-NAME-HERE
 *   Copyright (C) 2021 Victor Tran
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
#include "passwordreset.h"

#include <QSqlQuery>
#include <QDBusMetaType>
#include <QDateTime>
#include "useraccount.h"
#include "utils.h"

struct PasswordResetPrivate {
    UserAccount* parent;
};

QDBusArgument& operator<<(QDBusArgument& arg, const ResetMethod& method) {
    arg.beginStructure();
    arg << method.type << method.challenge;
    arg.endStructure();
    return arg;
}

const QDBusArgument& operator>>(const QDBusArgument& arg, ResetMethod& method) {
    arg.beginStructure();
    arg >> method.type >> method.challenge;
    arg.endStructure();
    return arg;
}

PasswordReset::PasswordReset(UserAccount* parent) : QDBusAbstractAdaptor(parent) {
    d = new PasswordResetPrivate();
    d->parent = parent;

    qRegisterMetaType<ResetMethod>();
    qRegisterMetaType<QList<ResetMethod>>();
    qDBusRegisterMetaType<ResetMethod>();
    qDBusRegisterMetaType<QList<ResetMethod>>();
}

PasswordReset::~PasswordReset() {
    delete d;
}

QList<ResetMethod> PasswordReset::ResetMethods(const QDBusMessage& message) {
    QList<ResetMethod> methods;

    QSqlQuery userQuery;
    userQuery.prepare("SELECT * FROM users WHERE id=:id");
    userQuery.bindValue(":id", d->parent->id());
    userQuery.exec();
    if (!userQuery.next()) {
        Utils::sendDbusError(Utils::QueryError, message);
        return {};
    }

    [&] {
        QString email = userQuery.value("email").toString();
        if (!email.contains("@")) return;

        QString user = email.split("@").first();
        user.truncate(2);
        QString domain = email.split("@").at(1);
        domain.truncate(1);

        methods.append({
            "email",
            {
                {"user", user},
                {"domain", domain}
            }
        });
    }();

    return methods;
}

void PasswordReset::ResetPassword(QString type, QVariantMap challenge, const QDBusMessage& message) {
    QSqlQuery userQuery;
    userQuery.prepare("SELECT * FROM users WHERE id=:id");
    userQuery.bindValue(":id", d->parent->id());
    userQuery.exec();
    if (!userQuery.next()) {
        Utils::sendDbusError(Utils::QueryError, message);
        return;
    }

    if (type == "email") {
        QString email = userQuery.value("email").toString();
        if (email == challenge.value("email").toString()) {
            //Issue the password reset
            issuePasswordReset();
        }
    } else {
        Utils::sendDbusError(Utils::InvalidInput, message);
        return;
    }
}

void PasswordReset::issuePasswordReset() {
    QSqlQuery userQuery;
    userQuery.prepare("SELECT * FROM users WHERE id=:id");
    userQuery.bindValue(":id", d->parent->id());
    userQuery.exec();
    if (!userQuery.next()) {
        return;
    }

    QString password = Utils::generateRandomBytes(24).toBase64();

    QSqlQuery resetQuery;
    resetQuery.prepare("INSERT INTO passwordResets(userId, temporaryPassword, expiry) VALUES(:id, :password, :expiry) ON CONFLICT ON CONSTRAINT pk_passwordresets DO UPDATE SET temporaryPassword=:password, expiry=:expiry");
    resetQuery.bindValue(":id", d->parent->id());
    resetQuery.bindValue(":password", Utils::generateHashedPassword(password));
    resetQuery.bindValue(":expiry", QDateTime::currentMSecsSinceEpoch() + 30 * 60 * 1000);

    if (!resetQuery.exec()) {
        return;
    }

    Utils::sendTemplateEmail("recover", {userQuery.value("email").toString()}, "en", {
        {"user", userQuery.value("username").toString()},
        {"password", password}
    });
}
