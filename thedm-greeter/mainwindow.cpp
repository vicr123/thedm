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
#include <QMessageBox>
#include "pamquestion.h"
#include "pamchauthtok.h"
#include "poweroptions.h"
#include <tvirtualkeyboard.h>
#include <ttoast.h>
#include <tpopover.h>
#include "pam.h"

#include <grp.h>
#include <pwd.h>

#include "nativeeventfilter.h"

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/XF86keysym.h>
#undef KeyPress
#undef KeyRelease

MainWindow::MainWindow(QString vtnr, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    settings = new QSettings("/etc/thedm.conf", QSettings::IniFormat);
    internalSettings = new QSettings(QString(SHAREDIR) + "internal.conf", QSettings::IniFormat);

    this->vtnr = vtnr;

    userTranslator = new QTranslator();
    QApplication::installTranslator(userTranslator);

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

    //pulse.start("pulseaudio");

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
            knownUsers.append(userButton);
            connect(userButton, &QPushButton::clicked, [=] {
                attemptLoginUser(username, userButton->text(), home, pw->pw_uid);
            });
        }
    }
    ui->userScroll->setFixedHeight(qMin((float) ui->userList->sizeHint().height(), this->height() / 4 * theLibsGlobal::getDPIScaling()));

    //Check to see if there's only one known user
    if (knownUsers.count() == 1) {
        //Immediately start authentication as the only known user
        QTimer::singleShot(0, [=] {
            knownUsers.first()->click();
        });
    }

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
    ui->sessionSelect_2->setMenu(sessionsMenu);

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

    nativeEventFilter = new NativeEventFilter();
    connect(nativeEventFilter, &NativeEventFilter::PowerPressed, [=] {
        ui->powerButton->click();
    });
    QApplication::instance()->installNativeEventFilter(nativeEventFilter);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::attemptLoginUser(QString username, QString displayName, QString homeDir, int uid) {
    this->setEnabled(false);
    passwordScreenShown = false;

    QSettings tsSettings(homeDir + "/.config/theSuite/theShell.conf", QSettings::IniFormat);

    QString localeName = tsSettings.value("locale/language", "en_US").toString();
    QLocale userLocale(localeName);
    QLocale::setDefault(userLocale);

    if (userLocale.name() == "C") {
        userTranslator->load(localeName, QString(SHAREDIR) + "translations");
    } else {
        userTranslator->load(userLocale.name(), QString(SHAREDIR) + "translations");
    }
    ui->retranslateUi(this);

    //Choose the previously selected session for the user, otherwise leave it as the default
    internalSettings->beginGroup("sessions");
    if (internalSettings->contains(QString::number(uid))) {
        QString sessionName = internalSettings->value(QString::number(uid)).toString();
        for (QAction* a : ui->sessionSelect->menu()->actions()) {
            if (a->text() == sessionName) on_sessionSelect_triggered(a); //Select the session if found
        }
    }
    internalSettings->endGroup();

    pamBackend = new PamBackend(username);
    pamBackend->putenv("DISPLAY", qgetenv("DISPLAY"));
    pamBackend->putenv("XDG_SESSION_CLASS", "user");
    pamBackend->putenv("XDG_SESSION_TYPE", "x11");
    pamBackend->putenv("XDG_SEAT", "seat0");
    pamBackend->putenv("XDG_VTNR", this->vtnr);
    pamBackend->setItem(PAM_XDISPLAY, qgetenv("DISPLAY"));
    pamBackend->setItem(PAM_TTY, qgetenv("DISPLAY"));
    connect(pamBackend, &PamBackend::inputRequired, [=](bool echo, QString msg, PamInputCallback callback) {
        //Dismiss the info message if it's showing
        if (this->currentInfoMessage != nullptr) {
            this->currentInfoMessage->dismiss();
            this->currentInfoMessage = nullptr;
        }

        if (msg == "Password: ") {
            ui->loginStack->setCurrentIndex(0);
            ui->usernameLabel->setText(tr("Hi %1!").arg(displayName));
            ui->pagesStack->setCurrentIndex(1);
            ui->LoginOptions->setVisible(true);
            currentLoginUsername = username;

            if (QFile(homeDir + "/.theshell/mousepassword").exists()) {
                ui->mousePasswordWarning->setVisible(true);
                ui->LoginOptions->setVisible(true);
            } else {
                ui->mousePasswordWarning->setVisible(false);
                ui->LoginOptions->setVisible(false);
            }

            QTimer::singleShot(100, [=] {
                ui->PasswordUnderline->startAnimation();

                QMetaObject::Connection* cn = new QMetaObject::Connection();
                *cn = connect(ui->pagesStack, &tStackedWidget::currentChanged, [=](int current) {
                    disconnect(*cn);

                    ui->password->setFocus(Qt::PopupFocusReason);
                    delete cn;
                });
            });
            passwordScreenShown = true;
        } else {
            PamQuestion* q = new PamQuestion(echo, msg);
            q->setTitle(tr("PAM Challenge"));
            q->setPlaceholder(tr("Response"));

            tPopover* p = new tPopover(q);
            p->setPopoverWidth(400 * theLibsGlobal::getDPIScaling());
            bool* responded = new bool(false);

            connect(q, &PamQuestion::dismiss, [=] {
                p->dismiss();
            });
            connect(q, &PamQuestion::respond, [=](QString response) {
                *responded = true;
                callback(response, false);
            });
            connect(p, &tPopover::dismissed, [=] {
                if (!*responded) {
                    callback("", true);
                }
                delete responded;
                p->deleteLater();
                q->deleteLater();
            });

            p->show(this);
        }
        this->setEnabled(true);
    });
    connect(pamBackend, &PamBackend::message, [=](QString warning) {
        //Dismiss the info message if it's showing
        if (this->currentInfoMessage != nullptr) {
            this->currentInfoMessage->dismiss();
            this->currentInfoMessage = nullptr;
        }

        PamQuestion* q = new PamQuestion(true, warning);
        q->setTitle(tr("PAM Message"));
        q->setResponseRequired(false);

        tPopover* p = new tPopover(q);
        p->setPopoverWidth(400 * theLibsGlobal::getDPIScaling());

        connect(q, &PamQuestion::dismiss, [=] {
            p->dismiss();
        });
        connect(p, &tPopover::dismissed, [=] {
            p->deleteLater();
            q->deleteLater();

            if (this->currentInfoMessage == p) {
                this->currentInfoMessage = nullptr;
                this->setEnabled(false);
            }
        });

        p->show(this);
        this->currentInfoMessage = p;
        this->setEnabled(true);
    });
    connect(pamBackend, &PamBackend::passwordChangeRequired, [=](bool needOldPassword, PamAuthTokCallback callback) {
        //Dismiss the info message if it's showing
        if (this->currentInfoMessage != nullptr) {
            this->currentInfoMessage->dismiss();
            this->currentInfoMessage = nullptr;
        }

        PamChauthtok* q = new PamChauthtok(needOldPassword);

        tPopover* p = new tPopover(q);
        p->setPopoverWidth(400 * theLibsGlobal::getDPIScaling());
        bool* responded = new bool(false);

        connect(q, &PamChauthtok::dismiss, [=] {
            p->dismiss();
        });
        connect(q, &PamChauthtok::respond, [=](QString current, QString newPasswordInput, QString confirmPasswordInput) {
            *responded = true;
            callback(current, newPasswordInput, confirmPasswordInput, false);
        });
        connect(p, &tPopover::dismissed, [=] {
            if (!*responded) {
                callback("", "", "", true);
            }
            delete responded;
            p->deleteLater();
            q->deleteLater();
        });

        p->show(this);
        this->setEnabled(true);
    });

    PamBackend::PamAuthenticationResult authResult = pamBackend->authenticate();
    //Dismiss the info message if it's showing
    if (this->currentInfoMessage != nullptr) {
        this->currentInfoMessage->dismiss();
        this->currentInfoMessage = nullptr;
    }

    if (authResult == PamBackend::AuthFailure) {
        failLoginUser(tr("Authentication failed."));
        //Try to log in again
        QTimer::singleShot(0, [=] {
            attemptLoginUser(username, displayName, homeDir, uid);
        });
        return;
    } else if (authResult == PamBackend::AuthCancelled) {
        //Go back to the main screen
        ui->pagesStack->setCurrentIndex(0);
        return;
    }

    pamBackend->putenv("XDG_SESSION_DESKTOP", "theShell");

    PamBackend::PamAccountMgmtResult accMgmtResult = pamBackend->acctMgmt();

    //Dismiss the info message if it's showing
    if (this->currentInfoMessage != nullptr) {
        this->currentInfoMessage->dismiss();
        this->currentInfoMessage = nullptr;
    }
    if (accMgmtResult == PamBackend::AccMgmtFailure) {
        failLoginUser(tr("PAM Account Management failed"));
        return;
    } else if (accMgmtResult == PamBackend::AccMgmtExipred) {
        failLoginUser(tr("Your account has expired. Contact your system administrator for more details."));
        return;
    } else if (accMgmtResult == PamBackend::AccMgmtNeedRefresh) {
        //Ask the user for a new password
        if (!pamBackend->changeAuthTok()) {
            failLoginUser(tr("Unable to update your password"));
            return;
        }
    }

    //TODO: Check to see if a password was not input and pause the PAM transaction here if so
    this->currentLoginUid = uid;
    if (!passwordScreenShown) {
        //Pause the PAM transaction so the user can choose the session
        ui->loginStack->setCurrentIndex(1);
        ui->usernameLabel_2->setText(tr("Hi %1!").arg(displayName));
        ui->pagesStack->setCurrentIndex(1);
        currentLoginUsername = username;
        ui->LoginOptions->setVisible(false);
        this->setEnabled(true);
    } else {
        attemptStartSessionUser();
    }
}

