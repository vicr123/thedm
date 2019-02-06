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
#include "mainwindow.h"
#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    /*QCommandLineParser parser;
    parser.addPositionalArgument("vt", "The VT to display on");

    parser.process(a);
    QString vtString = parser.positionalArguments().first();

    bool vtIntOk;
    vtString.toInt(&vtIntOk);
    if (!vtIntOk) {
        //TODO: Yell error
        return 1;
    }*/

    PamBackend backend("root", "sddm-greeter");
    backend.putenv("DISPLAY", qgetenv("DISPLAY"));
    backend.putenv("XDG_SESSION_CLASS", "greeter");
    backend.putenv("XDG_SESSION_TYPE", "x11");
    backend.putenv("XDG_SEAT", "seat0");
    backend.setItem(PAM_XDISPLAY, qgetenv("DISPLAY"));
    backend.setItem(PAM_TTY, qgetenv("DISPLAY"));
    backend.authenticate();
    backend.acctMgmt();
    backend.setCred();
    backend.startSession("");

    qDebug() << backend.getenv("XDG_RUNTIME_DIR");

    MainWindow w("1");
    w.showFullScreen();

    return a.exec();
}
