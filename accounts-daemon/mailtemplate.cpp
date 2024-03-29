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

#include "utils.h"

#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QSettings>

#include <src/SmtpMime>

struct MailTemplatePrivate {
        QJsonObject metadata;
        QString textContent;
        QString htmlContent;
        QMap<QString, QString> replacements;
};

MailTemplate::MailTemplate(QString templateName, QString locale, QMap<QString, QString> replacements, QObject* parent) :
    QObject(parent) {
    d = new MailTemplatePrivate();
    d->replacements = replacements;

    QSettings settings(Utils::settingsFile(), QSettings::IniFormat);
    QDir maildir(qEnvironmentVariable("MAIL_MAILDIR", settings.value("mail/maildir").toString()));

    QDir templatePath(maildir.absoluteFilePath(QStringLiteral("%1/%2").arg(locale, templateName)));

    QFile mailMeta = templatePath.absoluteFilePath("meta.json");
    if (!mailMeta.open(QFile::ReadOnly)) {
        //TODO: Figure out what to do if the email can't be opened
        return;
    }

    d->metadata = QJsonDocument::fromJson(mailMeta.readAll()).object();
    mailMeta.close();

    QFile textPart = templatePath.absoluteFilePath(d->metadata.value("text").toString());
    textPart.open(QFile::ReadOnly);
    d->textContent = performReplacements(QString(textPart.readAll()).trimmed());
    textPart.close();

    QFile htmlPart = templatePath.absoluteFilePath(d->metadata.value("html").toString());
    htmlPart.open(QFile::ReadOnly);
    d->htmlContent = performReplacements(QString(htmlPart.readAll()).trimmed());
    htmlPart.close();
}

MailTemplate::~MailTemplate() {
    delete d;
}

QString MailTemplate::subject() {
    return d->metadata.value("subject").toString();
}

MimePart* MailTemplate::textPart() {
    MimeText* textPart = new MimeText();
    textPart->setText(d->textContent.toUtf8());
    return textPart;
}

MimePart* MailTemplate::htmlPart() {
    MimeHtml* htmlPart = new MimeHtml();
    htmlPart->setHtml(d->htmlContent.toUtf8());
    return htmlPart;
}

QString MailTemplate::performReplacements(QString sourceString) {
    for (QString replacement : d->replacements.keys()) {
        sourceString.replace(QStringLiteral("{%1}").arg(replacement), d->replacements.value(replacement));
    }
    return sourceString;
}