void MainWindow::attemptStartSessionUser() {
    if (!pamBackend->setCred()) {
        failLoginUser(tr("PAM Credential Management failed"));
    }

    //Save the session
    internalSettings->beginGroup("sessions");
    internalSettings->setValue(QString::number(this->currentLoginUid), ui->sessionSelect->text());
    internalSettings->endGroup();
    internalSettings->sync();

    //Kill pulseaudio
    //The DE will autostart it
    //pulse.kill();

    if (!pamBackend->startSession(sessionExec)) {
        failLoginUser(tr("Session unable to be opened"));
    }

    QApplication::instance()->removeNativeEventFilter(nativeEventFilter);
    nativeEventFilter->deleteLater();

    //At this point, the session is running.
    QApplication::setQuitOnLastWindowClosed(false);

    //Animate away
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
        this->hide();
        QApplication::processEvents();

        //Now wait for the session to close
        pamBackend->waitForSessionEnd();

        //The session has ended; close the PAM session and exit
        pamBackend->deleteLater();
        QApplication::exit(0);
    });

    tVariantAnimation* palAnim = new tVariantAnimation();
    palAnim->setStartValue(this->palette().color(QPalette::Window));
    palAnim->setEndValue(QColor(0, 0, 0));
    palAnim->setDuration(250);
    palAnim->setEasingCurve(QEasingCurve::InCubic);
    connect(palAnim, &tVariantAnimation::valueChanged, [=](QVariant value) {
        QPalette pal = this->palette();
        pal.setColor(QPalette::Window, value.value<QColor>());
        this->setPalette(pal);
    });
    connect(palAnim, &tVariantAnimation::finished, palAnim, &tVariantAnimation::deleteLater);

    opacity->start();
    passUp->start();
    palAnim->start();
}

