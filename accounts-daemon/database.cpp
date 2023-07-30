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
#include "database.h"

#include "logger.h"
#include "utils.h"
#include <QFile>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QThread>

Database::Database(QObject* parent) :
    QObject(parent) {
}

bool Database::init() {
    QSettings settings(Utils::settingsFile(), QSettings::IniFormat);
    if (!QSqlDatabase::isDriverAvailable(settings.value("database/driver").toString())) {
        Logger::error() << "The database driver is not available.";
        return false;
    }

    QSqlDatabase db = QSqlDatabase::addDatabase(qEnvironmentVariable("ACCOUNTS_DB_DRIVER", settings.value("database/driver").toString()));
    db.setHostName(qEnvironmentVariable("ACCOUNTS_DB_HOSTNAME", settings.value("database/hostname").toString()));
    db.setDatabaseName(qEnvironmentVariable("ACCOUNTS_DB_DATABASE", settings.value("database/database").toString()));
    db.setUserName(qEnvironmentVariable("ACCOUNTS_DB_USERNAME", settings.value("database/username").toString()));
    db.setPassword(qEnvironmentVariable("ACCOUNTS_DB_PASSWORD", settings.value("database/password").toString()));

    while (!db.open()) {
        Logger::error() << "Could not connect to the database\n";
        Logger::error() << "Trying again after 5 seconds.\n";
        QThread::sleep(5);
    }

    // Initialise the database
    if (!db.tables().contains("version")) {
        this->runSqlScript("init");
    } else {
        // See if we need to migrate the database
        auto versionQuery = db.exec("SELECT number FROM version");
        versionQuery.next();
        auto number = versionQuery.value("number").toInt();
        if (number == 1) {
            // Update to V2 required
            this->runSqlScript("v2");
        } else {
            //We are up to date!
        }
    }

    return true;
}

void Database::runSqlScript(QString script) {
    QFile scriptFile(QStringLiteral(":/sql/%1.sql").arg(script));
    scriptFile.open(QFile::ReadOnly);
    QString scriptContents = scriptFile.readAll();
    scriptFile.close();

    QSqlQuery query;
    query.exec(scriptContents);
}
