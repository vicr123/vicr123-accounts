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
#ifndef USER_H
#define USER_H

#include <QDBusMessage>
#include <QDBusAbstractAdaptor>

class UserAccount;
struct UserPrivate;
class User : public QDBusAbstractAdaptor {
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "com.vicr123.accounts.User");

        Q_SCRIPTABLE Q_PROPERTY(quint64 Id READ id)
        Q_SCRIPTABLE Q_PROPERTY(QString Username READ username NOTIFY UsernameChanged);
        Q_SCRIPTABLE Q_PROPERTY(QString Email READ email NOTIFY EmailChanged);
        Q_SCRIPTABLE Q_PROPERTY(bool Verified READ verified NOTIFY VerifiedChanged);

    public:
        explicit User(UserAccount* parent);
        ~User();

        quint64 id();
        QString username();
        QString email();
        bool verified();

    public slots:
        Q_SCRIPTABLE void SetUsername(QString username, const QDBusMessage& message);
        Q_SCRIPTABLE void SetPassword(QString password, const QDBusMessage& message);
        Q_SCRIPTABLE void SetEmail(QString email, const QDBusMessage& message);
        Q_SCRIPTABLE void ResendVerificationEmail(const QDBusMessage& message);
        Q_SCRIPTABLE void VerifyEmail(QString verificationCode, const QDBusMessage& message);
        Q_SCRIPTABLE bool VerifyPassword(QString password, const QDBusMessage& message);

    signals:
        Q_SCRIPTABLE void UsernameChanged(QString oldUsername, QString newUsername);
        Q_SCRIPTABLE void EmailChanged(QString newEmail);
        Q_SCRIPTABLE void VerifiedChanged(bool verified);

    private:
        UserPrivate* d;
};

#endif // USER_H