void MainWindow::failLoginUser(QString reason) {
    //Dismiss the info message if it's showing
    if (this->currentInfoMessage != nullptr) {
        this->currentInfoMessage->dismiss();
        this->currentInfoMessage = nullptr;
    }

    tToast* toast = new tToast();
    toast->setTitle(tr("Couldn't log in"));
    toast->setText(reason);
    connect(toast, &tToast::dismissed, toast, &tToast::deleteLater);
    toast->show(this);

    this->setEnabled(true);
    if (reason != tr("Authentication failed.")) {
        ui->pagesStack->setCurrentIndex(0);
    }
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

        this->activateWindow();
    });

    QMargins currentMargins = ui->coverFrame->contentsMargins();
    currentMargins.setBottom(ui->unlockPromptFrame->height());
    ui->coverFrame->layout()->setContentsMargins(currentMargins);
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
    if (coverHidden) {
        ui->coverFrame->setGeometry(0, -this->height(), this->width(), this->height());
    } else {
        ui->coverFrame->setGeometry(0, 0, this->width(), this->height());
    }
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
                displayOutput.append(batteryText);
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
    tPropertyAnimation* animation = new tPropertyAnimation(ui->coverFrame, "geometry");
    animation->setStartValue(ui->coverFrame->geometry());
    animation->setEndValue(QRect(0, 0, this->width(), 0));
    animation->setDuration(500);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    animation->start();
    connect(animation, &tPropertyAnimation::finished, animation, &tPropertyAnimation::deleteLater);

    tPropertyAnimation* opacity = new tPropertyAnimation(passwordFrameOpacity, "opacity");
    opacity->setStartValue((float) 0);
    opacity->setEndValue((float) 1);
    opacity->setDuration(500);
    opacity->setEasingCurve(QEasingCurve::OutCubic);
    connect(opacity, &tPropertyAnimation::valueChanged, [=] {
        ui->pagesStack->update();
        ui->pagesStack->currentWidget()->update();
        ui->userScrollContents->update();
    });
    connect(opacity, SIGNAL(finished()), opacity, SLOT(deleteLater()));
    opacity->start();

    coverHidden = true;
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
    this->setEnabled(false);
    pamBackend->currentInputCallback()(ui->password->text(), false);
    ui->password->setText("");
}

