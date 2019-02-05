#include <QApplication>

#include "pam.h"
#include "errno.h"

extern QString currentVt;

static pam_handle_t *pamHandle;
QString currentPassword, currentUsername;

PamBackend::PamBackend(QString username, QString sessionName, QObject* parent) : QObject(parent) {
    struct pam_conv pamConversationParams = {
        PamBackend::conversation,
        this
    };
    pam_start(sessionName.toLocal8Bit().data(), username.toLocal8Bit().constData(), &pamConversationParams, &this->pamHandle);

    pw = getpwnam(username.toStdString().data());
}

PamBackend::~PamBackend() {
    pam_end(this->pamHandle, PAM_SUCCESS);
}

bool PamBackend::authenticate() {
    this->password = password;
    return pam_authenticate(this->pamHandle, 0) == PAM_SUCCESS;
}

bool PamBackend::acctMgmt() {
    return pam_acct_mgmt(this->pamHandle, PAM_SUCCESS) == PAM_SUCCESS;
}

bool PamBackend::setCred() {
    return pam_setcred(this->pamHandle, PAM_ESTABLISH_CRED) == PAM_SUCCESS;
}

void PamBackend::putenv(QString env, QString var) {
    pam_putenv(pamHandle, QString("%1=%2").arg(env, var).toLocal8Bit().data());
}

QString PamBackend::getenv(QString env) {
    return pam_getenv(pamHandle, env.toLocal8Bit().data());
}

bool PamBackend::startSession() {
    if (pam_open_session(this->pamHandle, 0) != PAM_SUCCESS) {
        return false;
    }

    //Change the UID and GID of this process
    pid_t sid = setsid();
    setgid(pw->pw_gid);
    setuid(pw->pw_uid);
    chdir(pw->pw_dir);

    QDBusInterface userInterface("org.freedesktop.login1", "/org/freedesktop/login1/user/self", "org.freedesktop.login1.User", QDBusConnection::systemBus());

    //Set up other environment variables
    pam_putenv(pamHandle, QString("HOME=").append(pw->pw_dir).toLocal8Bit().data());
    pam_putenv(pamHandle, QString("PWD=").append(pw->pw_dir).toLocal8Bit().data());
    pam_putenv(pamHandle, QString("SHELL=").append(pw->pw_shell).toLocal8Bit().data());
    pam_putenv(pamHandle, QString("USER=").append(pw->pw_name).toLocal8Bit().data());
    pam_putenv(pamHandle, QString("PATH=/usr/local/sbin:/usr/local/bin:/usr/bin").toLocal8Bit().data());
    pam_putenv(pamHandle, QString("MAIL=").append(_PATH_MAILDIR).toLocal8Bit().data());
    pam_putenv(pamHandle, QString("XAUTHORITY=").append(pw->pw_dir).append("/.Xauthority").toLocal8Bit().data());
    pam_putenv(pamHandle, QString("DISPLAY=").append(qgetenv("DISPLAY")).toLocal8Bit().data());
    pam_putenv(pamHandle, QString("PATH=").append(qgetenv("PATH")).toLocal8Bit().data());
    //pam_putenv(pamHandle, "XDG_SESSION_CLASS=user");

    //Blank out the current password
    currentPassword = "";

    return true;
}

void PamBackend::runSession(QString exec) {
    //Start DBus
    QProcess* dbusLaunch = new QProcess;
    dbusLaunch->start("dbus-launch");
    dbusLaunch->waitForFinished();
    for (QString env : QString(dbusLaunch->readAll()).split("\n")) {
        pam_putenv(pamHandle, env.toLocal8Bit().data());
    }

    //Start Pulse
    QProcess::startDetached("pulseaudio");

    //Start the new process
    QStringList execParts = exec.split(" ");
    char* argv[execParts.count() + 1];
    for (int i = 0; i < execParts.count(); i++) {
        argv[i] = (char*) malloc(execParts.at(i).toLocal8Bit().length());
        strcpy(argv[i], execParts.at(i).toLocal8Bit().constData());
        //argv[i] = execParts.at(i).toLocal8Bit().data();
        qDebug() << argv[i];
    }
    argv[execParts.count()] = nullptr;

    execvpe(argv[0], argv, pam_getenvlist(pamHandle));
    qDebug() << "execl failed." << strerror(errno);
    qDebug() << "Exiting now.";

    QApplication::exit();
}

