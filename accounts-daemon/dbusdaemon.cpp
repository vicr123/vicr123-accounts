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
#include "dbusdaemon.h"

#include <sys/prctl.h>
#include <signal.h>
#include <QProcess>
#include <QThread>
#include <QFile>

struct DBusDaemonPrivate {
    QProcess* daemonProcess;
};

DBusDaemon::DBusDaemon(QString configurationFile, QObject* parent) : QObject(parent) {
    d = new DBusDaemonPrivate();
    d->daemonProcess = new QProcess();
    d->daemonProcess->setChildProcessModifier([] {
        prctl(PR_SET_PDEATHSIG, SIGTERM);
    });
    d->daemonProcess->start("dbus-daemon", {"--nofork", QStringLiteral("--config-file=%1").arg(configurationFile)});
    d->daemonProcess->waitForStarted();

    //HACK: Race condition!!!
    QThread::msleep(500);
}

DBusDaemon::~DBusDaemon() {
    d->daemonProcess->terminate();
    d->daemonProcess->deleteLater();
    delete d;
}
