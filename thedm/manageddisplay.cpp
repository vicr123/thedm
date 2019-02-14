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
#include "seatmanager.h"
#include <QProcess>
#include <QDebug>
#include <QSettings>
#include <QThread>
#include <QFile>
#include <QDBusInterface>
#include <the-libs_global.h>

#include "sddm/virtualterminal.h"

#include <unistd.h>
#include <signal.h>

struct ManagedDisplayPrivate {
    SeatManager* mgr;

    QProcess* x11Process = nullptr;
    QProcess* greeterProcess = nullptr;

    QString seat;
    QString seed;
    QString srvName;
    QString xdisplay;
    QString sessionId;
    QString username;

    ManagedDisplay::DisplayGoneReason reason = ManagedDisplay::SessionExit;
    int spawnRetryCount = 0;
    int vt;
    bool testMode;
    bool loggedIn = false;

    QLocalSocket* sock = nullptr;
};

ManagedDisplay::ManagedDisplay(QString seat, int vt, bool testMode, QString seed, QString srvName, SeatManager *parent) : QObject(parent)
{
    d = new ManagedDisplayPrivate();
    d->mgr = parent;

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
        if (testMode) {
            d->x11Process->start("/usr/bin/Xephyr", {
                ":" + QString::number(display),
                "-dpi", dpi,
                "-displayfd", QString::number(pipeFds[1]),
                "-screen", "1024x768"
            });
        } else {
            d->x11Process->start("/usr/bin/Xorg", {
                ":" + QString::number(display),
                "vt" + QString::number(vt),
                "-dpi", dpi,
                "-displayfd", QString::number(pipeFds[1]),
                "-seat", seat
            });
        }
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

            d->xdisplay = QString(":" + QString::number(display));
            serverStarted = true;

            close(pipeFds[0]);
        }
    }

    d->vt = vt;
    d->testMode = testMode;
    d->seed = seed;
    d->srvName = srvName;
    d->seat = seat;

    doSpawnGreeter();
}

ManagedDisplay::~ManagedDisplay() {
    if (d->x11Process->state() == QProcess::Running) {
        d->x11Process->terminate();
    }

    emit displayGone(d->reason);

    d->sock->close();
    d->sock->deleteLater();
    delete d;
}

void ManagedDisplay::doSpawnGreeter() {
    //Reset everything
    d->sessionId = "";
    d->username = "";
    d->sock = nullptr;

    //Find the greeter path
    QStringList possibleGreeters;
    if (QFile("../thedm-greeter/thedm-greeter").exists()) {
        possibleGreeters.append("../thedm-greeter/thedm-greeter");
    }
    possibleGreeters.append(theLibsGlobal::searchInPath("thedm-greeter"));

    if (possibleGreeters.count() == 0) {
        //Error
        qDebug() << "Can't find a suitable greeter";
        qDebug() << "Your installation of theDM is broken.";
        qDebug() << "You should try reinstalling theDM";

        return;
    }

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("QT_QPA_PLATFORMTHEME", "ts");
    env.insert("QT_IM_MODULE", "ts-kbd");
    env.insert("DISPLAY", d->xdisplay);

    //Spawn a DBus session bus
    QProcess dbusLaunchProcess;
    dbusLaunchProcess.start("dbus-launch --sh-syntax");
    dbusLaunchProcess.waitForFinished();
    while (!dbusLaunchProcess.atEnd()) {
        QString envvar = dbusLaunchProcess.readLine();
        int sepLocation = envvar.indexOf("=");
        env.insert(envvar.left(sepLocation), envvar.mid(sepLocation + 1));
    }

    QStringList args = {
        "dbus-launch",
        possibleGreeters.first(),
        "--vt", QString::number(d->vt),
        "--seed", d->seed,
        "--srv", d->srvName,
        "--seat", d->seat
    };

    if (d->testMode) {
        args.append("--test-mode");
    }

    //Spawn the greeter
    //Also spawn a DBus session bus
    d->greeterProcess = new QProcess();
    d->greeterProcess->setProcessEnvironment(env);
    d->greeterProcess->setProcessChannelMode(QProcess::ForwardedChannels);
    d->greeterProcess->start(args.join(" "));
    connect(d->greeterProcess, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished), [=](int exitCode, QProcess::ExitStatus status) {
        qDebug() << "Greeter exited with exit code" << exitCode;
        switch (exitCode) {
            case 0:
                d->reason = SessionExit;
                break;
            case 1:
                d->reason = SwitchSession;
                break;
            default: {
                qWarning() << "Cannot spawn greeter process.";
                d->spawnRetryCount++;
                if (d->spawnRetryCount < 5) {
                    doSpawnGreeter();
                } else {
                    qWarning() << "Giving up on spawning the greeter. Maybe something is wrong with your configuration.";
                    d->reason = GreeterNotSpawned;
                    this->deleteLater();
                }
                return;
            }
        }

        //Kill the DBus session bus
        if (env.contains("DBUS_SESSION_BUS_PID")) {
            kill(env.value("DBUS_SESSION_BUS_PID").toInt(), SIGTERM);
        }

        this->deleteLater();
    });
}

int ManagedDisplay::nextAvailableVt() {
    int vt = SDDM::VirtualTerminal::setUpNewVt();
    if (vt == -1) {
        QDBusInterface logind("org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.login1.Manager", QDBusConnection::systemBus());
        QDBusMessage sessions = logind.call("ListSessions");

        QDBusArgument sessionArg = sessions.arguments().first().value<QDBusArgument>();
        QList<SessionList> listOfSessions;
        sessionArg >> listOfSessions;

        QList<int> usedVts;
        for (SessionList s : listOfSessions) {
            QDBusInterface session("org.freedesktop.login1", s.sessionPath.path(), "org.freedesktop.login1.Session", QDBusConnection::systemBus());

            if (!usedVts.contains(session.property("VTNr").toInt())) {
                usedVts.append(session.property("VTNr").toInt());
            }
        }

        int currentVt = 1;
        while (usedVts.contains(currentVt)) currentVt++;

        vt = currentVt;
    }

    return vt;
}

bool ManagedDisplay::needsSocket() {
    if (d->sock == nullptr) {
        return true;
    } else {
        return false;
    }
}

void ManagedDisplay::socketAvailable(QLocalSocket* socket) {
    qDebug() << "Socket acquired for " + d->srvName;
    d->sock = socket;
    connect(socket, &QLocalSocket::readyRead, [=] {
        while (socket->canReadLine()) {
            QString line = socket->readLine().trimmed();
            if (line.startsWith("LOGIN ")) {
                //We're logged in
                d->loggedIn = true;
                d->username = line.mid(6);
            } else if (line.startsWith("SESSIONID ")) {
                d->sessionId = line.mid(10);
            } else if (line.startsWith("SWITCH ")) {
                QString username = line.mid(7);
                if (d->mgr->SwitchToUser(username)) {
                    //We're switching to someone else
                    //Reset the greeter
                    socket->write("RESET\n");
                    socket->flush();
                }
            }
        }
    });
}

bool ManagedDisplay::loggedIn() {
    return d->loggedIn;
}

int ManagedDisplay::vt() {
    return d->vt;
}

void ManagedDisplay::activate() {
    if (d->loggedIn) {
        //Ask logind to activate session
        QDBusMessage message = QDBusMessage::createMethodCall("org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.login1.Manager", "ActivateSession");
        message.setArguments({d->sessionId});

        QDBusConnection::systemBus().call(message, QDBus::NoBlock);
    } else {
        SDDM::VirtualTerminal::jumpToVt(d->vt, true);
    }
}

QString ManagedDisplay::username() {
    return d->username;
}
