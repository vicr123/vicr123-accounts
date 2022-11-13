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
#include "validation.h"
#include <QRegularExpression>

static QRegularExpression usernameRegex = QRegularExpression(QRegularExpression::anchoredPattern("[A-Za-z0-9 \\-_.&,!\\[\\]{}()\"'~`@#$%^*?/\\\\]+"));
static QRegularExpression emailRegex = QRegularExpression(QRegularExpression::anchoredPattern("[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,4}"));

bool Validation::validateUsername(const QString& username) {
    if (username.isEmpty()) return false;
    if (username.length() > 32) return false;
    if (!usernameRegex.match(username).hasMatch()) return false;
    return true;
}
bool Validation::validatePassword(const QString& password) {
    if (password.isEmpty()) return false;
    if (password.length() > 256) return false;
    return true;
}
bool Validation::validateEmailAddress(const QString& email) {
    if (email.isEmpty()) return false;
    if (!emailRegex.match(email).hasMatch()) return false;
    return true;
}