PamInputCallback PamBackend::currentInputCallback() {
    return this->inputCallback;
}

PamMessageCallback PamBackend::currentMessageCallback() {
    return this->messageCallback;
}

int PamBackend::conversation(int num_msg, const struct pam_message **msg, struct pam_response **resp, void *appdata_ptr) {
    //Check for memory buffer errors
    *resp = (pam_response*) calloc(num_msg, sizeof(struct pam_response));
    if (*resp == nullptr) {
        return PAM_BUF_ERR;
    }

    PamBackend* backend = (PamBackend*) appdata_ptr;

    QEventLoop* loop = new QEventLoop();
    int result = PAM_SUCCESS;
    for (int i = 0; i < num_msg; i++) {
        switch(msg[i]->msg_style) {
            case PAM_PROMPT_ECHO_ON:
            case PAM_PROMPT_ECHO_OFF: {
                PamInputCallback callback = [=](QString response, bool canceled) {
                    if (canceled) {
                        loop->exit(1);
                    } else {
                        (*resp)[i].resp = strdup(response.toLocal8Bit().data());
                        loop->quit();
                    }
                };

                backend->inputCallback = callback;

                emit backend->inputRequired(msg[i]->msg_style == PAM_PROMPT_ECHO_ON, msg[i]->msg, callback);
                break;
            }
            case PAM_ERROR_MSG:
                result = PAM_CONV_ERR;
            case PAM_TEXT_INFO:
                PamMessageCallback callback = [=] {
                    loop->quit();
                };

                backend->messageCallback = callback;

                emit backend->message(msg[i]->msg, callback);
                break;
        }

        if (loop->exec() != 0) {
            result = PAM_CONV_ERR;
        }

        if (result != PAM_SUCCESS) {
            break;
        }
    }
    loop->deleteLater();

    if (result != PAM_SUCCESS) {
        //If there was an issue, free memory pointers
        free(*resp);
        *resp = nullptr;
    }

    return result;
}

