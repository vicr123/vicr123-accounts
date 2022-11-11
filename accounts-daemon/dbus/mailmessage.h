//
// Created by victor on 10/11/22.
//

#ifndef MAILMESSAGE_H
#define MAILMESSAGE_H

#include <QDBusAbstractAdaptor>
#include <QDBusMessage>
#include <QDBusObjectPath>

struct MailMessagePrivate;
class MailMessage : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.vicr123.accounts.MailMessage")

    Q_SCRIPTABLE Q_PROPERTY(QString Subject READ subject WRITE setSubject)
    Q_SCRIPTABLE Q_PROPERTY(QPair<QString, QString> From READ from WRITE setFrom)
    Q_SCRIPTABLE Q_PROPERTY(QString TextContent READ textContent WRITE setTextContent)
    Q_SCRIPTABLE Q_PROPERTY(QString HtmlContent READ htmlContent WRITE setHtmlContent)

public:
    explicit MailMessage(const QString& to, QObject* parent = nullptr);
    ~MailMessage();

    QDBusObjectPath path();

    QString subject();
    void setSubject(const QString& subject);

    QPair<QString, QString> from();
    void setFrom(const QPair<QString, QString>& from);

    QString htmlContent();
    void setHtmlContent(QString htmlContent);

    QString textContent();
    void setTextContent(QString textContent);

public slots:
    Q_SCRIPTABLE [[maybe_unused]] void Send(const QDBusMessage& message);
    Q_SCRIPTABLE [[maybe_unused]] void Discard(const QDBusMessage& message);

private:
    MailMessagePrivate* d;
};

#endif // MAILMESSAGE_H
