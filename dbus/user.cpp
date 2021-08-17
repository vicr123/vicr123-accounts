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
#include "user.h"

#include <QSqlQuery>
#include <QSqlError>
#include "useraccount.h"
#include "utils.h"

struct UserPrivate {
    UserAccount* parent;
    QString username;
    QString email;
    bool verified;
};

User::User(UserAccount* parent) : QDBusAbstractAdaptor(parent) {
    d = new UserPrivate();
    d->parent = parent;

    QSqlQuery query;
    query.prepare("SELECT * FROM users WHERE id=:id");
    query.bindValue(":id", d->parent->id());
    query.exec();
    query.next();

    d->username = query.value("username").toString();
    d->email = query.value("email").toString();
    d->verified = query.value("verified").toBool();
}

User::~User() {
    delete d;
}

quint64 User::id() {
    return d->parent->id();
}

QString User::username() {
    return d->username;
}

QString User::email() {
    return d->email;
}

bool User::verified() {
    return d->verified;
}

void User::SetUsername(QString username, const QDBusMessage& message) {
    QString oldUsername = d->username;

    QSqlQuery query;
    query.prepare("UPDATE users SET username=:username WHERE id=:id");
    query.bindValue(":username", username);
    query.bindValue(":id", d->parent->id());
    if (!query.exec()) {
        Utils::sendDbusError(Utils::QueryError, message);
        return;
    }

    d->username = username;
    emit UsernameChanged(oldUsername, username);
}

void User::SetPassword(QString password, const QDBusMessage& message) {
    QString hashedPassword = Utils::generateHashedPassword(password);

    QSqlQuery query;
    query.prepare("UPDATE users SET password=:password WHERE id=:id");
    query.bindValue(":password", hashedPassword);
    query.bindValue(":id", d->parent->id());
    if (!query.exec()) {
        Utils::sendDbusError(Utils::QueryError, message);
        return;
    }
}

void User::SetEmail(QString email, const QDBusMessage& message) {
    QSqlQuery query;
    query.prepare("UPDATE users SET email=:email, verified=false WHERE id=:id");
    query.bindValue(":email", email);
    query.bindValue(":id", d->parent->id());
    if (!query.exec()) {
        Utils::sendDbusError(Utils::QueryError, message);
        return;
    }

    d->verified = false;
    d->email = email;

    emit VerifiedChanged(false);
    emit EmailChanged(email);
}
