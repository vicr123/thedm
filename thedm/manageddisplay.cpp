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
#include "manageddisplay.h"
#include <QProcess>
#include <QDebug>
#include <QSettings>
#include <QThread>

struct ManagedDisplayPrivate {
    QProcess* x11Process = nullptr;
    QProcess* greeterProcess = nullptr;
};

ManagedDisplay::ManagedDisplay(QString seat, QString vt, QObject *parent) : QObject(parent)
{
    d = new ManagedDisplayPrivate();

    QSettings settings("/etc/thedm.conf", QSettings::IniFormat);

    //Spawn X on the seat and VT
    int display = 0;
    bool serverStarted = false;
    QString dpi = QString::number(settings.value("screen/dpi", 96).toInt());
    while (!serverStarted) {
        if (d->x11Process != nullptr) {
            d->x11Process->deleteLater();
        }

        d->x11Process = new QProcess();
        d->x11Process->start("/usr/bin/X :" + QString::number(display) + " " + vt + " -dpi " + dpi);
        d->x11Process->waitForFinished(1000);

        if (d->x11Process->state() != QProcess::Running) {
            display++;
        } else {
            qputenv("DISPLAY", QString(":" + QString::number(display)).toUtf8());
            serverStarted = true;
        }
    }

    qputenv("QT_QPA_PLATFORMTHEME", "ts");
    qputenv("QT_IM_MODULE", "ts-kbd");

    //Spawn the greeter
    d->greeterProcess = new QProcess();
    d->greeterProcess->start("../thedm-greeter/thedm-greeter");
    d->greeterProcess->setProcessChannelMode(QProcess::ForwardedChannels);
    connect(d->greeterProcess, QOverload<int>::of(&QProcess::finished), [=] {
        this->deleteLater();
    });
}

ManagedDisplay::~ManagedDisplay() {
    if (d->x11Process->state() == QProcess::Running) {
        d->x11Process->terminate();
    }
    delete d;
}
