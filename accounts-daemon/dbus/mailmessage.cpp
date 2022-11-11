//
// Created by victor on 10/11/22.
//

#include "mailmessage.h"
#include "src/mimehtml.h"
#include "src/mimemessage.h"
#include "utils.h"
#include <QDBusMetaType>
#include <QFutureWatcher>

struct MailMessagePrivate {
        static quint64 nextId;
        QString path;
//        QSharedPointer<MimeMessage> message;

        QString to;
        QString subject;
        QString htmlContent;
        QString textContent;

        QString from;
        QString fromAddress;
};

quint64 MailMessagePrivate::nextId = 0;

MailMessage::MailMessage(const QString& to, QObject* parent) :
    QObject(parent) {
    qDBusRegisterMetaType<QPair<QString, QString>>();

    d = new MailMessagePrivate();
    d->path = QStringLiteral("/com/vicr123/accounts/mail/Message%1").arg(d->nextId++);
    d->from = qEnvironmentVariable("SMTP_SENDER_NAME");
    d->fromAddress = qEnvironmentVariable("SMTP_SENDER_EMAIL");
    d->to = to;

    Utils::accountsBus().registerObject(d->path, this, QDBusConnection::ExportScriptableContents);
}

MailMessage::~MailMessage() {
    delete d;
}

QDBusObjectPath MailMessage::path() {
    return QDBusObjectPath(d->path);
}

void MailMessage::setSubject(const QString& subject) {
    d->subject = subject;
}

QString MailMessage::subject() {
    return d->subject;
}

[[maybe_unused]] void MailMessage::Send(const QDBusMessage& message) {
    message.setDelayedReply(true);

    auto mailMessage = QSharedPointer<MimeMessage>(new MimeMessage());
    mailMessage->addTo(new EmailAddress(d->to));
    mailMessage->setSender(new EmailAddress(d->fromAddress, d->from));
    mailMessage->setSubject(d->subject);

    auto* htmlPart = new MimeHtml();
    htmlPart->setHtml(d->htmlContent);

    auto textPart = new MimeText();
    textPart->setText(d->textContent);

    mailMessage->addPart(htmlPart);
    mailMessage->addPart(textPart);

    auto watcher = new QFutureWatcher<void>(this);
    watcher->setFuture(Utils::sendMailMessage(mailMessage));
    connect(watcher, &QFutureWatcher<void>::finished, this, [message, this, watcher] {
        if (watcher->isCanceled()) return;

        auto reply = message.createReply();
        Utils::accountsBus().send(reply);

        this->deleteLater();
    });
    connect(watcher, &QFutureWatcher<void>::canceled, this, [message] {
        Utils::sendDbusError(Utils::EmailError, message);
    });
}

[[maybe_unused]] void MailMessage::Discard(const QDBusMessage& message) {
    this->deleteLater();
}

void MailMessage::setFrom(const QPair<QString, QString>& from) {
    d->fromAddress = from.first;
    d->from = from.second;
}

QPair<QString, QString> MailMessage::from() {
    return {
        d->fromAddress,
            d->from
    };
}

QString MailMessage::htmlContent() {
    return d->htmlContent;
}

void MailMessage::setHtmlContent(QString htmlContent) {
    d->htmlContent = htmlContent;
}

QString MailMessage::textContent() {
    return d->textContent;
}

void MailMessage::setTextContent(QString textContent) {
    d->textContent = textContent;
}
