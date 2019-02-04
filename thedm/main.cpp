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

#include "manageddisplay.h"
#include <QProcess>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    //Find the current VT
    QProcess vtProc;
    vtProc.start("fgconsole");
    vtProc.waitForFinished();

    bool vtOk;
    int vt = vtProc.readAll().toInt(&vtOk);

    if (!vtOk) {
        vt = 1;
    }
    ManagedDisplay* d = new ManagedDisplay("seat0", "vt" + QString::number(vt));

    return a.exec();
}