bool login(QString username, QString password, QString execFile, pid_t *child_pid, QDBusObjectPath* resumeSession) {
    //Prevent paging to disk
    mlockall(MCL_CURRENT | MCL_FUTURE);

    //Set XDG environment variables
    //Get current user information
    struct passwd *pw = getpwnam(username.toStdString().data());

    //Get Current Session
    QDBusInterface sessionInterface("org.freedesktop.login1", "/org/freedesktop/login1/session/self", "org.freedesktop.login1.Session", QDBusConnection::systemBus());
    QDBusInterface userInterface("org.freedesktop.login1", "/org/freedesktop/login1/user/self", "org.freedesktop.login1.User", QDBusConnection::systemBus());
    /*qputenv("XDG_SESSION_ID", sessionInterface.property("Id").toString().toUtf8());
    qputenv("XDG_VTNR", sessionInterface.property("VTNr").toString().toUtf8());
    qputenv("XDG_RUNTIME_DIR", QString("/run/user/" + QString::number(pw->pw_uid)).toUtf8());*/

    //TODO: Correct these environment variables
    /*qputenv("XDG_SESSION_PATH", "/org/freedesktop/DisplayManager/Session0");
    qputenv("XDG_SEAT_PATH", "/org/freedesktop/DisplayManager/Seat0");
    qputenv("XDG_SEAT", "seat0");*/

    const char *data[2];
    data[0] = username.toStdString().data();
    data[1] = password.toStdString().data();
    struct pam_conv pamConversationParams = {
        conversation, data
    };

    currentUsername = username;
    currentPassword = password;

    int result = pam_start("thedm", username.toStdString().data(), &pamConversationParams, &pamHandle);
    if (result != PAM_SUCCESS) {
        //ERROR ERROR
        qCritical() << "pam_start failed";
        return false;
    }

    pam_set_item(pamHandle, PAM_USER, username.toUtf8().data());
    pam_set_item(pamHandle, PAM_XDISPLAY, qgetenv("DISPLAY").data());
    pam_set_item(pamHandle, PAM_TTY, qgetenv("DISPLAY").data());
    pam_misc_setenv(pamHandle, "XDG_SEAT", "seat0", false);
    pam_misc_setenv(pamHandle, "XDG_VTNR", sessionInterface.property("VTNr").toString().toUtf8(), false);

    result = pam_authenticate(pamHandle, 0);
    if (result != PAM_SUCCESS) {
        //ERROR ERROR
        qCritical() << "pam_authenticate failed with error " + QString::number(result);
        return false;
    }

    //Make sure that user is currently allowed to log in
    result = pam_acct_mgmt(pamHandle, 0);
    if (result != PAM_SUCCESS) {
        //ERROR ERROR
        qWarning() << "pam_acct_mgmt failed";
        return false;
    }

    pam_misc_setenv(pamHandle, "PATH", "/usr/local/bin:/usr/bin:/bin", false);
    pam_misc_setenv(pamHandle, "USER", pw->pw_name, false);
    pam_misc_setenv(pamHandle, "LOGNAME", pw->pw_name, false);
    pam_misc_setenv(pamHandle, "HOME", pw->pw_dir, false);
    pam_misc_setenv(pamHandle, "SHELL", pw->pw_shell, false);

    if (getuid() == 0) {
        initgroups(username.toStdString().data(), pw->pw_gid);
    }

    //Get an authentication token
    result = pam_setcred(pamHandle, PAM_ESTABLISH_CRED);
    if (result != PAM_SUCCESS) {
        //ERROR ERROR
        qWarning() << "pam_setcred failed";
        return false;
    }

    //At this point, the user is authenticated.
    //If the user currently has a session that should be unlocked, activate that session now.
    if (resumeSession != NULL) {
        //Activate the session.
        QDBusMessage activateMessage = QDBusMessage::createMethodCall("org.freedesktop.login1", resumeSession->path(), "org.freedesktop.login1.Session", "Activate");
        QDBusConnection::systemBus().call(activateMessage, QDBus::NoBlock);

        //Unlock the session
        QDBusMessage unlockMessage = QDBusMessage::createMethodCall("org.freedesktop.login1", resumeSession->path(), "org.freedesktop.login1.Session", "Unlock");
        QDBusConnection::systemBus().call(unlockMessage, QDBus::NoBlock);


        //Log out of this session.
        return logout();
    }

    result = pam_open_session(pamHandle, 0);
    if (result != PAM_SUCCESS) {
        //ERROR ERROR
        //Release the authentication token
        pam_setcred(pamHandle, PAM_DELETE_CRED);
        qWarning() << "pam_open_session failed";
        return false;
    }


    //Set up other environment variables
    pam_putenv(pamHandle, QString("HOME=").append(pw->pw_dir).toLocal8Bit().data());
    pam_putenv(pamHandle, QString("PWD=").append(pw->pw_dir).toLocal8Bit().data());
    pam_putenv(pamHandle, QString("SHELL=").append(pw->pw_shell).toLocal8Bit().data());
    pam_putenv(pamHandle, QString("USER=").append(pw->pw_name).toLocal8Bit().data());
    pam_putenv(pamHandle, QString("PATH=/usr/local/sbin:/usr/local/bin:/usr/bin").toLocal8Bit().data());
    pam_putenv(pamHandle, QString("MAIL=").append(_PATH_MAILDIR).toLocal8Bit().data());
    pam_putenv(pamHandle, QString("XAUTHORITY=").append(pw->pw_dir).append("/.Xauthority").toLocal8Bit().data());
    pam_putenv(pamHandle, QString("DISPLAY=").append(qgetenv("DISPLAY")).toLocal8Bit().data());
    pam_putenv(pamHandle, QString("PATH=").append(qgetenv("PATH")).toLocal8Bit().data());
    pam_putenv(pamHandle, QString("XDG_RUNTIME_DIR=").append(userInterface.property("RuntimePath").toString()).toLocal8Bit().data());
    pam_putenv(pamHandle, "XDG_SESSION_CLASS=user");
    //pam_putenv(pamHandle, QString("DISPLAY").append(pw->pw_dir).append("/.Xauthority").toLocal8Bit().data());
    /*qputenv("PWD", pw->pw_dir);
    qputenv("SHELL", pw->pw_shell);
    qputenv("USER", pw->pw_name);
    qputenv("PATH", "/usr/local/sbin:/usr/local/bin:/usr/bin");
    qputenv("MAIL", _PATH_MAILDIR);
    qputenv("XAUTHORITY", QByteArray(pw->pw_dir) + "/.Xauthority");
    qputenv("XDG_SESSION_CLASS", "user");*/

    //Blank out the current password
    currentPassword = "";

    //Change the UID and GID of this process
    pid_t sid = setsid();

    //Set group information
    setgid(pw->pw_gid);
    setuid(pw->pw_uid);
    chdir(pw->pw_dir);

    //Start DBus
    QProcess* dbusLaunch = new QProcess;
    dbusLaunch->start("dbus-launch");
    dbusLaunch->waitForFinished();
    for (QString env : QString(dbusLaunch->readAll()).split("\n")) {
        pam_putenv(pamHandle, env.toLocal8Bit().data());
    }

    //Start Pulse
    QProcess::startDetached("pulseaudio");

    //Start the new process
    QStringList execParts = execFile.split(" ");
    char* argv[execParts.count() + 1];
    for (int i = 0; i < execParts.count(); i++) {
        argv[i] = (char*) malloc(execParts.at(i).toLocal8Bit().length());
        strcpy(argv[i], execParts.at(i).toLocal8Bit().constData());
        //argv[i] = execParts.at(i).toLocal8Bit().data();
        qDebug() << argv[i];
    }
    argv[execParts.count()] = nullptr;

    execvpe(argv[0], argv, pam_getenvlist(pamHandle));
    qDebug() << "execl failed." << strerror(errno);
    qDebug() << "Exiting now.";

    QApplication::exit();
    return false;
}

