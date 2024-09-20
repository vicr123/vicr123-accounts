cmake_minimum_required(VERSION 3.12.0)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

project(vicr123-accounts VERSION 1.0.0 LANGUAGES CXX)

IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    link_directories(/usr/local/lib)
ENDIF()

add_subdirectory(SMTPEmail)
add_subdirectory(QJsonWebToken)
add_subdirectory(accounts-daemon)
