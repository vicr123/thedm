#include <QApplication>
#include <QTimer>

#include "pam.h"
#include "errno.h"
#include <sys/wait.h>

extern QString currentVt;
extern bool testMode;

PamBackend::PamBackend(QString username, QString sessionName, QObject* parent) : QObject(parent) {
    struct pam_conv pamConversationParams = {
        PamBackend::conversation,
        this
    };
    pam_start(sessionName.toLocal8Bit().data(), username.toLocal8Bit().constData(), &pamConversationParams, &this->pamHandle);

    pw = getpwnam(username.toStdString().data());
}

PamBackend::~PamBackend() {
    if (sessionOpen) {
        //Tear down the session
        pam_close_session(this->pamHandle, 0);

        if (!testMode) {
            //Kill the session if needed
            QDBusInterface sessionInterface("org.freedesktop.login1", "/org/freedesktop/login1/session/self", "org.freedesktop.login1.Session", QDBusConnection::systemBus());
            sessionInterface.call(QDBus::NoBlock, "Kill", "all", (int) SIGTERM);

            //Give any remaining processes 10 seconds and then kill them
            QTimer::singleShot(10000, [] {
                QDBusInterface sessionInterface("org.freedesktop.login1", "/org/freedesktop/login1/session/self", "org.freedesktop.login1.Session", QDBusConnection::systemBus());
                sessionInterface.call(QDBus::NoBlock, "Kill", "all", (int) SIGKILL);
            });
        }
    }
    pam_end(this->pamHandle, PAM_SUCCESS);
}

PamBackend::PamAuthenticationResult PamBackend::authenticate() {
    authenticating = true;
    int retval = pam_authenticate(this->pamHandle, 0);

    if (!authenticating) {
        return AuthCancelled;
    }

    authenticating = false;
    if (retval == PAM_SUCCESS) {
        return AuthSuccess;
    } else {
        return AuthFailure;
    }
}

PamBackend::PamAccountMgmtResult PamBackend::acctMgmt() {
    int retval = pam_acct_mgmt(this->pamHandle, PAM_SUCCESS);
    if (retval == PAM_SUCCESS) {
        return AccMgmtSuccess;
    } else if (retval == PAM_ACCT_EXPIRED) {
        return AccMgmtExipred;
    } else if (retval == PAM_NEW_AUTHTOK_REQD) {
        return AccMgmtNeedRefresh;
    } else {
        return AccMgmtFailure;
    }
}

bool PamBackend::setCred() {
    return pam_setcred(this->pamHandle, PAM_ESTABLISH_CRED) == PAM_SUCCESS;
}

bool PamBackend::changeAuthTok() {
    changingTok = true;
    newPasswords = new PasswordChanges;
    int retval = pam_chauthtok(this->pamHandle, PAM_CHANGE_EXPIRED_AUTHTOK);
    delete newPasswords;
    newPasswords = nullptr;

    return retval == PAM_SUCCESS;
}

void PamBackend::putenv(QString env, QString var) {
    pam_putenv(pamHandle, QString("%1=%2").arg(env, var).toLocal8Bit().data());
}

QString PamBackend::getenv(QString env) {
    return pam_getenv(pamHandle, env.toLocal8Bit().data());
}

void PamBackend::setItem(int type, const void* value) {
    pam_set_item(pamHandle, type, value);
}

bool PamBackend::startSession(QString exec) {
    if (int retval = pam_open_session(this->pamHandle, 0) != PAM_SUCCESS) {
        qDebug() << "PAM open session failed:" << retval;
        return false;
    }

    sessionOpen = true;

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

    if (!exec.isEmpty()) {
        //Fork and start the session

        int pid = fork();
        if (pid == -1) {
            //Fork failed
            QApplication::exit();
            return false;
        } else if (pid == 0) {
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
            }
            argv[execParts.count()] = nullptr;

            execvpe(argv[0], argv, pam_getenvlist(pamHandle));
            qDebug() << "execl failed." << strerror(errno);
            qDebug() << "Exiting now.";

            QApplication::exit();
            return false;
        } else {
            sessionPid = pid;
            return true;
        }
    } else {
        return true;
    }
}

