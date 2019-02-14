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

bool testMode = false;
QCommandLineParser* parser;

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    a.setOrganizationName("theSuite");
    a.setOrganizationDomain("");
    a.setApplicationName("theDM");

    parser = new QCommandLineParser();
    parser->addHelpOption();
    parser->addOption({"test-mode", "Run theDM in testing mode"});
    parser->addOption({"seed", "Seed of the master theDM process", "seed"});
    parser->addOption({"vt", "VT that we're running on", "vt"});
    parser->addOption({"srv", "Name of local socket server to connect to", "srv"});
    parser->addOption({"seat", "Name of seat this greeter is for", "seat"});

    parser->process(a);

    if (parser->isSet("test-mode")) {
        testMode = true;
    }

    QString vtString = parser->value("vt");

    bool vtIntOk;
    vtString.toInt(&vtIntOk);
    if (!vtIntOk || vtString.isEmpty()) {
        //TODO: Yell error
        qWarning() << "VT is required";
        return 1;
    }

    /*PamBackend backend("root", "sddm-greeter");
    backend.putenv("DISPLAY", qgetenv("DISPLAY"));
    backend.putenv("XDG_SESSION_CLASS", "greeter");
    backend.putenv("XDG_SESSION_TYPE", "x11");
    backend.setItem(PAM_XDISPLAY, qgetenv("DISPLAY"));
    backend.setItem(PAM_TTY, qgetenv("DISPLAY"));
    backend.authenticate();
    backend.acctMgmt();
    backend.setCred();
    backend.startSession("");

    qDebug() << backend.getenv("XDG_RUNTIME_DIR");*/


    MainWindow w(vtString);
    w.showFullScreen();

    return a.exec();
}
