/****************************************
 *
 *   theShell - Desktop Environment
 *   Copyright (C) 2019 Victor Tran
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * *************************************/

#ifndef NATIVEEVENTFILTER_H
#define NATIVEEVENTFILTER_H

#include <QObject>
#include <QAbstractNativeEventFilter>
#include <xcb/xcb.h>
#include <xcb/xcb_atom.h>
#include <QX11Info>
#include <QProcess>
#include <QTime>
#include <math.h>
#include <QIcon>
#include <QDBusUnixFileDescriptor>
#include <QDBusMessage>
#include <QDBusConnection>
#include <QDBusReply>
#include <QDebug>
#include <QMessageBox>
#include "mainwindow.h"

#include <X11/XF86keysym.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>

class MainWindow;

class NativeEventFilter : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
public:
    explicit NativeEventFilter(QObject* parent = 0);
    ~NativeEventFilter();

signals:
    void PowerPressed();

public slots:
    void handlePowerButton();

private:
    bool nativeEventFilter(const QByteArray &eventType, void *message, long *result);

    QTime lastPress;

    bool isEndSessionBoxShowing = false;
    bool ignoreSuper = false;

    QTimer* powerButtonTimer = nullptr;
    bool powerPressed = false;

    QDBusUnixFileDescriptor powerInhibit;
};

#endif // NATIVEEVENTFILTER_H
