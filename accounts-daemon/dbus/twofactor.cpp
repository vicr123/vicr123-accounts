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
#include "twofactor.h"

#include <QDBusMetaType>
#include <QSqlQuery>
#include <QtConcurrent>
#include "useraccount.h"
#include "utils.h"

struct TwoFactorPrivate {
    UserAccount* parent;
    bool enabled = false;
    QString secretKey;
    QList<OtpBackupKeys> backups;
};

QDBusArgument& operator<<(QDBusArgument& arg, const OtpBackupKeys& keys) {
    arg.beginStructure();
    arg << keys.key << keys.used;
    arg.endStructure();
    return arg;
}

const QDBusArgument& operator>>(const QDBusArgument& arg, OtpBackupKeys& keys) {
    arg.beginStructure();
    arg >> keys.key >> keys.used;
    arg.endStructure();
    return arg;
}

TwoFactor::TwoFactor(UserAccount* parent) : QDBusAbstractAdaptor(parent) {
    d = new TwoFactorPrivate();
    d->parent = parent;

    qRegisterMetaType<OtpBackupKeys>();
    qRegisterMetaType<QList<OtpBackupKeys>>();
    qDBusRegisterMetaType<OtpBackupKeys>();
    qDBusRegisterMetaType<QList<OtpBackupKeys>>();

    QSqlQuery query;
    query.prepare("SELECT * FROM otp WHERE userid=:id");
    query.bindValue(":id", d->parent->id());
    query.exec();

    if (query.next()) {
        d->secretKey = query.value("otpkey").toString();
        d->enabled = query.value("enabled").toBool();
    }

    QSqlQuery backupsQuery;
    backupsQuery.prepare("SELECT * FROM otpbackup WHERE userid=:id");
    backupsQuery.bindValue(":id", d->parent->id());
    backupsQuery.exec();

    while (backupsQuery.next()) {
        d->backups.append({
            backupsQuery.value("backupkey").toString(),
            backupsQuery.value("used").toBool()
        });
    }
}

TwoFactor::~TwoFactor() {
    delete d;
}

bool TwoFactor::twoFactorEnabled() {
    return d->enabled;
}

QString TwoFactor::secretKey() {
    return d->secretKey;
}

QList<OtpBackupKeys> TwoFactor::backupKeys() {
    return d->backups;
}

QString TwoFactor::GenerateTwoFactorKey(const QDBusMessage& message) {
    if (d->enabled) {
        //2FA should be disabled first
        Utils::sendDbusError(Utils::TwoFactorEnabled, message);
        return "";
    }

    QString newKey = Utils::generateSharedOtpKey();

    QSqlQuery query;
    query.prepare("INSERT INTO otp(userid, otpkey, enabled) VALUES(:id, :otpkey, :enabled) ON CONFLICT ON CONSTRAINT otp_pkey DO UPDATE SET otpkey=:otpkey, enabled=:enabled WHERE otp.userid=:id");
    query.bindValue(":id", d->parent->id());
    query.bindValue(":otpkey", newKey);
    query.bindValue(":enabled", false);

    if (!query.exec()) {
        Utils::sendDbusError(Utils::QueryError, message);
        return "";
    }

    d->secretKey = newKey;
    emit SecretKeyChanged(d->secretKey);

    return newKey;
}

void TwoFactor::EnableTwoFactorAuthentication(QString otpKey, const QDBusMessage& message) {
    if (d->enabled) {
        //2FA should be disabled first
        Utils::sendDbusError(Utils::TwoFactorEnabled, message);
        return;
    }

    if (!Utils::isValidOtpKey(otpKey, d->secretKey)) {
        Utils::sendDbusError(Utils::TwoFactorRequired, message);
        return;
    }


    QSqlQuery query;
    query.prepare("UPDATE otp SET enabled=:enabled WHERE otp.userid=:id");
    query.bindValue(":id", d->parent->id());
    query.bindValue(":enabled", true);

    if (!query.exec()) {
        Utils::sendDbusError(Utils::QueryError, message);
        return;
    }

    d->enabled = true;
    emit TwoFactorEnabledChanged(d->enabled);
    this->RegenerateBackupKeys(message);
}

void TwoFactor::DisableTwoFactorAuthentication(const QDBusMessage& message) {
    if (!d->enabled) {
        //2FA should be enabled first
        Utils::sendDbusError(Utils::TwoFactorDisabled, message);
        return;
    }

    QSqlQuery query;
    query.prepare("UPDATE otp SET enabled=:enabled WHERE otp.userid=:id");
    query.bindValue(":id", d->parent->id());
    query.bindValue(":enabled", false);

    if (!query.exec()) {
        Utils::sendDbusError(Utils::QueryError, message);
        return;
    }

    d->enabled = false;
    emit TwoFactorEnabledChanged(d->enabled);
}

void TwoFactor::RegenerateBackupKeys(const QDBusMessage& message) {
    if (!d->enabled) {
        //2FA should be enabled first
        Utils::sendDbusError(Utils::TwoFactorDisabled, message);
        return;
    }

    QSqlDatabase db;
    db.transaction();

    QSqlQuery deleteQuery;
    deleteQuery.prepare("DELETE FROM otpbackup WHERE userid=:id");
    deleteQuery.bindValue(":id", d->parent->id());
    if (!deleteQuery.exec()) {
        db.rollback();
        Utils::sendDbusError(Utils::QueryError, message);
        return;
    }

    QList<OtpBackupKeys> backups;
    for (int i = 0; i < 10; i++) {
        quint32 backup = QRandomGenerator::system()->generate();
        QString key;
        for (int j = 0; j < 4; j++) {
            key.append(QString::number((backup >> (j * 8)) & 0xFF).rightJustified(3, '0'));
        }
        backups.append({key, false});
    }

    QSqlQuery query;
    query.prepare("INSERT INTO otpbackup(userid, backupkey, used) VALUES(:id, :backupkey, :used)");
    query.bindValue(":id", QVariant::fromValue(QList<quint64>().fill(d->parent->id(), 10)));
    query.bindValue(":backupkey", QtConcurrent::blockingMapped(backups, [ = ](OtpBackupKeys key) {
        return key.key;
    }));
    query.bindValue(":used", QVariant::fromValue(QList<bool>().fill(false, 10)));

    if (!query.execBatch()) {
        db.rollback();
        Utils::sendDbusError(Utils::QueryError, message);
        return;
    }

    if (db.commit()) {
        Utils::sendDbusError(Utils::QueryError, message);
        return;
    }

    d->backups = backups;
    emit BackupKeysChanged(backups);
}
