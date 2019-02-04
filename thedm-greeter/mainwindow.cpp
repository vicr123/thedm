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
#include "ui_mainwindow.h"

#include <QFileInfo>
#include <QDBusMessage>
#include <QSvgRenderer>
#include <QProcess>
#include <QX11Info>
#include <tpropertyanimation.h>
#include <QDateTime>
#include <QDBusConnectionInterface>
#include <QScreen>
#include <QToolButton>
#include <QMenu>
#include <QDir>
#include <QSysInfo>
#include <tvirtualkeyboard.h>
#include <ttoast.h>
#include "pam.h"

#include <grp.h>
#include <pwd.h>

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#undef KeyPress
#undef KeyRelease

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    settings = new QSettings("/etc/thedm.conf", QSettings::IniFormat);

    background = settings->value("background", "/usr/share/tsscreenlock/triangles.svg").toString();
    ui->pagesStack->setCurrentAnimation(tStackedWidget::SlideHorizontal);

    QFileInfo backgroundInfo(background);
    if (backgroundInfo.suffix() == "svg") {
        image = QPixmap(this->size());
        QSvgRenderer renderer(background);
        QPainter imagePainter(&image);
        renderer.render(&imagePainter, image.rect());
        imagePainter.fillRect(0, 0, image.width(), image.height(), QColor::fromRgb(0, 0, 0, 127));
    } else {
        image = QPixmap(background);

        QPainter imagePainter(&image);
        imagePainter.fillRect(0, 0, image.width(), image.height(), QColor::fromRgb(0, 0, 0, 127));
    }

    ui->coverArrowUp->setPixmap(QIcon::fromTheme("go-up").pixmap(16, 16));

    ui->hostnameLabel->setText(QSysInfo::machineHostName());
    ui->hostnameLabel_2->setText(QSysInfo::machineHostName());

    //Load users
    for (int i = settings->value("users/uidMin", 1000).toInt(); i < settings->value("users/uidMax", 10000).toInt(); i++) {
        //Get user info
        struct passwd *pw = getpwuid(i);
        if (pw != nullptr) {
            QString gecosData = pw->pw_gecos;
            QStringList gecosDataList = gecosData.split(",");
            QString username = pw->pw_name;
            QString home = pw->pw_dir;

            QToolButton* userButton = new QToolButton();
            userButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
            userButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            userButton->setIcon(QIcon::fromTheme("user"));
            if (gecosDataList.count() == 0 || gecosData == "") {
                userButton->setText(username);
            } else {
                userButton->setText(gecosDataList.first());
            }
            ui->userList->addWidget(userButton);
            connect(userButton, &QPushButton::clicked, [=] {
                ui->usernameLabel->setText(tr("Hi %1!").arg(userButton->text()));
                ui->pagesStack->setCurrentIndex(1);
                currentLoginUsername = username;

                if (QFile(home + "/.theshell/mousepassword").exists()) {
                    ui->mousePasswordWarning->setVisible(true);
                    ui->LoginOptions->setVisible(true);
                } else {
                    ui->mousePasswordWarning->setVisible(false);
                    ui->LoginOptions->setVisible(false);
                }

                QTimer::singleShot(100, [=] {
                    ui->PasswordUnderline->startAnimation();
                    tVirtualKeyboard::instance()->showKeyboard();
                });
            });
        }
    }
    ui->userScroll->setFixedHeight(qMin((float) ui->userList->sizeHint().height(), this->height() / 4 * theLibsGlobal::getDPIScaling()));

    //Load sessions
    QMenu* sessionsMenu = new QMenu();
    QDir xsessions("/usr/share/xsessions/");
    for (QString file : xsessions.entryList(QDir::Files | QDir::NoDotAndDotDot)) {
        QSettings xsessionsFile(xsessions.path() + "/" + file, QSettings::IniFormat);
        xsessionsFile.beginGroup("Desktop Entry");
        QAction* action = new QAction;
        action->setText(xsessionsFile.value("Name", xsessions.path() + "/" + file).toString());
        action->setData(xsessionsFile.value("Exec"));
        sessionsMenu->addAction(action);
    }
    if (sessionsMenu->actions().count() != 0) {
        on_sessionSelect_triggered(sessionsMenu->actions().first());
    }
    ui->sessionSelect->setMenu(sessionsMenu);

    QTimer* timer = new QTimer();
    timer->setInterval(1000);
    connect(timer, SIGNAL(timeout()), this, SLOT(tick()));
    connect(timer, SIGNAL(timeout()), this, SLOT(BatteryChanged()));
    timer->start();

    tick();
    BatteriesChanged();

    QDBusConnection::systemBus().connect("org.freedesktop.UPower", "/org/freedesktop/UPower", "org.freedesktop.UPower", "DeviceAdded", this, SLOT(BatteriesChanged()));
    QDBusConnection::systemBus().connect("org.freedesktop.UPower", "/org/freedesktop/UPower", "org.freedesktop.UPower", "DeviceRemoved", this, SLOT(BatteriesChanged()));

    ui->capsLockOn->setPixmap(QIcon::fromTheme("input-caps-on").pixmap(16, 16));
    ui->password->installEventFilter(this);

    unsigned int capsState;
    XkbGetIndicatorState(QX11Info::display(), XkbUseCoreKbd, &capsState);
    if ((capsState & 0x01) != 1) {
        ui->capsLockOn->setVisible(false);
    }

    passwordFrameOpacity = new QGraphicsOpacityEffect();
    passwordFrameOpacity->setOpacity(0);
    ui->passwordFrame->setGraphicsEffect(passwordFrameOpacity);

    connect(tVirtualKeyboard::instance(), &tVirtualKeyboard::keyboardVisibleChanged, [=](bool visible) {
        if (visible) {
            this->setGeometry(QApplication::screens().first()->geometry().adjusted(0, 0, 0, -tVirtualKeyboard::instance()->height()));
        } else {
            this->setGeometry(QApplication::screens().first()->geometry());
        }
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::showFullScreen() {
    QMainWindow::show();

    this->setGeometry(QApplication::screens().first()->geometry());

    this->centralWidget()->layout()->removeWidget(ui->coverFrame);
    ui->coverFrame->setParent(this->centralWidget());
    ui->coverFrame->setGeometry(0, 0, this->width(), this->height());
    ui->coverFrame->installEventFilter(this);

    ui->coverFrame->layout()->removeWidget(ui->unlockPromptFrame);
    ui->unlockPromptFrame->setParent(ui->coverFrame);
    ui->unlockPromptFrame->setGeometry(0, this->height(), this->width(), ui->unlockPromptFrame->height());

    this->centralWidget()->layout()->removeWidget(ui->passwordFrame);
    ui->passwordFrame->setParent(this->centralWidget());
    ui->passwordFrame->setGeometry(0, 0, this->width(), this->height());

    ui->scrollAreaWidgetContents->installEventFilter(this);

    QTimer::singleShot(1000, [=] {
        tPropertyAnimation* anim = new tPropertyAnimation(ui->unlockPromptFrame, "geometry");
        anim->setStartValue(ui->unlockPromptFrame->geometry());
        anim->setEndValue(QRect(0, this->height() - ui->unlockPromptFrame->height(), this->width(), ui->unlockPromptFrame->height()));
        anim->setDuration(500);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        connect(anim, SIGNAL(finished()), anim, SLOT(deleteLater()));
        anim->start();
    });

    QMargins currentMargins = ui->coverFrame->contentsMargins();
    currentMargins.setBottom(ui->unlockPromptFrame->height());
    ui->coverFrame->layout()->setContentsMargins(currentMargins);

    ui->password->setFocus();
}

void MainWindow::tick() {
    ui->coverDate->setText(QLocale().toString(QDateTime::currentDateTime().date(), "ddd dd MMM yyyy"));
    ui->coverClock->setText(QLocale().toString(QDateTime::currentDateTime().time(), "hh:mm:ss"));
}

bool MainWindow::eventFilter(QObject *obj, QEvent *e) {
    if (obj == ui->coverFrame) {
        if (e->type() == QEvent::Paint) {
            QPainter painter(ui->coverFrame);

            //Determine rectangle in which to draw image
            QSize paintSize(image.width(), image.height());
            QSize screenSize = this->size();

            QSize fitSize = paintSize.scaled(screenSize, Qt::KeepAspectRatioByExpanding);

            QRect paintRect;
            paintRect.setSize(fitSize);
            paintRect.moveCenter(QPoint(screenSize.width() / 2, screenSize.height() / 2));

            painter.drawPixmap(paintRect, image);

            //Draw cover rectangle
            /*QRect coverRect;
            coverRect.setWidth(this->width());
            coverRect.moveTop(ui->unlockPromptFrame->geometry().bottom());
            coverRect.setHeight(this->height() - ui->unlockPromptFrame->geometry().bottom());
            coverRect.moveLeft(0);

            painter.setPen(Qt::transparent);
            painter.setBrush(Qt::transparent);//this->palette().color(QPalette::Window));
            painter.drawRect(coverRect);*/

            //Draw shade
            float percentShade = 1 - (float) ui->coverFrame->geometry().bottom() / (float) this->height();
            painter.fillRect(paintRect, QColor::fromRgb(0, 0, 0, 127 * percentShade));
            return true;
        } else if (e->type() == QEvent::MouseButtonPress) {
            QMouseEvent* event = (QMouseEvent*) e;
            this->moveY = event->y();
        } else if (e->type() == QEvent::MouseButtonRelease) {
            hideCover();
        } else if (e->type() == QEvent::MouseMove) {
            QMouseEvent* event = (QMouseEvent*) e;

            QRect geometry = ui->coverFrame->geometry();
            //QRect geometry = ui->unlockPromptFrame->geometry();
            geometry.translate(0, 0 - this->moveY + event->y());
            if (geometry.bottom() > this->height()) {
                geometry.moveTop(0);
            }
            geometry.setTop(0);
            ui->coverFrame->setGeometry(geometry);
            //ui->unlockPromptFrame->setGeometry(geometry);
            this->moveY = event->y();

            ui->coverFrame->repaint();
        } else if (e->type() == QEvent::Resize) {
            ui->unlockPromptFrame->setGeometry(0, ui->coverFrame->height() - ui->unlockPromptFrame->height(), this->width(), ui->unlockPromptFrame->height());
        }
    } else if (obj == ui->scrollAreaWidgetContents) {
        if (e->type() == QEvent::Paint) {
            //return true;
        }
    } else if (obj == ui->password) {
        if (e->type() == QEvent::KeyPress) {
            QKeyEvent* event = (QKeyEvent*) e;
            if (event->key() == Qt::Key_Return && event->modifiers() == Qt::MetaModifier) {
                this->keyPressEvent(event);
                return true;
            }
            if (!coverHidden) {
                hideCover();
                return true;
            }
        } else if (e->type() == QEvent::KeyRelease) {
            unsigned int capsState;
            XkbGetIndicatorState(QX11Info::display(), XkbUseCoreKbd, &capsState);
            if ((capsState & 0x01) == 1) {
                ui->capsLockOn->setVisible(true);
            } else {
                ui->capsLockOn->setVisible(false);
            }
        }
    }
    return false;
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QFileInfo backgroundInfo(background);
    ui->coverFrame->setGeometry(0, 0, this->width(), this->height());
    ui->passwordFrame->setGeometry(0, 0, this->width(), this->height());
    if (backgroundInfo.suffix() == "svg") {
        image = QPixmap(this->size());
        QSvgRenderer renderer(background);
        QPainter imagePainter(&image);
        renderer.render(&imagePainter, image.rect());
        imagePainter.fillRect(0, 0, image.width(), image.height(), QColor::fromRgb(0, 0, 0, 127));
    }
}

void MainWindow::BatteriesChanged() {
    allDevices.clear();
    QDBusInterface *i = new QDBusInterface("org.freedesktop.UPower", "/org/freedesktop/UPower", "org.freedesktop.UPower", QDBusConnection::systemBus(), this);
    QDBusReply<QList<QDBusObjectPath>> reply = i->call("EnumerateDevices"); //Get all devices

    if (reply.isValid()) { //Check if the reply is ok
        for (QDBusObjectPath device : reply.value()) {
            if (device.path().contains("battery") || device.path().contains("media_player") || device.path().contains("computer") || device.path().contains("phone")) { //This is a battery or media player or tablet computer
                QDBusConnection::systemBus().connect("org.freedesktop.UPower", device.path(),
                                                     "org.freedesktop.DBus.Properties", "PropertiesChanged", this,
                                                                      SLOT(DeviceChanged()));

                QDBusInterface *i = new QDBusInterface("org.freedesktop.UPower", device.path(), "org.freedesktop.UPower.Device", QDBusConnection::systemBus(), this);
                allDevices.append(i);
            }
        }
        BatteryChanged();
    } else {
        ui->batteryFrame->setVisible(true);
        ui->BatteryLabel->setText("Can't get battery information.");
    }
}


void MainWindow::BatteryChanged() {
    QStringList displayOutput;

    int batLevel;
    bool hasPCBat = false;
    bool isCharging;
    for (QDBusInterface *i : allDevices) {
        //Get the percentage of battery remaining.
        //We do the calculation ourselves because UPower can be inaccurate sometimes
        double percentage;

        //Check that the battery actually reports energy information
        double energyFull = i->property("EnergyFull").toDouble();
        double energy = i->property("Energy").toDouble();
        double energyEmpty = i->property("EnergyEmpty").toDouble();
        if (energyFull == 0 && energy == 0 && energyEmpty == 0) {
            //The battery does not report energy information, so get the percentage from UPower.
            percentage = i->property("Percentage").toDouble();
        } else {
            //Calculate the percentage ourself, and round it to an integer.
            //Add 0.5 because C++ always rounds down.
            percentage = (int) (((energy - energyEmpty) / (energyFull - energyEmpty) * 100) + 0.5);
        }
        if (i->path().contains("battery")) {
            //PC Battery
            if (i->property("IsPresent").toBool()) {
                hasPCBat = true;
                bool showRed = false;
                qulonglong timeToFull = i->property("TimeToFull").toULongLong();
                qulonglong timeToEmpty = i->property("TimeToEmpty").toULongLong();
                QDateTime timeRemain;

                //Depending on the state, do different things.
                QString state;
                switch (i->property("State").toUInt()) {
                case 1: //Charging
                    state = " (" + tr("Charging");

                    isCharging = true;

                    if (timeToFull != 0) {
                        timeRemain = QDateTime::fromTime_t(timeToFull).toUTC();
                        state.append(" · " + timeRemain.toString("h:mm"));
                    } else {
                        timeRemain = QDateTime(QDate(0, 0, 0));
                    }

                    state += ")";
                    break;
                case 2: //Discharging
                    //state = " (" + tr("Discharging");
                    state = " (";

                    if (timeToEmpty != 0) {
                        timeRemain = QDateTime::fromTime_t(timeToEmpty).toUTC();
                        state.append(/*" · " + */timeRemain.toString("h:mm"));
                    } else {
                        timeRemain = QDateTime(QDate(0, 0, 0));
                    }
                    state += ")";

                    isCharging = false;
                    break;
                case 3: //Empty
                    state = " (" + tr("Empty") + ")";
                    break;
                case 4: //Charged
                case 6: //Pending Discharge
                    state = " (" + tr("Full") + ")";
                    timeRemain = QDateTime(QDate(0, 0, 0));
                    isCharging = false;
                    break;
                case 5: //Pending Charge
                    state = " (" + tr("Not Charging") + ")";
                    break;
                }

                if (showRed) {
                    displayOutput.append("<span style=\"background-color: red; color: black;\">" + tr("%1% PC Battery%2").arg(QString::number(percentage), state) + "</span>");
                } else {
                    displayOutput.append(tr("%1% PC Battery%2").arg(QString::number(percentage), state));
                }

                batLevel = percentage;
            } else {
                displayOutput.append(tr("No Battery Inserted"));
            }
        } else if (i->path().contains("media_player") || i->path().contains("computer") || i->path().contains("phone")) {
            //This is an external media player (or tablet)
            //Get the model of this media player
            QString model = i->property("Model").toString();

            if (i->property("Serial").toString().length() == 40 && i->property("Vendor").toString().contains("Apple") && QFile("/usr/bin/idevice_id").exists()) { //This is probably an iOS device
                //Get the name of the iOS device
                QProcess iosName;
                iosName.start("idevice_id " + i->property("Serial").toString());
                iosName.waitForFinished();

                QString name(iosName.readAll());
                name = name.trimmed();

                if (name != "" && !name.startsWith("ERROR:")) {
                    model = name;
                }
            }
            if (i->property("State").toUInt() == 0) {
                if (QFile("/usr/bin/thefile").exists()) {
                    displayOutput.append(tr("Pair %1 using theFile to see battery status.").arg(model));
                } else {
                    displayOutput.append(tr("%1 battery unavailable. Device trusted?").arg(model));
                }
            } else {
                QString batteryText;
                batteryText.append(tr("%1% battery on %2").arg(QString::number(percentage), model));
                switch (i->property("State").toUInt()) {
                case 1:
                    batteryText.append(" (" + tr("Charging") + ")");
                    break;
                case 2:
                    batteryText.append(" (" + tr("Discharging") + ")");
                    break;
                case 3:
                    batteryText.append(" (" + tr("Empty") + ")");
                    break;
                case 4:
                case 6:
                    batteryText.append(" (" + tr("Full") + ")");
                    break;
                case 5:
                    batteryText.append(" (" + tr("Not Charging") + ")");
                    break;
                }
                displayOutput.append(batteryText)
                        ;
            }
        }
    }

    if (displayOutput.count() == 0) {
        ui->batteryFrame->setVisible(false);
    } else {
        ui->batteryFrame->setVisible(true);
        ui->BatteryLabel->setText(displayOutput.join(" · "));

        QString iconName;
        if (isCharging) {
            if (batLevel < 10) {
                iconName = "battery-charging-empty";
            } else if (batLevel < 30) {
                iconName = "battery-charging-020";
            } else if (batLevel < 50) {
                iconName = "battery-charging-040";
            } else if (batLevel < 70) {
                iconName = "battery-charging-060";
            } else if (batLevel < 90) {
                iconName = "battery-charging-080";
            } else {
                iconName = "battery-charging-100";
            }
        } else if (theLibsGlobal::instance()->powerStretchEnabled()) {
            if (batLevel < 10) {
                iconName = "battery-stretch-empty";
            } else if (batLevel < 30) {
                iconName = "battery-stretch-020";
            } else if (batLevel < 50) {
                iconName = "battery-stretch-040";
            } else if (batLevel < 70) {
                iconName = "battery-stretch-060";
            } else if (batLevel < 90) {
                iconName = "battery-stretch-080";
            } else {
                iconName = "battery-stretch-100";
            }
        } else {
            if (batLevel < 10) {
                iconName = "battery-empty";
            } else if (batLevel < 30) {
                iconName = "battery-020";
            } else if (batLevel < 50) {
                iconName = "battery-040";
            } else if (batLevel < 70) {
                iconName = "battery-060";
            } else if (batLevel < 90) {
                iconName = "battery-080";
            } else {
                iconName = "battery-100";
            }
        }

        ui->BatteryIcon->setPixmap(QIcon::fromTheme(iconName).pixmap(16, 16));
    }
}


void MainWindow::hideCover() {
    //if (!typePassword) {
        tPropertyAnimation* animation = new tPropertyAnimation(ui->coverFrame, "geometry");
        animation->setStartValue(ui->coverFrame->geometry());
        animation->setEndValue(QRect(0, 0, this->width(), 0));
        animation->setDuration(500);
        animation->setEasingCurve(QEasingCurve::OutCubic);
        animation->start();
        connect(animation, &tPropertyAnimation::finished, [=]() {
            QString name = qgetenv("USER");
            if (name.isEmpty()) {
                name = qgetenv("USERNAME");
            }

            QProcess* tscheckpass = new QProcess();
            tscheckpass->start("tscheckpass " + name);
            tscheckpass->waitForFinished();
            if (tscheckpass->exitCode() == 0) {
                QApplication::exit(0);
            }

            animation->deleteLater();
        });

        tPropertyAnimation* opacity = new tPropertyAnimation(passwordFrameOpacity, "opacity");
        opacity->setStartValue((float) 0);
        opacity->setEndValue((float) 1);
        opacity->setDuration(500);
        opacity->setEasingCurve(QEasingCurve::OutCubic);
        connect(opacity, &tPropertyAnimation::valueChanged, [=] {
            ui->pagesStack->update();
            ui->page_3->update();
            ui->userScrollContents->update();
        });
        connect(opacity, SIGNAL(finished()), opacity, SLOT(deleteLater()));
        opacity->start();

        //typePassword = true;
        coverHidden = true;
        ui->password->setFocus();
    //}
}

void MainWindow::showCover() {
    //if (typePassword) {
        tPropertyAnimation* animation = new tPropertyAnimation(ui->coverFrame, "geometry");
        animation->setStartValue(ui->coverFrame->geometry());
        animation->setEndValue(QRect(0, 0, this->width(), this->height()));
        animation->setDuration(500);
        animation->setEasingCurve(QEasingCurve::OutCubic);
        animation->start();
        connect(animation, SIGNAL(finished()), animation, SLOT(deleteLater()));
        connect(animation, &tPropertyAnimation::finished, [=] {
            passwordFrameOpacity->setOpacity(0);
        });

        tPropertyAnimation* opacity = new tPropertyAnimation(passwordFrameOpacity, "opacity");
        opacity->setStartValue((float) 1);
        opacity->setEndValue((float) 0);
        opacity->setDuration(500);
        opacity->setEasingCurve(QEasingCurve::OutCubic);
        connect(opacity, SIGNAL(finished()), opacity, SLOT(deleteLater()));
        opacity->start();

        //typePassword = false;
        ui->password->setText("");
        ui->password->setFocus();

        ui->unlockPromptFrame->setGeometry(0, this->height() - ui->unlockPromptFrame->height(), this->width(), ui->unlockPromptFrame->height());
        coverHidden = false;
    //}
}

void MainWindow::on_goBackButton_clicked()
{
    showCover();
}

void MainWindow::on_unlockButton_clicked()
{
    ui->password->setEnabled(false);
    ui->unlockButton->setEnabled(false);

    //TODO: check with PAM and start the session
    pid_t processId;
    if (login(currentLoginUsername, ui->password->text(), sessionExec, &processId, nullptr)) {
        /*
        if (sessionPath == NULL) {
            closeWindows();
            showCover();

            QTimer* checkRunTimer = new QTimer();
            checkRunTimer->setInterval(1000);
            connect(checkRunTimer, &QTimer::timeout, [=]() {
                int status;
                if (waitpid(processId, &status, WNOHANG) != 0) {
                    checkRunTimer->stop();
                    checkRunTimer->deleteLater();
                    logout();
                    openWindows();
                }
            });
            checkRunTimer->start();

            int status;
            waitpid(processId, &status, 0);
        } else {
            //We're just switching sessions. Show the cover again.
            showCover();
        }        */
    } else {
        ui->password->setText("");

        tToast* toast = new tToast();
        toast->setTitle(tr("Incorrect Password"));
        toast->setText(tr("The password you entered was incorrect. Please enter your password again."));
        connect(toast, SIGNAL(dismissed()), toast, SLOT(deleteLater()));
        toast->show(this);

        ui->password->setEnabled(true);
        ui->unlockButton->setEnabled(true);
        ui->password->setFocus();
    }

    /*
    QProcess tscheckpass;
    tscheckpass.start("tscheckpass " + name + " " + ui->password->text());
    tscheckpass.waitForFinished();
    if (tscheckpass.exitCode() == 0) {
        XUngrabKeyboard(QX11Info::display(), CurrentTime);
        XUngrabPointer(QX11Info::display(), CurrentTime);

        QApplication::exit();
    } else {
        QTimer::singleShot(1000, [=] {
            ui->password->setText("");

            tToast* toast = new tToast();
            toast->setTitle(tr("Incorrect Password"));
            toast->setText(tr("The password you entered was incorrect. Please enter your password again."));
            connect(toast, SIGNAL(dismissed()), toast, SLOT(deleteLater()));
            toast->show(this);

            ui->password->setEnabled(true);
            ui->unlockButton->setEnabled(true);
            ui->password->setFocus();
        });
        return;
    }*/

    ui->password->setEnabled(true);
    ui->unlockButton->setEnabled(true);
}

void MainWindow::on_password_returnPressed()
{
    ui->unlockButton->click();
}

void MainWindow::on_TurnOffScreenButton_clicked()
{
    showCover();
    QProcess::startDetached("xset dpms force off");
}

void MainWindow::animateClose() {
    tPropertyAnimation* passUp = new tPropertyAnimation(ui->passwordFrame, "geometry");
    passUp->setStartValue(ui->passwordFrame->geometry());
    passUp->setEndValue(QRect(0, -200, this->width(), this->height()));
    passUp->setDuration(250);
    passUp->setEasingCurve(QEasingCurve::InCubic);
    connect(passUp, SIGNAL(finished()), passUp, SLOT(deleteLater()));

    tPropertyAnimation* opacity = new tPropertyAnimation(passwordFrameOpacity, "opacity");
    opacity->setStartValue((float) 1);
    opacity->setEndValue((float) 0);
    opacity->setDuration(250);
    opacity->setEasingCurve(QEasingCurve::InCubic);
    connect(opacity, SIGNAL(finished()), opacity, SLOT(deleteLater()));
    connect(opacity, &tPropertyAnimation::finished, [=] {
        tVariantAnimation* anim = new tVariantAnimation();
        anim->setStartValue((float) 1);
        anim->setEndValue((float) 0);
        anim->setDuration(250);
        anim->setEasingCurve(QEasingCurve::InCubic);
        connect(anim, &tVariantAnimation::valueChanged, [=](QVariant value) {
            this->setWindowOpacity(value.toFloat());
        });
        connect(anim, SIGNAL(finished()), anim, SLOT(deleteLater()));
        connect(anim, &tVariantAnimation::finished, [=] {
            this->close();
        });

        anim->start();
    });
    opacity->start();
    passUp->start();
}

void MainWindow::on_SuspendButton_clicked()
{
    showCover();
    QList<QVariant> arguments;
    arguments.append(true);

    QDBusMessage message = QDBusMessage::createMethodCall("org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.login1.Manager", "Suspend");
    message.setArguments(arguments);
    QDBusConnection::systemBus().send(message);
}

struct LoginSession {
    QString sessionId;
    uint userId;
    QString username;
    QString seat;
    QDBusObjectPath path;
};
Q_DECLARE_METATYPE(LoginSession)

const QDBusArgument &operator<<(QDBusArgument &argument, const LoginSession &session) {
    argument.beginStructure();
    argument << session.sessionId << session.userId << session.username << session.seat << session.path;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, LoginSession &session) {
    argument.beginStructure();
    argument >> session.sessionId >> session.userId >> session.username >> session.seat >> session.path;
    argument.endStructure();
    return argument;
}

void MainWindow::on_passwordButton_toggled(bool checked)
{
    if (checked) {
        ui->loginStack->setCurrentIndex(0);
        ui->password->setFocus();
    }
}

void MainWindow::on_mousePasswordButton_toggled(bool checked)
{
    if (checked) {
        ui->loginStack->setCurrentIndex(1);
    }
}

void MainWindow::on_loginStack_currentChanged(int arg1)
{
    switch (arg1) {
        case 0:
            ui->passwordButton->setChecked(true);
            break;
        case 1:
            ui->mousePasswordButton->setChecked(true);
            break;
    }
}

void MainWindow::on_powerButton_clicked()
{
    this->close();
}

void MainWindow::on_goBackUserSelect_clicked()
{
    ui->pagesStack->setCurrentIndex(0);
}

void MainWindow::on_sessionSelect_triggered(QAction *arg1)
{
    ui->sessionSelect->setText(arg1->text());
    sessionExec = arg1->data().toString();
}
