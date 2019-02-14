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
#include <QCoreApplication>

#include "seatmanager.h"
#include <QDebug>
#include <QProcess>
#include <QDBusConnection>
#include <QCommandLineParser>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    a.setOrganizationName("theSuite");
    a.setOrganizationDomain("");
    a.setApplicationName("theDM");

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({"test-mode", "Run theDM in testing mode"});
    parser.process(a);

    QDBusConnection::systemBus().registerService("org.freedesktop.DisplayManager");
    if (parser.isSet("test-mode")) {
        qDebug() << "Running in test mode";
    } else {
        //Check if root
    }

    SeatManager* s = new SeatManager("seat0", parser.isSet("test-mode"));

    return a.exec();
}
