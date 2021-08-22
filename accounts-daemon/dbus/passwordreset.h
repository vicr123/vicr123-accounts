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
#ifndef PASSWORDRESET_H
#define PASSWORDRESET_H

#include <QDBusAbstractAdaptor>
#include <QDBusMessage>

struct ResetMethod {
    QString type;
    QVariantMap challenge;
};
Q_DECLARE_METATYPE(QList<ResetMethod>)

class UserAccount;
struct PasswordResetPrivate;
class PasswordReset : public QDBusAbstractAdaptor {
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "com.vicr123.accounts.PasswordReset");
    public:
        explicit PasswordReset(UserAccount* parent);
        ~PasswordReset();

    public slots:
        Q_SCRIPTABLE QList<ResetMethod> ResetMethods(const QDBusMessage& message);
        Q_SCRIPTABLE void ResetPassword(QString type, QVariantMap challenge, const QDBusMessage& message);

    signals:

    private:
        PasswordResetPrivate* d;
        void issuePasswordReset();
};

#endif // PASSWORDRESET_H
