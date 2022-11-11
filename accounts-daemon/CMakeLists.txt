project(vicr123accounts VERSION 1.0.0 LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS DBus Sql Network Concurrent)
include(GNUInstallDirs)

set(SOURCES
    database.cpp
    dbus/accountmanager.cpp
    dbus/passwordreset.cpp
    dbus/twofactor.cpp
    dbus/user.cpp
    dbus/useraccount.cpp
    dbus/fido2.cpp
    dbus/mailmessage.cpp
    dbusdaemon.cpp
    logger.cpp
    mailtemplate.cpp
    main.cpp
    utils.cpp
    validation.cpp
    
    resources.qrc
    fidoutils.cpp
)

set(HEADERS
    database.h
    dbus/accountmanager.h
    dbus/passwordreset.h
    dbus/twofactor.h
    dbus/user.h
    dbus/useraccount.h
    dbus/fido2.h
    dbus/mailmessage.h
    dbusdaemon.h
    logger.h
    mailtemplate.h
    utils.h
    validation.h
    fidoutils.h
)

add_executable(vicr123accounts ${SOURCES} ${HEADERS})

target_link_libraries(vicr123accounts Qt6::DBus Qt6::Sql Qt6::Network Qt6::Concurrent smtpemail)
target_include_directories(vicr123accounts PUBLIC ../SMTPEmail/)
target_compile_definitions(vicr123accounts PRIVATE SYSCONFDIR=\"${CMAKE_INSTALL_FULL_SYSCONFDIR}\")

set_target_properties(vicr123accounts PROPERTIES
    OUTPUT_NAME vicr123-accounts)

install(TARGETS vicr123accounts
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES vicr123-accounts.conf vicr123-accounts-dbus-config.conf
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR})

install(DIRECTORY mail
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/vicr123-accounts/)
