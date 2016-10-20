#ifndef PAM_H
#define PAM_H

#include <QString>
#include <QProcess>
#include <QDebug>
#include <QX11Info>
#include <security/pam_appl.h>
#include <security/pam_misc.h>
#include <pwd.h>
#include <paths.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>

bool login(QString username, QString password, QString exec, pid_t *child_pid, QDBusObjectPath* resumeSession = NULL);
bool logout();
int conversation(int num_msg, const struct pam_message **msg, struct pam_response **resp, void *appdata_ptr);

#endif // PAM_H