bool logout() {
    qputenv("XDG_SESSION_CLASS", "greeter");

    int result = pam_close_session(pamHandle, 0);
    if (result != PAM_SUCCESS) {
        //ERROR ERROR
        //Release the authentication token
        pam_setcred(pamHandle, PAM_DELETE_CRED);
        qWarning() << "pam_close_session failed";
        return false;
    }

    //Release the authentication token
    result = pam_setcred(pamHandle, PAM_DELETE_CRED);
    if (result != PAM_SUCCESS) {
        //ERROR ERROR
        qWarning() << "pam_setcred failed";
        return false;
    }

    pam_end(pamHandle, result);
    pamHandle = NULL;

    return true;
}

int conversation(int num_msg, const pam_message **msg, pam_response **resp, void *appdata_ptr) {
    //Check for memory buffer errors
    *resp = (pam_response*) calloc(num_msg, sizeof(struct pam_response));
    if (*resp == NULL) {
        return PAM_BUF_ERR;
    }

    PamBackend* backend = (PamBackend*) appdata_ptr;

    QEventLoop* loop = new QEventLoop();
    int result = PAM_SUCCESS;
    for (int i = 0; i < num_msg; i++) {
        switch(msg[i]->msg_style) {
            case PAM_PROMPT_ECHO_ON:
            case PAM_PROMPT_ECHO_OFF:
                emit backend->inputRequired(msg[i]->msg_style == PAM_PROMPT_ECHO_ON, msg[i]->msg, [=](QString response, bool canceled) {
                    if (canceled) {
                        loop->exit(1);
                    } else {
                        (*resp)[i].resp = strdup(response.toLocal8Bit().data());
                        loop->quit();
                    }
                });
                break;
            case PAM_ERROR_MSG:
                result = PAM_CONV_ERR;
            case PAM_TEXT_INFO:
                emit backend->message(msg[i]->msg, [=] {
                    loop->quit();
                });
                qWarning() << msg[i]->msg;
                break;
        }

        if (loop->exec() != 0) {
            result = PAM_CONV_ERR;
        }

        if (result != PAM_SUCCESS) {
            break;
        }
    }
    loop->deleteLater();

    if (result != PAM_SUCCESS) {
        //If there was an issue, free memory pointers
        free(*resp);
        *resp = 0;
    }

    return result;
}
