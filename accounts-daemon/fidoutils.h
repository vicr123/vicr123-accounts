#ifndef VICR123_ACCOUNTS_FIDOUTILS_H
#define VICR123_ACCOUNTS_FIDOUTILS_H

#include <QByteArray>

namespace FidoUtils {
    QStringList FidoCredsForUser(quint64 userId);
    QStringList FidoCredsForUser(quint64 userId, QString application);
}

#endif // VICR123_ACCOUNTS_FIDOUTILS_H
