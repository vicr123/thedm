#-------------------------------------------------
#
# Project created by QtCreator 2016-10-19T22:21:21
#
#-------------------------------------------------

QT       += core gui x11extras dbus
CONFIG   += c++11
LIBS     += -lX11 -lpam

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = thedm
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    coverframe.cpp \
    pam.cpp

HEADERS  += mainwindow.h \
    coverframe.h \
    pam.h

FORMS    += mainwindow.ui

RESOURCES += \
    resources.qrc
