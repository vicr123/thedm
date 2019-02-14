#ifndef PAM_H
#define PAM_H

#include <QString>
#include <QProcess>
#include <QDebug>
#include <QX11Info>
#include <security/pam_appl.h>
#include <security/pam_misc.h>
#include <pwd.h>
#include <grp.h>
#include <paths.h>
#include <unistd.h>
#include <sys/mman.h>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusInterface>
#include <QDBusArgument>
#include <functional>

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

typedef std::function<void(QString,bool)> PamInputCallback;
typedef std::function<void(QString,QString,QString,bool)> PamAuthTokCallback;

class PamBackend : public QObject {
    Q_OBJECT

    public:
        PamBackend(QString username, QString sessionName = "thedm", QObject* parent = nullptr);
        ~PamBackend();

        PamInputCallback currentInputCallback();

        enum PamAuthenticationResult {
            AuthSuccess,
            AuthFailure,
            AuthCancelled
        };

        enum PamAccountMgmtResult {
            AccMgmtSuccess,
            AccMgmtFailure,
            AccMgmtExipred,
            AccMgmtNeedRefresh
        };

    public slots:
        PamAuthenticationResult authenticate();
        PamAccountMgmtResult acctMgmt();
        bool setCred();
        bool startSession(QString exec);
        bool changeAuthTok();
        void waitForSessionEnd();
        void putenv(QString env, QString value);
        void setItem(int type, const void* value);
        QString getenv(QString env);
        void endSession();

    signals:
        void inputRequired(bool echo, QString msg, PamInputCallback callback);
        void passwordChangeRequired(bool needOldPassword, PamAuthTokCallback callback);
        void message(QString warning);

    private:
        static int conversation(int num_msg, const struct pam_message **msg, struct pam_response **resp, void *appdata_ptr);
        pam_handle_t* pamHandle;
        struct passwd *pw;
        QString username;
        QString dbusPid;

        int sessionPid;

        bool authenticating = false;
        bool changingTok = false;
        bool sessionOpen = false;

        struct PasswordChanges {
            QString newPassword;
            QString newConfirm;
        };
        PasswordChanges* newPasswords;

        PamInputCallback inputCallback;
};

#endif // PAM_H
