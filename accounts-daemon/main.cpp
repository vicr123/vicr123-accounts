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

#include "database.h"
#include "dbus/accountmanager.h"
#include "dbusdaemon.h"
#include "utils.h"
#include <QDBusConnection>
#include <QFile>
#include <QProcess>
#include <QSqlDatabase>
#include <logger.h>

int main(int argc, char* argv[]) {
    QCoreApplication a(argc, argv);

    Database* db = new Database();
    if (!db->init()) {
        return 1;
    }

    QSettings settings(QStringLiteral(SYSCONFDIR).append("/vicr123-accounts.conf"), QSettings::IniFormat);
    if (settings.value("dbus/bus").toString() == "dedicated") {
        new DBusDaemon(qEnvironmentVariable("DBUS_CONFIGURATION_FILE", settings.value("dbus/configuration").toString()));
        QDBusConnection::connectToBus("unix:path=/var/vicr123-accounts/vicr123-accounts-bus", "accounts");
    }

    if (!Utils::accountsBus().registerService("com.vicr123.accounts")) {
        Logger::error() << "Could not register service on bus";
    }

    AccountManager* accountManager = new AccountManager();

    return a.exec();
}
