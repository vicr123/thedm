QT -= gui
QT += thelib network

LIBS += -lthe-libs

CONFIG += c++11 console
CONFIG -= app_bundle

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        main.cpp \
    manageddisplay.cpp \
    seatmanager.cpp \
    sddm/virtualterminal.cpp

HEADERS += \
    manageddisplay.h \
    seatmanager.h \
    sddm/virtualterminal.h

DISTFILES += \
    thedm.service \
    pam/thedm \
    thedm_org.freedesktop.DisplayManager.conf

dbuscpptoxml.output = dbus_${QMAKE_FILE_BASE}.xml
dbuscpptoxml.commands = qdbuscpp2xml -a -o ${QMAKE_FILE_OUT} ${QMAKE_FILE_NAME}
dbuscpptoxml.input = DBUS_ADAPTORS_CPP
dbuscpptoxml.CONFIG = no_link target_predeps
QMAKE_EXTRA_COMPILERS += dbuscpptoxml

DBUS_ADAPTORS_CPP = seatmanager.h

DBUS_ADAPTORS = dbus_seatmanager.xml

unix {
    target.path = /usr/bin/

    #translations.files = translations/*
    #translations.path = /usr/share/thedm/translations

    systemd.files = thedm.service
    systemd.path = /usr/lib/systemd/system

    dbus.files = thedm_org.freedesktop.DisplayManager.conf
    dbus.path = /usr/share/dbus-1/system.d

    pam.files = pam/thedm
    pam.path = /etc/pam.d/

    INSTALLS += target systemd pam #translations
}
