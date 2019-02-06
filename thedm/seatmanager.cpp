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

struct SeatManagerPrivate {
    QString seat;
    QList<ManagedDisplay*> dpys;
};

SeatManager::SeatManager(QString seat, QObject *parent) : QObject(parent)
{
    d = new SeatManagerPrivate();

    d->seat = seat;
    spawnGreeter();
}

void SeatManager::spawnGreeter() {
    int vt = ManagedDisplay::nextAvailableVt();
    ManagedDisplay* dpy = new ManagedDisplay(d->seat, vt);
    connect(dpy, &ManagedDisplay::displayGone, [=](ManagedDisplay::DisplayGoneReason reason) {
        d->dpys.removeOne(dpy);

        if (reason == ManagedDisplay::SessionExit) {
            //Spawn a new greeter
            spawnGreeter();
        }
    });
    d->dpys.append(dpy);
}

SeatManager::~SeatManager() {
    delete d;
}
