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

#include <QRandomGenerator64>
#include <QPasswordDigestor>
#include <QSettings>
#include "utils.h"

QDBusConnection Utils::accountsBus() {
    QSettings settings("/etc/vicr123-accounts.conf", QSettings::IniFormat);
    if (settings.value("dbus/configuration").toString() == "dedicated") {
        return QDBusConnection("accounts");
    } else {
        return QDBusConnection::sessionBus();
    }
}

QString Utils::generateHashedPassword(QString password, int iterations) {
    QByteArray saltByteArray = generateSalt();
    QString saltString = saltByteArray.toBase64();

    QByteArray hash = QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha3_512, password.toUtf8(), saltByteArray, iterations, 512);
    return QStringLiteral("PBKDF2.SHA3_512.%1.%2.%3").arg(iterations).arg(saltString, hash.toBase64());
}

bool Utils::verifyHashedPassword(QString password, QString hash) {
    QStringList parts = hash.split(".");
    if (parts.length() != 5) return false;
    if (parts.at(0) != "PBKDF2") return false;
    if (parts.at(1) != "SHA3_512") return false;

    int iterations = parts.at(2).toInt();
    QByteArray salt = QByteArray::fromBase64(parts.at(3).toUtf8());
    QByteArray storedHash = QByteArray::fromBase64(parts.at(4).toUtf8());

    QByteArray comparedHash = QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha3_512, password.toUtf8(), salt, iterations, 512);
    if (storedHash != comparedHash) return false;
    return true;
}

void Utils::sendDbusError(DBusError error, const QDBusMessage& replyTo) {
    const QMap<DBusError, QPair<QString, QString>> errors = {
        {InternalError, {"com.vicr123.accounts.Error.InternalError", "Internal Error"}},
        {NoAccount, {"com.vicr123.accounts.Error.NoAccount", "The user account does not exist"}},
        {QueryError, {"com.vicr123.accounts.Error.QueryError", "Could not execute the query on the database"}},
        {IncorrectPassword, {"com.vicr123.accounts.Error.IncorrectPassword", "The password is incorrect"}}
    };

    QPair<QString, QString> errorStrings = errors.value(error);

    Utils::accountsBus().send(replyTo.createErrorReply(errorStrings.first, errorStrings.second));
}

QByteArray Utils::generateSalt() {
    quint32 saltBytes[33];
    QRandomGenerator::global()->fillRange(saltBytes);
    saltBytes[32] = '\0';

    QByteArray saltByteArray(reinterpret_cast<char*>(saltBytes));
    return saltByteArray;
}
