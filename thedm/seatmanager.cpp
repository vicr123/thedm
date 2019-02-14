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
#include "seatmanager.h"
#include "manageddisplay.h"
#include "dbus_seatmanager_adaptor.h"
#include <QDebug>
#include <QRandomGenerator>
#include <QLocalServer>
#include <QLocalSocket>

struct SeatManagerPrivate {
    QString seat;
    QMap<QString, ManagedDisplay*> dpys;

    QLocalServer srv;

    bool testMode;
};

SeatManager::SeatManager(QString seat, bool testMode, QObject *parent) : QObject(parent)
{
    d = new SeatManagerPrivate();

    SeatAdaptor* dbusAdaptor = new SeatAdaptor(this);
    QDBusConnection::systemBus().registerObject("/org/freedesktop/DisplayManager/" + seat, "org.freedesktop.DisplayManager.Seat", this);

    d->seat = seat;
    d->testMode = testMode;

    //Create a random server name
    QString choices = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    QString seed;
    for (int i = 0; i < 11; i++) {
        seed.append(choices.at(QRandomGenerator::global()->bounded(choices.count())));
    }
    d->srv.listen("thedm" + seed);

    connect(&d->srv, &QLocalServer::newConnection, [=] {
        QLocalSocket* connection = d->srv.nextPendingConnection();
        QMetaObject::Connection* c = new QMetaObject::Connection();
        *c = connect(connection, &QLocalSocket::readyRead, [=] {
            while (connection->canReadLine()) {
                QString line = connection->readLine().trimmed();
                QStringList parts = line.split(" ");
                if (parts.count() == 2 && parts.first() == "SEED") {
                    QString seed = parts.at(1);
                    if (!d->dpys.contains(seed)) continue;
                    if (!d->dpys.value(seed)->needsSocket()) continue;

                    //Tell the display about the socket
                    d->dpys.value(seed)->socketAvailable(connection);
                    disconnect(*c);
                    delete c;

                    return;
                }
            }
        });
    });

    spawnGreeter();
}

void SeatManager::spawnGreeter(int vt) {
    qDebug() << "Spawning a greeter";
    if (vt == -1) {
        vt = ManagedDisplay::nextAvailableVt();
    }

    //Create a random seed
    QString choices = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

    QString seed;
    do {
        for (int i = 0; i < 127; i++) {
            seed.append(choices.at(QRandomGenerator::global()->bounded(choices.count())));
        }
    } while (d->dpys.keys().contains(seed));

    ManagedDisplay* dpy = new ManagedDisplay(d->seat, vt, d->testMode, seed, d->srv.serverName(), this);
    connect(dpy, &ManagedDisplay::displayGone, [=](ManagedDisplay::DisplayGoneReason reason) {
        d->dpys.remove(seed);

        if (reason == ManagedDisplay::SessionExit) {
            //Switch to a greeter if one exists
            //otherwise spawn a new greeter
            SwitchToGreeter();
        }
    });
    d->dpys.insert(seed, dpy);
}

SeatManager::~SeatManager() {
    delete d;
}

void SeatManager::SwitchToGreeter() {
    for (ManagedDisplay* dpy : d->dpys.values()) {
        if (!dpy->loggedIn()) {
            //Switch to this greeter
            qDebug() << "Switching to existing greeter";
            dpy->activate();
            return;
        }
    }

    qDebug() << "Spawning an existing greeter";
    spawnGreeter();
}

void SeatManager::SwitchToUser(QString username, QString session_name) {

}

void SeatManager::Lock() {

}

bool SeatManager::CanSwitch() {
    return true;
}

bool SeatManager::HasGuestAccount() {
    return false;
}

QList<QDBusObjectPath> SeatManager::Sessions() {
    return QList<QDBusObjectPath>();
}

bool SeatManager::SwitchToUser(QString username) {
    for (ManagedDisplay* dpy : d->dpys.values()) {
        if (dpy->username() == username && dpy->loggedIn()) {
            //Activate this display
            dpy->activate();
            return true;
        }
    }
    return false;
}
