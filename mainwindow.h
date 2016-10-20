#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QX11Info>
#include <QProcess>
#include <QTimer>
#include <QDateTime>
#include <QDesktopWidget>
#include <QPropertyAnimation>
#include <QMouseEvent>
#include <QDBusMessage>
#include <QDBusConnection>
#include <QSettings>
#include <QDBusReply>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QFrame>
#include <QDebug>
#include <QMenu>
#include <QDir>
#include <QToolButton>
#include <QLineEdit>
#include <QApplication>
#include <QComboBox>
#include <QPushButton>
#include "pam.h"

#include <sys/wait.h>
#include <X11/Xlib.h>

//typedef QMap<QString, QDBusObjectPath> QStringDBusObjectPathMap;
struct QStringDBusObjectPathMap {
    QString sessionText;
    QDBusObjectPath objectPath;

};
Q_DECLARE_METATYPE(QStringDBusObjectPathMap)

inline QDBusArgument &operator<<(QDBusArgument &arg, const QStringDBusObjectPathMap &map) {
    arg.beginStructure();
    arg << map.sessionText << map.objectPath;
    arg.endStructure();
    return arg;
}

inline const QDBusArgument &operator>>(const QDBusArgument &arg, QStringDBusObjectPathMap &map) {
    qDebug() << arg.currentType();
    arg.beginStructure();
    arg >> map.sessionText >> map.objectPath;
    arg.endStructure();
    return arg;
}

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_passwordBox_returnPressed();

    void on_Cover_clicked();

    void on_Cover_MouseDown(QMouseEvent *);

    void on_Cover_MouseMove(QMouseEvent *);

    void on_Cover_MouseUp(QMouseEvent *);

    void hideCover();

    void showCover();

    void on_passwordBox_textEdited(const QString &arg1);
    void on_goBack_clicked();

    void resizeSlot();

    void on_sessionSelect_triggered(QAction *arg1);

    void on_loginButton_clicked();

    void on_turnOffScreen_clicked();

    void on_pushButton_clicked();

    void on_suspendButton_clicked();

    void on_usernameBox_currentIndexChanged(int index);

private:
    Ui::MainWindow *ui;

    QImage image;

    void resizeEvent(QResizeEvent* event);
    void keyPressEvent(QKeyEvent* event);

    int moveY;
    bool typePassword = false;
    QSize imageSize;

    QString sessionExec;
    QDBusObjectPath* sessionPath;
};

#endif // MAINWINDOW_H
