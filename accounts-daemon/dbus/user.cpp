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

#include "mailmessage.h"
#include "useraccount.h"
#include "utils.h"
#include "validation.h"
#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>

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
    if (username.isEmpty()) {
        Utils::sendDbusError(Utils::InvalidInput, message);
        return;
    }

    if (!Validation::validateUsername(username)) {
        Utils::sendDbusError(Utils::InvalidInput, message);
        return;
    }

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
    auto error = setPassword(password);
    if (error != Utils::NoError) {
        Utils::sendDbusError(error, message);
    }
}

void User::SetEmail(QString email, const QDBusMessage& message) {
    if (!Validation::validateEmailAddress(email)) {
        Utils::sendDbusError(Utils::InvalidInput, message);
        return;
    }

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

void User::ResendVerificationEmail(const QDBusMessage& message) {
    if (!Utils::sendVerificationEmail(d->parent->id())) {
        Utils::sendDbusError(Utils::InternalError, message);
        return;
    }
}

void User::VerifyEmail(QString verificationCode, const QDBusMessage& message) {
    if (verificationCode.isEmpty()) {
        Utils::sendDbusError(Utils::InvalidInput, message);
        return;
    }

    QSqlDatabase db;
    db.transaction();

    QSqlQuery query;
    query.prepare("DELETE FROM verifications WHERE userid=:id AND verificationstring=:code AND expiry > :now");
    query.bindValue(":id", d->parent->id());
    query.bindValue(":code", verificationCode);
    query.bindValue(":now", QDateTime::currentMSecsSinceEpoch());

    if (!query.exec()) {
        db.rollback();
        Utils::sendDbusError(Utils::QueryError, message);
        return;
    }

    if (query.numRowsAffected() == 0) {
        db.rollback();
        Utils::sendDbusError(Utils::VerificationCodeIncorrect, message);
        return;
    }

    QSqlQuery updateUserQuery;
    updateUserQuery.prepare("UPDATE users SET verified=:verified WHERE id=:id");
    updateUserQuery.bindValue(":verified", true);
    updateUserQuery.bindValue(":id", d->parent->id());
    if (!updateUserQuery.exec()) {
        db.rollback();
        Utils::sendDbusError(Utils::QueryError, message);
        return;
    }

    if (db.commit()) {
        Utils::sendDbusError(Utils::QueryError, message);
        return;
    }

    d->verified = true;
    emit VerifiedChanged(true);
}

bool User::VerifyPassword(QString password, const QDBusMessage& message) {
    if (password.isEmpty()) {
        Utils::sendDbusError(Utils::InvalidInput, message);
        return false;
    }

    QSqlQuery userQuery;
    userQuery.prepare("SELECT * FROM users WHERE id=:id");
    userQuery.bindValue(":id", d->parent->id());
    userQuery.exec();
    userQuery.next();

    //Ensure the password is correct
    QString passwordHash = userQuery.value("password").toString();
    if (passwordHash.startsWith("!")) {
        Utils::sendDbusError(Utils::DisabledAccount, message);
        return false;
    }

    return Utils::verifyHashedPassword(password, passwordHash);
}

void User::ErasePassword(const QDBusMessage& message) {
    QSqlQuery query;
    query.prepare("UPDATE users SET password=:password WHERE id=:id");
    query.bindValue(":password", "x");
    query.bindValue(":id", d->parent->id());
    if (!query.exec()) {
        Utils::sendDbusError(Utils::QueryError, message);
        return;
    }
}

void User::SetEmailVerified(bool verified, const QDBusMessage& message) {
    QSqlQuery updateUserQuery;
    updateUserQuery.prepare("UPDATE users SET verified=:verified WHERE id=:id");
    updateUserQuery.bindValue(":verified", verified);
    updateUserQuery.bindValue(":id", d->parent->id());
    if (!updateUserQuery.exec()) {
        Utils::sendDbusError(Utils::QueryError, message);
        return;
    }

    d->verified = true;
    emit VerifiedChanged(true);
}
QDBusObjectPath User::CreateMailMessage(const QDBusMessage& message) {
    if (!verified()) {
        Utils::sendDbusError(Utils::AccountEmailNotVerified, message);
        return {};
    }

    auto* mailMessage = new MailMessage(this->email());
    return mailMessage->path();
}

Utils::DBusError User::setPassword(QString password) {
    if (password.isEmpty()) {
        return Utils::InvalidInput;
    }

    if (!Validation::validatePassword(password)) {
        return Utils::InvalidInput;
    }

    QString hashedPassword = Utils::generateHashedPassword(password);

    QSqlQuery query;
    query.prepare("UPDATE users SET password=:password WHERE id=:id");
    query.bindValue(":password", hashedPassword);
    query.bindValue(":id", d->parent->id());
    if (!query.exec()) {
        return Utils::QueryError;
    }

    if (d->verified) {
        Utils::sendTemplateEmail("passwordchange", {d->email}, this->locale(), {
              {"user", d->username}
          });
    }
    return Utils::NoError;
}

QString User::locale() {
    // TODO
    return "en";
}
