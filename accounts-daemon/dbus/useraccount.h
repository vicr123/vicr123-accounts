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
#ifndef USERACCOUNT_H
#define USERACCOUNT_H

#include <QObject>
#include <QDBusObjectPath>

class TwoFactor;
struct UserAccountPrivate;
class UserAccount : public QObject {
        Q_OBJECT
    public:
        ~UserAccount();

        static UserAccount* accountForId(quint64 id);

        quint64 id();
        QDBusObjectPath path();

        TwoFactor* twoFactor();

    signals:

    private:
        UserAccountPrivate* d;

        explicit UserAccount(quint64 id);
};

#endif // USERACCOUNT_H
