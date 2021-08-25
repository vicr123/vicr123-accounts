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
#ifndef ACCOUNTMANAGER_H
#define ACCOUNTMANAGER_H

#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <QDBusMessage>

struct AccountManagerPrivate;
class AccountManager : public QObject {
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "com.vicr123.accounts.Manager");
    public:
        explicit AccountManager();
        ~AccountManager();

        quint64 userIdByUsername(QString username);

    public slots:
        Q_SCRIPTABLE QDBusObjectPath CreateUser(QString username, QString password, QString email, const QDBusMessage& message);
        Q_SCRIPTABLE QDBusObjectPath UserById(quint64 id, const QDBusMessage& message);
        Q_SCRIPTABLE quint64 UserIdByUsername(QString username, const QDBusMessage& message);
        Q_SCRIPTABLE QString ProvisionToken(QString username, QString password, QString application, QVariantMap extraOptions, const QDBusMessage& message);
        Q_SCRIPTABLE QDBusObjectPath UserForToken(QString token, const QDBusMessage& message);
        Q_SCRIPTABLE QList<quint64> AllUsers(const QDBusMessage& message);

    signals:

    private:
        AccountManagerPrivate* d;

};

#endif // ACCOUNTMANAGER_H
