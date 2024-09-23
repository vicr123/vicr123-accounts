# Need to provide our own CMake rules because the default rules in QJsonWebToken target Qt 5

find_package(Qt6 REQUIRED COMPONENTS Core)
add_library(QJsonWebToken STATIC
        QJsonWebToken/src/qjsonwebtoken.h QJsonWebToken/src/qjsonwebtoken.cpp
)
target_link_libraries(QJsonWebToken PRIVATE Qt::Core)
target_include_directories(QJsonWebToken INTERFACE QJsonWebToken/src)