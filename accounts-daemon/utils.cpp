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
#include <QtConcurrent>
#include <QMessageAuthenticationCode>
#include "logger.h"
#include "utils.h"
#include "mailtemplate.h"

#include <src/SmtpMime>

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
        {IncorrectPassword, {"com.vicr123.accounts.Error.IncorrectPassword", "The password is incorrect"}},
        {PasswordResetRequired, {"com.vicr123.accounts.Error.PasswordResetRequired", "A password reset is required"}},
        {DisabledAccount, {"com.vicr123.accounts.Error.DisabledAccount", "The account is disabled"}},
        {TwoFactorEnabled, {"com.vicr123.accounts.Error.TwoFactorEnabled", "Two Factor Authentication is already enabled"}},
        {TwoFactorDisabled, {"com.vicr123.accounts.Error.TwoFactorDisabled", "Two Factor Authentication is already disabled"}},
        {TwoFactorRequired, {"com.vicr123.accounts.Error.TwoFactorRequired", "Two Factor Authentication is required"}}
    };

    QPair<QString, QString> errorStrings = errors.value(error);

    Utils::accountsBus().send(replyTo.createErrorReply(errorStrings.first, errorStrings.second));
}

QByteArray Utils::generateSalt() {
    quint32 saltBytes[33];
    QRandomGenerator::global()->fillRange(saltBytes);
    saltBytes[32] = '\0';

    QByteArray saltByteArray(reinterpret_cast<char*>(saltBytes), 32);
    return saltByteArray;
}

void Utils::sendTemplateEmail(QString templateName, QList<QString> recipients, QString locale, QMap<QString, QString> replacements) {
    QFuture<void> future = QtConcurrent::run([ = ](QPromise<void>& promise) {
        QString securityTypeString = qEnvironmentVariable("SMTP_SECURITY");
        SmtpClient::ConnectionType securityType;
        if (securityTypeString == "STARTTLS") {
            securityType = SmtpClient::TlsConnection;
        } else if (securityTypeString == "true") {
            securityType = SmtpClient::SslConnection;
        } else {
            securityType = SmtpClient::TcpConnection;
        }
        SmtpClient client(qEnvironmentVariable("SMTP_HOST"), qEnvironmentVariableIntValue("SMTP_PORT"), securityType);

        client.setUser(qEnvironmentVariable("SMTP_USERNAME"));
        client.setPassword(qEnvironmentVariable("SMTP_PASSWORD"));
        client.setAuthMethod(SmtpClient::AuthLogin);

        MimeMessage message;
        message.setSender(new EmailAddress(qEnvironmentVariable("SMTP_SENDER_EMAIL"), qEnvironmentVariable("SMTP_SENDER_NAME")));
        for (QString recipient : recipients) {
            message.addRecipient(new EmailAddress(recipient));
        }

        MailTemplate mailTemplate(templateName, locale, replacements);
        QSharedPointer<MimePart> htmlPart = mailTemplate.htmlPart();
        QSharedPointer<MimePart> textPart = mailTemplate.textPart();

        message.setSubject(mailTemplate.subject());
        message.addPart(htmlPart.data());
        message.addPart(textPart.data());

        if (!client.connectToHost()) {
            Logger::error() << "Could not connect to SMTP server";
            promise.finish();
            return;
        }

        if (!client.login()) {
            Logger::error() << "Could not log in to SMTP server";
            promise.finish();
            return;
        }

        if (!client.sendMail(message)) {
            Logger::error() << "Could not send email";
            promise.finish();
            return;
        }

        client.quit();

        promise.finish();
    });
}

QString Utils::otpKey(QString sharedKey, int offset) {
    char* decodedKey = new char[sharedKey.length() * 5 / 8];
    std::fill(decodedKey, decodedKey + sharedKey.length() * 5 / 8, 0);
    int next = 0;
    for (QChar c : sharedKey.toUpper()) {
        int bits;
        if (c.isNumber()) {
            bits = c.toLatin1() - '2' + 26;
        } else {
            bits = c.toLatin1() - 'A';
        }

        for (int i = 4; i >= 0; i--) {
            decodedKey[next / 8] |= (bits >> i & 0b1) << (7 - (next % 8));
            next++;
        }
    }

    QByteArray decodedBytes = QByteArray(reinterpret_cast<char*>(decodedKey), sharedKey.length() * 5 / 8);
    for (int i = 0; i < decodedBytes.size(); i++) {
        decodedBytes.data()[i] = qFromBigEndian(decodedBytes.data()[i]);
    }

    delete[] decodedKey;

    quint64 currentNumber = qToBigEndian(static_cast<quint64>(QDateTime::currentSecsSinceEpoch() / 30) + offset);
    QByteArray hmac = QMessageAuthenticationCode::hash(QByteArray(reinterpret_cast<char*>(&currentNumber), sizeof(quint64)), decodedBytes, QCryptographicHash::Sha1);
    char truncationStart = hmac.at(hmac.length() - 1) & 0xf;

    qint32 number = ((hmac.at(truncationStart) & 0x7f) << 24) |
        (hmac.at(truncationStart + 1) & 0xff) << 16 |
        (hmac.at(truncationStart + 2) & 0xff) << 8 |
        (hmac.at(truncationStart + 3) & 0xff);

    return QString::number(number % 1000000).rightJustified(6, '0', true);
}

QString Utils::generateSharedOtpKey() {
    QString validLetters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    QString key;
    for (int i = 0; i < 32; i++) {
        key.append(validLetters.at(QRandomGenerator::securelySeeded().bounded(validLetters.count())));
    }
    return key;
}

bool Utils::isValidOtpKey(QString otpKey, QString sharedKey) {
    return otpKey == Utils::otpKey(sharedKey, -1) ||
        otpKey == Utils::otpKey(sharedKey, 0) ||
        otpKey == Utils::otpKey(sharedKey, 1);
}
