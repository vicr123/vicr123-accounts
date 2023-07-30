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
#ifndef UTILS_H
#define UTILS_H

#include "src/mimemessage.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QFuture>

namespace Utils {
    enum DBusError {
        NoError,
        InternalError,
        NoAccount,
        QueryError,
        IncorrectPassword,
        PasswordResetRequired,
        PasswordResetRequestRequired,
        DisabledAccount,
        TwoFactorEnabled,
        TwoFactorDisabled,
        TwoFactorRequired,
        VerificationCodeIncorrect,
        InvalidInput,
        FidoSupportUnavailable,
        AccountEmailNotVerified,
        EmailError
    };

    QString settingsFile();
    QDBusConnection accountsBus();
    QString fidoHelperPath();
    QByteArray generateRandomBytes(int count);
    QByteArray generateSalt();
    QString generateHashedPassword(QString password, int iterations = 10000);
    bool verifyHashedPassword(QString password, QString hash);
    void sendDbusError(DBusError error, const QDBusMessage& replyTo);
    void sendTemplateEmail(QString templateName, QList<QString> recipients, QString locale, QMap<QString, QString> replacements);
    QFuture<void> sendMailMessage(MimeMessage* message);
    QString otpKey(QString sharedKey, int offset = 0);
    QString generateSharedOtpKey();
    bool isValidOtpKey(QString otpKey, QString sharedKey);
    bool sendVerificationEmail(quint64 user);
} // namespace Utils

#endif // UTILS_H
