QT -= gui
QT += sql dbus network concurrent

CONFIG += c++11 console
CONFIG -= app_bundle

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        database.cpp \
        dbus/accountmanager.cpp \
        dbus/passwordreset.cpp \
        dbus/twofactor.cpp \
        dbus/user.cpp \
        dbus/useraccount.cpp \
        dbusdaemon.cpp \
        logger.cpp \
        mailtemplate.cpp \
        main.cpp \
        utils.cpp

unix {
    target.path = /usr/bin

    conf.files = vicr123-accounts.conf
    conf.path = /etc

    INSTALLS += target conf
}

DISTFILES += \
    sql/init.sql \
    vicr123-accounts.conf

HEADERS += \
    database.h \
    dbus/accountmanager.h \
    dbus/passwordreset.h \
    dbus/twofactor.h \
    dbus/user.h \
    dbus/useraccount.h \
    dbusdaemon.h \
    logger.h \
    mailtemplate.h \
    utils.h

RESOURCES += \
    resources.qrc

unix:!macx: LIBS += -L$$OUT_PWD/../SMTPEmail/ -lSMTPEmail

INCLUDEPATH += $$PWD/../SMTPEmail
DEPENDPATH += $$PWD/../SMTPEmail

unix:!macx: PRE_TARGETDEPS += $$OUT_PWD/../SMTPEmail/libSMTPEmail.a
