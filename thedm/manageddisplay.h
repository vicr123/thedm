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
#ifndef MANAGEDDISPLAY_H
#define MANAGEDDISPLAY_H

#include <QObject>
#include <QDBusObjectPath>
#include <QDBusArgument>
#include <QLocalSocket>

struct SessionList {
    QString sessionId;
    uint userId;
    QString userName;
    QString seatId;
    QDBusObjectPath sessionPath;
};
Q_DECLARE_METATYPE(SessionList)

inline QDBusArgument &operator<<(QDBusArgument &arg, const SessionList &map) {
    arg.beginStructure();
    arg << map.sessionId << map.userId << map.userName << map.seatId << map.sessionPath;
    arg.endStructure();
    return arg;
}

inline const QDBusArgument &operator>>(const QDBusArgument &arg, SessionList &map) {
    arg.beginStructure();
    arg >> map.sessionId >> map.userId >> map.userName >> map.seatId >> map.sessionPath;
    arg.endStructure();
    return arg;
}

struct ManagedDisplayPrivate;
class SeatManager;
class ManagedDisplay : public QObject
{
        Q_OBJECT
    public:
        explicit ManagedDisplay(QString seat, int vt, bool testMode, QString seed, QString srvName, SeatManager *parent);
        ~ManagedDisplay();

        enum DisplayGoneReason {
            SessionExit,
            SwitchSession,
            GreeterNotSpawned,
            Unknown
        };

        static int nextAvailableVt();
        bool needsSocket();
        bool loggedIn();
        int vt();
        QString username();

    signals:
        void displayGone(DisplayGoneReason reason);

    public slots:
        void socketAvailable(QLocalSocket* socket);
        void activate();

    private:
        ManagedDisplayPrivate* d;

        void doSpawnGreeter();
};

#endif // MANAGEDDISPLAY_H
