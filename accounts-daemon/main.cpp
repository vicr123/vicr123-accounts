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
#include <QCoreApplication>
#include <QSettings>

#include <QFile>
#include <QProcess>
#include <QDBusConnection>
#include <QSqlDatabase>
#include <logger.h>
#include "dbus/accountmanager.h"
#include "dbusdaemon.h"
#include "database.h"
#include "utils.h"

int main(int argc, char* argv[]) {
    QCoreApplication a(argc, argv);

    Database* db = new Database();
    if (!db->init()) {
        return 1;
    }

    QSettings settings("/etc/vicr123-accounts.conf", QSettings::IniFormat);
    if (settings.value("dbus/configuration").toString() == "dedicated") {
        new DBusDaemon(settings.value("dbus/configuration").toString());
        QDBusConnection::connectToBus("unix:path=/tmp/vicr123-accounts-bus", "accounts");
    }

    if (!Utils::accountsBus().registerService("com.vicr123.accounts")) {
        Logger::error() << "Could not register service on bus";
    }

    AccountManager* accountManager = new AccountManager();

    QString test = Utils::otpKey("JBSWY3DPEHPK3PXP");
    QString test2 = Utils::generateSharedOtpKey();

    return a.exec();
}
