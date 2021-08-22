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
#ifndef TWOFACTOR_H
#define TWOFACTOR_H

#include <QDBusAbstractAdaptor>
#include <QDBusMessage>

struct OtpBackupKeys {
    QString key;
    bool used;
};
Q_DECLARE_METATYPE(QList<OtpBackupKeys>)

class UserAccount;
struct TwoFactorPrivate;
class TwoFactor : public QDBusAbstractAdaptor {
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "com.vicr123.accounts.TwoFactor");
        Q_SCRIPTABLE Q_PROPERTY(bool TwoFactorEnabled READ twoFactorEnabled NOTIFY TwoFactorEnabledChanged);
        Q_SCRIPTABLE Q_PROPERTY(QString SecretKey READ secretKey NOTIFY SecretKeyChanged);
        Q_SCRIPTABLE Q_PROPERTY(QList<OtpBackupKeys> BackupKeys READ backupKeys NOTIFY BackupKeysChanged);

    public:
        explicit TwoFactor(UserAccount* parent);
        ~TwoFactor();


        bool twoFactorEnabled();
        QString secretKey();
        QList<OtpBackupKeys> backupKeys();

    public slots:
        Q_SCRIPTABLE QString GenerateTwoFactorKey(const QDBusMessage& message);
        Q_SCRIPTABLE void EnableTwoFactorAuthentication(QString otpKey, const QDBusMessage& message);
        Q_SCRIPTABLE void DisableTwoFactorAuthentication(const QDBusMessage& message);
        Q_SCRIPTABLE void RegenerateBackupKeys(const QDBusMessage& message);

    signals:
        Q_SCRIPTABLE void TwoFactorEnabledChanged(bool enabled);
        Q_SCRIPTABLE void SecretKeyChanged(QString newSecretKey);
        Q_SCRIPTABLE void BackupKeysChanged(QList<OtpBackupKeys> newKeys);

    private:
        TwoFactorPrivate* d;
};

#endif // TWOFACTOR_H
