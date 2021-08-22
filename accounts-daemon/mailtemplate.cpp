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
#include "mailtemplate.h"

#include <QSettings>
#include <QDir>
#include <QMap>
#include <QJsonObject>
#include <QJsonDocument>

#include <src/SmtpMime>

struct MailTemplatePrivate {
    QJsonObject metadata;
    QString textContent;
    QString htmlContent;
    QMap<QString, QString> replacements;
};

MailTemplate::MailTemplate(QString templateName, QString locale, QMap<QString, QString> replacements, QObject* parent) : QObject(parent) {
    d = new MailTemplatePrivate();
    d->replacements = replacements;

    QSettings settings("/etc/vicr123-accounts.conf", QSettings::IniFormat);
    QDir maildir(settings.value("mail/maildir").toString());

    QFile mailfile = maildir.absoluteFilePath(QStringLiteral("%1/%2").arg(locale, templateName));
    if (!mailfile.exists()) mailfile.setFileName(maildir.absoluteFilePath(QStringLiteral("%1/%2").arg(locale, templateName)));

    mailfile.open(QFile::ReadOnly);
    QStringList fileParts = QString(mailfile.readAll()).split("---");
    d->metadata = QJsonDocument::fromJson(fileParts.first().trimmed().toUtf8()).object();
    d->textContent = performReplacements(fileParts.at(1).trimmed());
    d->htmlContent = performReplacements(fileParts.at(2).trimmed());
}

MailTemplate::~MailTemplate() {
    delete d;
}

QString MailTemplate::subject() {
    return d->metadata.value("subject").toString();
}

QSharedPointer<MimePart> MailTemplate::textPart() {
    MimeText* textPart = new MimeText();
    textPart->setText(d->textContent.toUtf8());
    return QSharedPointer<MimePart>(textPart);
}

QSharedPointer<MimePart> MailTemplate::htmlPart() {
    MimeHtml* htmlPart = new MimeHtml();
    htmlPart->setHtml(d->htmlContent.toUtf8());
    return QSharedPointer<MimePart>(htmlPart);
}

QString MailTemplate::performReplacements(QString sourceString) {
    for (QString replacement : d->replacements.keys()) {
        sourceString.replace(QStringLiteral("{%1}").arg(replacement), d->replacements.value(replacement));
    }
    return sourceString;
}