void PamBackend::waitForSessionEnd() {
    waitpid(sessionPid, nullptr, 0);
}

PamInputCallback PamBackend::currentInputCallback() {
    return this->inputCallback;
}

int PamBackend::conversation(int num_msg, const struct pam_message **msg, struct pam_response **resp, void *appdata_ptr) {
    PamBackend* backend = (PamBackend*) appdata_ptr;

    //Make sure authentication or token changing hasn't been cancelled
    if (!backend->authenticating && !backend->changingTok) {
        //Authentication has been cancelled
        return PAM_CONV_ERR;
    }

    //Check for memory buffer errors
    *resp = (pam_response*) calloc(num_msg, sizeof(struct pam_response));
    if (*resp == nullptr) {
        return PAM_BUF_ERR;
    }

    QEventLoop* loop = new QEventLoop();
    int result = PAM_SUCCESS;
    for (int i = 0; i < num_msg; i++) {
        switch(msg[i]->msg_style) {
            case PAM_PROMPT_ECHO_ON:
            case PAM_PROMPT_ECHO_OFF: {
                QString message = QString::fromLocal8Bit(msg[i]->msg);
                if (message.startsWith("Current password:") && backend->changingTok) {
                    PamAuthTokCallback callback = [=](QString current, QString newPasswordInput, QString newConfirmInput, bool canceled) {
                        if (canceled) {
                            loop->exit(1);
                        } else {
                            (*resp)[i].resp = strdup(current.toLocal8Bit().data());
                            backend->newPasswords->newPassword = newPasswordInput;
                            backend->newPasswords->newConfirm = newConfirmInput;
                            loop->quit();
                        }
                    };

                    emit backend->passwordChangeRequired(true, callback);
                } else if (message.startsWith("New password:") && backend->changingTok) {
                    if (backend->newPasswords->newPassword != "") {
                        (*resp)[i].resp = strdup(backend->newPasswords->newPassword.toLocal8Bit().data());
                        QTimer::singleShot(0, loop, &QEventLoop::quit);
                    } else {
                        PamAuthTokCallback callback = [=](QString current, QString newPasswordInput, QString newConfirmInput, bool canceled) {
                            if (canceled) {
                                loop->exit(1);
                            } else {
                                (*resp)[i].resp = strdup(newPasswordInput.toLocal8Bit().data());
                                backend->newPasswords->newConfirm = newConfirmInput;
                                loop->quit();
                            }
                        };

                        emit backend->passwordChangeRequired(false, callback);
                    }
                } else if (message.startsWith("Retype new password:") && backend->changingTok && backend->newPasswords->newConfirm != "") {
                    (*resp)[i].resp = strdup(backend->newPasswords->newConfirm.toLocal8Bit().data());
                    QTimer::singleShot(0, loop, &QEventLoop::quit);
                } else {
                    PamInputCallback callback = [=](QString response, bool canceled) {
                        if (canceled) {
                            loop->exit(1);
                        } else {
                            (*resp)[i].resp = strdup(response.toLocal8Bit().data());
                            loop->quit();
                        }
                    };

                    backend->inputCallback = callback;

                    emit backend->inputRequired(msg[i]->msg_style == PAM_PROMPT_ECHO_ON, message, callback);
                }
                break;
            }
            case PAM_ERROR_MSG:
            case PAM_TEXT_INFO:
                QString message = QString::fromLocal8Bit(msg[i]->msg);
                if (message.startsWith("Changing password for") && backend->changingTok) {
                    //Ignore
                    QTimer::singleShot(0, loop, &QEventLoop::quit);
                } else {
                    emit backend->message(message);

                    //Make sure the message is shown for at least 3 seconds
                    QTimer::singleShot(3000, [=] {
                        loop->exit();
                    });

                }
                break;
        }

        if (loop->exec() != 0) {
            result = PAM_CONV_ERR;
            break;
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

        backend->authenticating = false;
        backend->changingTok = false;
    }

    return result;
}
