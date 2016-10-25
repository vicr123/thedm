#-------------------------------------------------
#
# Project created by QtCreator 2016-10-19T22:21:21
#
#-------------------------------------------------

QT       += core gui x11extras dbus
CONFIG   += c++11
LIBS     += -lX11 -lpam -lpam_misc

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = thedm
TEMPLATE = app

DBUS_ADAPTORS += org.freedesktop.DisplayManager.Seat.xml

SOURCES += main.cpp\
        mainwindow.cpp \
    coverframe.cpp \
    pam.cpp \
    dbusseat.cpp

HEADERS  += mainwindow.h \
    coverframe.h \
    pam.h \
    dbusseat.h

FORMS    += mainwindow.ui

RESOURCES += \
    resources.qrc
