#include "fidoutils.h"

#include <QList>
#include <QSqlQuery>

QStringList FidoUtils::FidoCredsForUser(quint64 userId) {
    QSqlQuery query;
    query.prepare("SELECT data FROM fido WHERE userid=:userid");
    query.bindValue(":userid", userId);

    query.exec();

    QStringList creds;
    while (query.next()) {
        creds.append(query.value("data").toByteArray().toBase64());
    }
    return creds;
}

QStringList FidoUtils::FidoCredsForUser(quint64 userId, QString application) {
    QSqlQuery query;
    query.prepare("SELECT data FROM fido WHERE userid=:userid AND application=:application");
    query.bindValue(":userid", userId);
    query.bindValue(":application", application);

    query.exec();

    QStringList creds;
    while (query.next()) {
        creds.append(query.value("data").toByteArray().toBase64());
    }
    return creds;
}