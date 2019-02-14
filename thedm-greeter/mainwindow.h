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
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDBusInterface>
#include <QGraphicsOpacityEffect>
#include <QSettings>
#include <QTranslator>
#include <QLocalSocket>
#include "pam.h"

namespace Ui {
    class MainWindow;
}

class tPopover;
class QToolButton;

class MainWindow : public QMainWindow
{
        Q_OBJECT

    public:
        explicit MainWindow(QString vtnr, QWidget *parent = nullptr);
        ~MainWindow();

    public slots:
        void hideCover();

        void showCover();

        void showFullScreen();

    private slots:
        void tick();

        void BatteriesChanged();

        void BatteryChanged();

        void on_goBackButton_clicked();

        void on_unlockButton_clicked();

        void on_password_returnPressed();

        void on_loginStack_currentChanged(int arg1);

        void on_powerButton_clicked();

        void on_goBackUserSelect_clicked();

        void on_sessionSelect_triggered(QAction *arg1);

        void on_someoneElseButton_clicked();

        void on_unlockButton_2_clicked();

        void on_sessionSelect_2_triggered(QAction *arg1);

    private:
        Ui::MainWindow *ui;

        int moveY;
        bool coverHidden = false;
        bool isResetting = false;

        QString background;
        QPixmap image;

        QString mprisCurrentAppName = "";
        QStringList mprisDetectedApps;
        bool pauseMprisMenuUpdate = false;

        bool eventFilter(QObject *obj, QEvent *e);
        void resizeEvent(QResizeEvent* event);

        QList<QDBusInterface*> allDevices;
        QGraphicsOpacityEffect* passwordFrameOpacity;

        QString currentLoginUsername;
        int currentLoginUid;
        QString sessionExec;
        QTranslator* userTranslator;
        QList<QToolButton*> knownUsers;

        QProcess pulse;

        QString vtnr;

        QSettings* settings;
        QSettings* internalSettings;

        PamBackend* pamBackend;
        tPopover* currentInfoMessage = nullptr;

        QLocalSocket* master = nullptr;

        void attemptLoginUser(QString username, QString displayName, QString homeDir, int uid);
        void attemptStartSessionUser();
        void failLoginUser(QString reason);

        bool passwordScreenShown = false;
};

#endif // MAINWINDOW_H
