/****************************************
 *
 *   INSERT-PROJECT-NAME-HERE - INSERT-GENERIC-NAME-HERE
 *   Copyright (C) 2022 Victor Tran
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
#ifndef FIDO2_H
#define FIDO2_H

#include <QCoroTask>
#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QObject>

class UserAccount;
struct Fido2Private;
class Fido2 : public QDBusAbstractAdaptor {
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "com.vicr123.accounts.Fido2");

    public:
        explicit Fido2(UserAccount* parent);
        ~Fido2();

        struct Fido2Key {
                int id;
                QString application;
                QString name;
        };

    public slots:
        Q_SCRIPTABLE QString PrepareRegister(QString application, QString rp, int authenticatorAttachment, const QDBusMessage& message);
        Q_SCRIPTABLE void CompleteRegister(QString response, QStringList expectOrigins, QString keyName, const QDBusMessage& message);
        Q_SCRIPTABLE QList<Fido2::Fido2Key> GetKeys(const QDBusMessage& message);
        Q_SCRIPTABLE void DeleteKey(int id, const QDBusMessage& message);

    private:
        Fido2Private* d;

        QCoro::Task<QString> privatePrepareRegister(QString application, QString rp, int authenticatorAttachment, const QDBusMessage& message);
        QCoro::Task<> privateCompleteRegister(QString response, QStringList expectOrigins, QString keyName, const QDBusMessage& message);
};

Q_DECLARE_METATYPE(Fido2::Fido2Key)
Q_DECLARE_METATYPE(QList<Fido2::Fido2Key>)

#endif // FIDO2_H
