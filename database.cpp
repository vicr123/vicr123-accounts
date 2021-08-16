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

#include <QSettings>
#include <QSqlDatabase>
#include <QFile>
#include <QSqlQuery>
#include "logger.h"

Database::Database(QObject* parent) : QObject(parent) {

}

bool Database::init() {
    QSettings settings("/etc/vicr123-accounts.conf", QSettings::IniFormat);
    if (!QSqlDatabase::isDriverAvailable(settings.value("database/driver").toString())) {
        Logger::error() << "The database driver is not available.";
        return false;
    }

    QSqlDatabase db = QSqlDatabase::addDatabase(settings.value("database/driver").toString());
    db.setHostName(settings.value("database/hostname").toString());
    db.setDatabaseName(settings.value("database/database").toString());
    db.setUserName(settings.value("database/username").toString());
    db.setPassword(settings.value("database/password").toString());
    if (!db.open()) {
        Logger::error() << "Could not connect to the database.";
        return false;
    }

    //Initialise the database
    if (!db.tables().contains("version")) {
        this->runSqlScript("init");
    } else {
        //See if we need to migrate the database
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
