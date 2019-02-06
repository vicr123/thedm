/****************************************
 *
 *   INSERT-PROJECT-NAME-HERE - INSERT-GENERIC-NAME-HERE
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
#include "poweroptions.h"
#include "ui_poweroptions.h"

#include <QDBusMessage>
#include <QDBusConnection>
#include <QX11Info>

#include <X11/Xlib.h>
#include <X11/extensions/dpms.h>

PowerOptions::PowerOptions(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PowerOptions)
{
    ui->setupUi(this);
}

PowerOptions::~PowerOptions()
{
    delete ui;
}

void PowerOptions::on_cancelButton_clicked()
{
    emit dismiss();
}

void PowerOptions::on_hibernateButton_clicked()
{
    QList<QVariant> arguments;
    arguments.append(true);

    QDBusMessage message = QDBusMessage::createMethodCall("org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.login1.Manager", "Hibernate");
    message.setArguments(arguments);
    QDBusConnection::systemBus().send(message);

    emit dismiss();
    emit showCover();
}

void PowerOptions::on_turnOffScreenButton_clicked()
{
    DPMSForceLevel(QX11Info::display(), DPMSModeOff);
    emit dismiss();
    emit showCover();
}

void PowerOptions::on_suspendButton_clicked()
{
    QList<QVariant> arguments;
    arguments.append(true);

    QDBusMessage message = QDBusMessage::createMethodCall("org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.login1.Manager", "Suspend");
    message.setArguments(arguments);
    QDBusConnection::systemBus().send(message);

    emit dismiss();
    emit showCover();
}

void PowerOptions::on_rebootButton_clicked()
{
    QList<QVariant> arguments;
    arguments.append(true);

    QDBusMessage message = QDBusMessage::createMethodCall("org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.login1.Manager", "Reboot");
    message.setArguments(arguments);
    QDBusConnection::systemBus().send(message);

    emit dismiss();
}

void PowerOptions::on_powerOffButton_clicked()
{
    QList<QVariant> arguments;
    arguments.append(true);

    QDBusMessage message = QDBusMessage::createMethodCall("org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.login1.Manager", "PowerOff");
    message.setArguments(arguments);
    QDBusConnection::systemBus().send(message);

    emit dismiss();
}
