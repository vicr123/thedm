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
#ifndef DISPLAYMANAGER_H
#define DISPLAYMANAGER_H

#include <QObject>
#include <QDBusObjectPath>

struct SeatManagerPrivate;
class SeatManager : public QObject
{
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "org.freedesktop.DisplayManager.Seat")

        Q_PROPERTY(bool CanSwitch READ CanSwitch)
        Q_PROPERTY(bool HasGuestAccount READ HasGuestAccount)
        Q_PROPERTY(QList<QDBusObjectPath> Sessions READ Sessions);
    public:
        explicit SeatManager(QString seat, bool testMode, QObject *parent = nullptr);
        ~SeatManager();

        bool CanSwitch();
        bool HasGuestAccount();
        QList<QDBusObjectPath> Sessions();
    signals:

    public slots:
        void spawnGreeter();
        Q_SCRIPTABLE void SwitchToGreeter();
        Q_SCRIPTABLE void SwitchToUser(QString username, QString session_name);
        Q_SCRIPTABLE void Lock();
        bool SwitchToUser(QString username);


    private:
        SeatManagerPrivate* d;
};

#endif // DISPLAYMANAGER_H