void MainWindow::on_password_returnPressed()
{
    ui->unlockButton->click();
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
    if (!powerOptionsShown) {
        powerOptionsShown = true;
        PowerOptions* power = new PowerOptions();

        tPopover* p = new tPopover(power);
        p->setPopoverWidth(400 * theLibsGlobal::getDPIScaling());

        connect(power, &PowerOptions::dismiss, [=] {
            p->dismiss();
        });
        connect(power, &PowerOptions::showCover, [=] {
            showCover();
        });
        connect(p, &tPopover::dismissed, [=] {
            p->deleteLater();
            power->deleteLater();
            powerOptionsShown = false;
        });

        p->show(this);
    }
}

void MainWindow::on_goBackUserSelect_clicked()
{
    if (ui->loginStack->currentIndex() == 0) {
        pamBackend->currentInputCallback()("", true);
        ui->password->setText("");
    } else {
        ui->pagesStack->setCurrentIndex(0);
        pamBackend->deleteLater();
        this->pamBackend = nullptr;
    }
}

void MainWindow::on_sessionSelect_triggered(QAction *arg1)
{
    ui->sessionSelect->setText(arg1->text());
    ui->sessionSelect_2->setText(arg1->text());
    sessionExec = arg1->data().toString();
}

void MainWindow::on_someoneElseButton_clicked()
{
    PamQuestion* q = new PamQuestion(true, tr("What's the username of the user you'd like to log in as?"));
    q->setTitle(tr("Log in as other user"));
    q->setPlaceholder(tr("Username"));

    tPopover* p = new tPopover(q);
    p->setPopoverWidth(400 * theLibsGlobal::getDPIScaling());

    connect(q, &PamQuestion::dismiss, [=] {
        p->dismiss();
    });
    connect(q, &PamQuestion::respond, [=](QString response) {
        struct passwd *pw = getpwnam(response.toLocal8Bit().data());
        if (pw != nullptr) {
            QString gecosData = pw->pw_gecos;
            QStringList gecosDataList = gecosData.split(",");
            QString username = pw->pw_name;
            QString home = pw->pw_dir;
            QString displayName;

            if (gecosDataList.count() == 0 || gecosData == "") {
                displayName = username;
            } else {
                displayName = gecosDataList.first();
            }

            QTimer::singleShot(0, [=] {
                attemptLoginUser(username, displayName, home, pw->pw_uid);
            });
        } else {
            tToast* toast = new tToast();
            toast->setTitle(tr("User not found"));
            toast->setText(tr("Couldn't find that user. Make sure you've spelled the username correctly."));
            connect(toast, &tToast::dismissed, toast, &tToast::deleteLater);
            toast->show(this);
        }
    });
    connect(p, &tPopover::dismissed, [=] {
        p->deleteLater();
        q->deleteLater();
    });

    p->show(this);
}

void MainWindow::on_unlockButton_2_clicked()
{
    attemptStartSessionUser();
}

void MainWindow::on_sessionSelect_2_triggered(QAction *arg1)
{
    on_sessionSelect_triggered(arg1);
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    qDebug() << event->key();
}
