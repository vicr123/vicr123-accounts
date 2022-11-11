project(lib VERSION 1.0.0 LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Network)

set(SOURCES
        src/emailaddress.cpp
        src/mimeattachment.cpp
        src/mimefile.cpp
        src/mimehtml.cpp
        src/mimeinlinefile.cpp
        src/mimemessage.cpp
        src/mimepart.cpp
        src/mimetext.cpp
        src/smtpclient.cpp
        src/quotedprintable.cpp
        src/mimemultipart.cpp
        src/mimecontentencoder.cpp
        src/mimebase64encoder.cpp
        src/mimeqpencoder.cpp
        src/mimeqpformatter.cpp
        src/mimebase64formatter.cpp
        src/mimecontentformatter.cpp
)

set(HEADERS
        src/emailaddress.h
        src/mimeattachment.h
        src/mimefile.h
        src/mimehtml.h
        src/mimeinlinefile.h
        src/mimemessage.h
        src/mimepart.h
        src/mimetext.h
        src/smtpclient.h
        src/SmtpMime
        src/quotedprintable.h
        src/mimemultipart.h
        src/smtpmime_global.h
        src/mimecontentencoder.h
        src/mimebase64encoder.h
        src/mimeqpencoder.h
        src/mimeqpformatter.h
        src/mimebase64formatter.h
        src/mimecontentformatter.h
)

add_library(smtpemail STATIC ${SOURCES} ${HEADERS})
target_link_libraries(smtpemail Qt6::Core Qt6::Network)

target_compile_definitions(smtpemail PRIVATE SMTP_MIME_LIBRARY)
