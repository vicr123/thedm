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
#include <QFile>
#include <the-libs_global.h>

#include <unistd.h>

struct ManagedDisplayPrivate {
    QProcess* x11Process = nullptr;
    QProcess* greeterProcess = nullptr;
};

ManagedDisplay::ManagedDisplay(QString seat, int vt, QObject *parent) : QObject(parent)
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

        //Create a pipe for talking to the X server and getting the display number
        int pipeFds[2];
        pipe(pipeFds);

        d->x11Process = new QProcess();
        //d->x11Process->start("/usr/bin/X :" + QString::number(display) + " vt" + QString::number(vt) + " -dpi " + dpi);
        d->x11Process->start("/usr/bin/X", {
            ":" + QString::number(display),
            "vt" + QString::number(vt),
            "-dpi", dpi,
            "-displayfd", QString::number(pipeFds[1]),
            "-seat", seat
        });
        d->x11Process->waitForFinished(1000);

        if (d->x11Process->state() != QProcess::Running) {
            //Close the pipe
            close(pipeFds[0]);
            display++;
        } else {
            //Close the unneeded pipe
            close(pipeFds[1]);

            QFile displayPipe;
            displayPipe.open(pipeFds[0], QFile::ReadOnly);

            QByteArray displayNumber = displayPipe.readLine();
            if (displayNumber.size() < 2) {
                //Couldn't read display number
                close(pipeFds[0]);
                display++;
                continue;
            }

            display = displayNumber.trimmed().toInt();

            qputenv("DISPLAY", QString(":" + QString::number(display)).toUtf8());
            serverStarted = true;

            close(pipeFds[0]);
        }
    }

    //qputenv("QT_QPA_PLATFORMTHEME", "ts");
    //qputenv("QT_IM_MODULE", "ts-kbd");

    //Find the greeter path
    QStringList possibleGreeters = theLibsGlobal::searchInPath("thedm-greeter");
    if (possibleGreeters.count() == 0) {
        possibleGreeters.append("../thedm-greeter/thedm-greeter");
    }

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("QT_QPA_PLATFORMTHEME", "ts");
    env.insert("QT_IM_MODULE", "ts-kbd");

    //Spawn the greeter
    d->greeterProcess = new QProcess();
    d->greeterProcess->setProcessEnvironment(env);
    d->greeterProcess->setArguments({
        QString::number(vt)
    });
    d->greeterProcess->setProcessChannelMode(QProcess::ForwardedChannels);
    d->greeterProcess->start(possibleGreeters.first());
    connect(d->greeterProcess, QOverload<int>::of(&QProcess::finished), [=] {
        this->deleteLater();
    });
    connect(d->greeterProcess, QOverload<QProcess::ProcessError>::of(&QProcess::error), [=] {
        qWarning() << "Cannot spawn greeter process.";
    });
}

ManagedDisplay::~ManagedDisplay() {
    if (d->x11Process->state() == QProcess::Running) {
        d->x11Process->terminate();
    }
    delete d;
}
