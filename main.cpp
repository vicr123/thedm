#include <QApplication>
#include <QWindow>
#include <QProcess>
#include <QX11Info>
#include <QDBusMetaType>
#include "mainwindow.h"
#include <X11/extensions/Xrandr.h>

QList<MainWindow*> windows;

void closeWindows() {
    for (MainWindow* window : windows) {
        window->close();
    }
    windows.clear();
}

void openWindows() {
    closeWindows();
    for (int i = 0; i < QApplication::desktop()->screenCount(); i++) {
        MainWindow* w = new MainWindow();
        w->show();
        w->setGeometry(QApplication::desktop()->screenGeometry(i));
        w->showFullScreen();
        windows.append(w);
    }
}


int main(int argc, char *argv[])
{
    qputenv("XDG_SESSION_CLASS", "greeter");


    if (qgetenv("DISPLAY") == "") {
        //Start the X server
        int display;
        for (display = 0; QFile("/tmp/.X" + QString::number(display) + "-lock").exists(); display++) {
            //Do nothing and go over the loop again.
            //In effect, we open a X server on the next available display number.
        }

        QProcess vtGet;
        vtGet.start("fgconsole");
        vtGet.waitForFinished();
        QString currentVt = "vt" + QString(vtGet.readAll());

        QProcess* XServerProcess = new QProcess();
        XServerProcess->start("/usr/bin/X :" + QString::number(display) + " " + currentVt);
        XServerProcess->waitForStarted();
        sleep(1);
        qputenv("DISPLAY", QString(":" + QString::number(display)).toUtf8());

    }
    QApplication a(argc, argv);
    a.setQuitOnLastWindowClosed(false);
    qRegisterMetaType<QStringDBusObjectPathMap>();
    qDBusRegisterMetaType<QStringDBusObjectPathMap>();

    a.setOrganizationName("theSuite");
    a.setOrganizationDomain("");
    a.setApplicationName("theDM");
    QIcon::setThemeName("breeze");

    //Start DBus so that we can use KDED
    QProcess* dbusLaunch = new QProcess;
    dbusLaunch->start("dbus-launch");
    dbusLaunch->waitForFinished();
    for (QString env : QString(dbusLaunch->readAll()).split("\n")) {
        QStringList parts = env.split("=");
        if (parts.length() >= 2) {
            QString key = parts.first();
            parts.removeFirst();
            qputenv(key.toStdString().data(), parts.join("=").toUtf8());
        }
    }

    {
        QProcess* kded = new QProcess;
        kded->start("kded5");
        kded->waitForStarted();

        QDBusMessage kscreen = QDBusMessage::createMethodCall("org.kde.kded5", "/kded", "org.kde.kded5", "loadModule");
        QVariantList args;
        args.append("kscreen");
        kscreen.setArguments(args);
        QDBusConnection::sessionBus().call(kscreen);
    }

    /*{
        int numberOfScreens = QApplication::screens().count();
        for (int i = 0; i < numberOfScreens; i++) {
            int numberOfSizes;
            int currentWidth = 0, currentHeight = 0, currentMWidth = 0, currentMHeight = 0;
            SizeID newSizeId = 0;
            XRRScreenSize *screenSize = XRRSizes(QX11Info::display(), numberOfScreens, &numberOfSizes);
            for (int sizes = 0; sizes < numberOfSizes; sizes++) {
                if ((screenSize[sizes].height * screenSize[sizes].width) > (currentWidth * currentHeight)) {
                    //This is a bigger screen.
                    currentWidth = screenSize[sizes].width;
                    currentHeight = screenSize[sizes].height;
                    currentMWidth = screenSize[sizes].mwidth;
                    currentMHeight = screenSize[sizes].mheight;
                    newSizeId = sizes;
                }

            }



            XRRScreenConfiguration *currentConfig = XRRGetScreenInfo(QX11Info::display(), DefaultRootWindow(QX11Info::display()));
            XRRSetScreenConfig(QX11Info::display(), currentConfig, DefaultRootWindow(QX11Info::display()), newSizeId, RR_Rotate_0, CurrentTime);

            XRRFreeScreenConfigInfo(currentConfig);
        }*/

        /*int numberOfMonitors;
        XRRMonitorInfo* monitorInfoArray = XRRGetMonitors(QX11Info::display(), DefaultRootWindow(QX11Info::display()), 1, &numberOfMonitors);
        for (int i = 0; i < numberOfMonitors; i++) {
            /*
            XRRMonitorInfo monitorInfo = monitorInfoArray[i];
            monitorInfo.automatic = 1;
            XRRSetMonitor(QX11Info::display(), DefaultRootWindow(QX11Info::display()), &monitorInfo);
            XRRFreeMonitors(monitorInfo);


            XRRMonitorInfo *m = XRRAllocateMonitor(QX11Info::display(), monitorInfoArray[i].noutput);

            m->name = monitorInfoArray[i].name;
            m->primary = monitorInfoArray[i].primary;
            m->x = monitorInfoArray[i].x;
            m->y = monitorInfoArray[i].y;
            m->width = monitorInfoArray[i].width;
            m->height = monitorInfoArray[i].height;
            m->mwidth = monitorInfoArray[i].mwidth;
            m->mheight = monitorInfoArray[i].mheight;
            for (int out = 0; out < monitorInfoArray[i].noutput; out++) {
                m->outputs[out] = monitorInfoArray[i].outputs[out];
            }
            m->automatic = 1;
            XRRSetMonitor(QX11Info::display(), DefaultRootWindow(QX11Info::display()), m);
            XRRFreeMonitors(m);
        }
        XRRFreeMonitors(monitorInfoArray);
        XSync(QX11Info::display(), false);
    }*/

    XGrabKeyboard(QX11Info::display(), RootWindow(QX11Info::display(), 0), True, GrabModeAsync, GrabModeAsync, CurrentTime);
    XGrabPointer(QX11Info::display(), RootWindow(QX11Info::display(), 0), True, None, GrabModeAsync, GrabModeAsync, RootWindow(QX11Info::display(), 0), None, CurrentTime);


    //Listen out for screen changes
    QObject::connect(QApplication::desktop(), &QDesktopWidget::resized, &openWindows);
    QObject::connect(QApplication::desktop(), &QDesktopWidget::screenCountChanged, &openWindows);

    openWindows();

    int ret = a.exec();

    for (MainWindow* w : windows) {
        w->close();
    }

    XUngrabKeyboard(QX11Info::display(), CurrentTime);
    XUngrabPointer(QX11Info::display(), CurrentTime);

    return ret;
}

