/****************************************
 *
 *   theShell - Desktop Environment
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

#include "nativeeventfilter.h"
#include <QProcess>
#include <QTimer>

NativeEventFilter::NativeEventFilter(QObject* parent) : QObject(parent)
{
    powerButtonTimer = new QTimer();
    powerButtonTimer->setInterval(500);
    powerButtonTimer->setSingleShot(true);
    connect(powerButtonTimer, SIGNAL(timeout()), this, SLOT(handlePowerButton()));

    //Capture required keys
    XGrabKey(QX11Info::display(), XKeysymToKeycode(QX11Info::display(), XF86XK_MonBrightnessUp), AnyModifier, RootWindow(QX11Info::display(), 0), true, GrabModeAsync, GrabModeAsync);
    XGrabKey(QX11Info::display(), XKeysymToKeycode(QX11Info::display(), XF86XK_MonBrightnessDown), AnyModifier, RootWindow(QX11Info::display(), 0), true, GrabModeAsync, GrabModeAsync);
    XGrabKey(QX11Info::display(), XKeysymToKeycode(QX11Info::display(), XF86XK_KbdBrightnessUp), AnyModifier, RootWindow(QX11Info::display(), 0), true, GrabModeAsync, GrabModeAsync);
    XGrabKey(QX11Info::display(), XKeysymToKeycode(QX11Info::display(), XF86XK_KbdBrightnessDown), AnyModifier, RootWindow(QX11Info::display(), 0), true, GrabModeAsync, GrabModeAsync);
    XGrabKey(QX11Info::display(), XKeysymToKeycode(QX11Info::display(), XF86XK_AudioLowerVolume), AnyModifier, RootWindow(QX11Info::display(), 0), true, GrabModeAsync, GrabModeAsync);
    XGrabKey(QX11Info::display(), XKeysymToKeycode(QX11Info::display(), XF86XK_AudioRaiseVolume), AnyModifier, RootWindow(QX11Info::display(), 0), true, GrabModeAsync, GrabModeAsync);
    XGrabKey(QX11Info::display(), XKeysymToKeycode(QX11Info::display(), XF86XK_AudioMute), AnyModifier, RootWindow(QX11Info::display(), 0), true, GrabModeAsync, GrabModeAsync);
    XGrabKey(QX11Info::display(), XKeysymToKeycode(QX11Info::display(), XF86XK_Eject), AnyModifier, RootWindow(QX11Info::display(), 0), true, GrabModeAsync, GrabModeAsync);
    XGrabKey(QX11Info::display(), XKeysymToKeycode(QX11Info::display(), XF86XK_PowerOff), AnyModifier, RootWindow(QX11Info::display(), 0), true, GrabModeAsync, GrabModeAsync);
    XGrabKey(QX11Info::display(), XKeysymToKeycode(QX11Info::display(), XF86XK_Sleep), AnyModifier, RootWindow(QX11Info::display(), 0), true, GrabModeAsync, GrabModeAsync);
    XGrabKey(QX11Info::display(), XKeysymToKeycode(QX11Info::display(), XK_Print), AnyModifier, RootWindow(QX11Info::display(), 0), true, GrabModeAsync, GrabModeAsync);
    XGrabKey(QX11Info::display(), XKeysymToKeycode(QX11Info::display(), XK_Delete), ControlMask | Mod1Mask, RootWindow(QX11Info::display(), 0), true, GrabModeAsync, GrabModeAsync);

    //Inhibit logind's handling of some power events
    QDBusMessage message = QDBusMessage::createMethodCall("org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.login1.Manager", "Inhibit");
    message.setArguments(QList<QVariant>() << "handle-power-key:handle-suspend-key:handle-lid-switch" << "theDM" << "theDM Handles Hardware Power Keys" << "block");
    QDBusReply<QDBusUnixFileDescriptor> inhibitReply = QDBusConnection::systemBus().call(message);
    powerInhibit = inhibitReply.value();

    //Start the Last Pressed timer to ignore repeated keys
    lastPress.start();
}

NativeEventFilter::~NativeEventFilter() {
    XUngrabKey(QX11Info::display(), XKeysymToKeycode(QX11Info::display(), XF86XK_MonBrightnessUp), AnyModifier, QX11Info::appRootWindow());
    XUngrabKey(QX11Info::display(), XKeysymToKeycode(QX11Info::display(), XF86XK_MonBrightnessDown), AnyModifier, QX11Info::appRootWindow());
    XUngrabKey(QX11Info::display(), XKeysymToKeycode(QX11Info::display(), XF86XK_KbdBrightnessUp), AnyModifier, QX11Info::appRootWindow());
    XUngrabKey(QX11Info::display(), XKeysymToKeycode(QX11Info::display(), XF86XK_KbdBrightnessDown), AnyModifier, QX11Info::appRootWindow());
    XUngrabKey(QX11Info::display(), XKeysymToKeycode(QX11Info::display(), XF86XK_AudioLowerVolume), AnyModifier, QX11Info::appRootWindow());
    XUngrabKey(QX11Info::display(), XKeysymToKeycode(QX11Info::display(), XF86XK_AudioRaiseVolume), AnyModifier, QX11Info::appRootWindow());
    XUngrabKey(QX11Info::display(), XKeysymToKeycode(QX11Info::display(), XF86XK_AudioMute), AnyModifier, QX11Info::appRootWindow());
    XUngrabKey(QX11Info::display(), XKeysymToKeycode(QX11Info::display(), XF86XK_Eject), AnyModifier, QX11Info::appRootWindow());
    XUngrabKey(QX11Info::display(), XKeysymToKeycode(QX11Info::display(), XF86XK_PowerOff), AnyModifier, QX11Info::appRootWindow());
    XUngrabKey(QX11Info::display(), XKeysymToKeycode(QX11Info::display(), XF86XK_Sleep), AnyModifier, QX11Info::appRootWindow());
    XUngrabKey(QX11Info::display(), XKeysymToKeycode(QX11Info::display(), XK_Print), AnyModifier, QX11Info::appRootWindow());
    XUngrabKey(QX11Info::display(), XKeysymToKeycode(QX11Info::display(), XK_Delete), ControlMask | Mod1Mask, QX11Info::appRootWindow());
}


bool NativeEventFilter::nativeEventFilter(const QByteArray &eventType, void *message, long *result) {
    Q_UNUSED(result)

    if (eventType == "xcb_generic_event_t") {
        xcb_generic_event_t* event = static_cast<xcb_generic_event_t*>(message);
        if (event->response_type == XCB_KEY_PRESS) { //Key Press Event
            if (lastPress.restart() > 100) {
                xcb_key_release_event_t* button = static_cast<xcb_key_release_event_t*>(message);

                //Get Current Brightness
                QProcess* backlight = new QProcess(this);
                backlight->start("xbacklight -get");
                backlight->waitForFinished();
                float currentBrightness = ceil(QString(backlight->readAll()).toFloat());
                delete backlight;

                //Get Current Volume
                //int volume = AudioMan->MasterVolume();

                int kbdBrightness = -1, maxKbdBrightness = -1;
                QDBusInterface keyboardInterface("org.freedesktop.UPower", "/org/freedesktop/UPower/KbdBacklight", "org.freedesktop.UPower.KbdBacklight", QDBusConnection::systemBus());
                if (keyboardInterface.isValid()) {
                    kbdBrightness = keyboardInterface.call("GetBrightness").arguments().first().toInt();
                    maxKbdBrightness = keyboardInterface.call("GetMaxBrightness").arguments().first().toInt();
                }

                if (button->detail == XKeysymToKeycode(QX11Info::display(), XF86XK_MonBrightnessUp)) { //Increase brightness by 10%
                    currentBrightness = currentBrightness + 10;
                    if (currentBrightness > 100) currentBrightness = 100;

                    QProcess* backlightAdj = new QProcess(this);
                    backlightAdj->start("xbacklight -set " + QString::number(currentBrightness));
                    connect(backlightAdj, SIGNAL(finished(int)), backlightAdj, SLOT(deleteLater()));

                    //Hotkeys->show(QIcon::fromTheme("video-display"), tr("Brightness"), (int) currentBrightness);
                } else if (button->detail == XKeysymToKeycode(QX11Info::display(), XF86XK_MonBrightnessDown)) { //Decrease brightness by 10%
                    currentBrightness = currentBrightness - 10;
                    if (currentBrightness < 0) currentBrightness = 0;

                    QProcess* backlightAdj = new QProcess(this);
                    backlightAdj->start("xbacklight -set " + QString::number(currentBrightness));
                    connect(backlightAdj, SIGNAL(finished(int)), backlightAdj, SLOT(deleteLater()));

                    //Hotkeys->show(QIcon::fromTheme("video-display"), tr("Brightness"), (int) currentBrightness);
                } /*else if (button->detail == XKeysymToKeycode(QX11Info::display(), XF86XK_AudioRaiseVolume)) {
                    if (button->state & Mod4Mask) {
                        //Increase brightness
                        currentBrightness = currentBrightness + 10;
                        if (currentBrightness > 100) currentBrightness = 100;

                        QProcess* backlightAdj = new QProcess(this);
                        backlightAdj->start("xbacklight -set " + QString::number(currentBrightness));
                        connect(backlightAdj, SIGNAL(finished(int)), backlightAdj, SLOT(deleteLater()));

                        Hotkeys->show(QIcon::fromTheme("video-display"), tr("Brightness"), (int) currentBrightness);
                    } else {
                        //Increase volume
                        if (AudioMan->QuietMode() == AudioManager::mute) {
                            Hotkeys->show(QIcon::fromTheme("audio-volume-muted"), tr("Volume"), tr("Quiet Mode is set to Mute."));
                        } else {
                                volume = volume + 5;
                                if (volume - 5 < 100 && volume > 100) {
                                    volume = 100;
                                }
                                AudioMan->changeVolume(5);

                                //Check if the user has feedback sound on
                                if (settings.value("sound/feedbackSound", true).toBool()) {
                                    QSoundEffect* volumeSound = new QSoundEffect();
                                    volumeSound->setSource(QUrl("qrc:/sounds/volfeedback.wav"));
                                    volumeSound->play();
                                    connect(volumeSound, SIGNAL(playingChanged()), volumeSound, SLOT(deleteLater()));
                                }

                                Hotkeys->show(QIcon::fromTheme("audio-volume-high"), tr("Volume"), volume);
                        }
                    }
                } else if (button->detail == XKeysymToKeycode(QX11Info::display(), XF86XK_AudioLowerVolume)) { //Decrease Volume by 5%
                    if (button->state & Mod4Mask) {
                        //Decrease brightness
                        currentBrightness = currentBrightness - 10;
                        if (currentBrightness < 0) currentBrightness = 0;

                        QProcess* backlightAdj = new QProcess(this);
                        backlightAdj->start("xbacklight -set " + QString::number(currentBrightness));
                        connect(backlightAdj, SIGNAL(finished(int)), backlightAdj, SLOT(deleteLater()));

                        Hotkeys->show(QIcon::fromTheme("video-display"), tr("Brightness"), (int) currentBrightness);
                    } else if (powerPressed) {
                        //Take a screenshot
                        screenshotWindow* screenshot = new screenshotWindow;
                        screenshot->show();
                        powerButtonTimer->stop();
                    } else {
                        if (AudioMan->QuietMode() == AudioManager::mute) {
                            Hotkeys->show(QIcon::fromTheme("audio-volume-muted"), tr("Volume"), tr("Quiet Mode is set to Mute."));
                        } else {
                            volume = volume - 5;
                            if (volume < 0) volume = 0;
                            AudioMan->changeVolume(-5);

                            //Check if the user has feedback sound on
                            if (settings.value("sound/feedbackSound", true).toBool()) {
                            QSoundEffect* volumeSound = new QSoundEffect();
                            volumeSound->setSource(QUrl("qrc:/sounds/volfeedback.wav"));
                            volumeSound->play();
                            connect(volumeSound, SIGNAL(playingChanged()), volumeSound, SLOT(deleteLater()));
                            }

                            Hotkeys->show(QIcon::fromTheme("audio-volume-high"), tr("Volume"), volume);
                        }
                    }
                } else if (button->detail == XKeysymToKeycode(QX11Info::display(), XF86XK_AudioMute)) { //Toggle Quiet Mode
                    switch (AudioMan->QuietMode()) {
                        case AudioManager::none:
                            AudioMan->setQuietMode(AudioManager::critical);
                            Hotkeys->show(QIcon::fromTheme("quiet-mode-critical-only"), tr("Critical Only"), AudioMan->getCurrentQuietModeDescription(), 5000);
                            break;
                        case AudioManager::critical:
                            AudioMan->setQuietMode(AudioManager::notifications);
                            Hotkeys->show(QIcon::fromTheme("quiet-mode"), tr("No Notifications"), AudioMan->getCurrentQuietModeDescription(), 5000);
                            break;
                        case AudioManager::notifications:
                            AudioMan->setQuietMode(AudioManager::mute);
                            Hotkeys->show(QIcon::fromTheme("audio-volume-muted"), tr("Mute"), AudioMan->getCurrentQuietModeDescription(), 5000);
                            break;
                        case AudioManager::mute:
                            AudioMan->setQuietMode(AudioManager::none);
                            Hotkeys->show(QIcon::fromTheme("audio-volume-high"), tr("Sound"), AudioMan->getCurrentQuietModeDescription(), 5000);
                            break;
                    }
                }*/ else if (button->detail == XKeysymToKeycode(QX11Info::display(), XF86XK_KbdBrightnessUp)) { //Increase keyboard brightness by 5%
                    kbdBrightness += (((float) maxKbdBrightness / 100) * 5);
                    if (kbdBrightness > maxKbdBrightness) kbdBrightness = maxKbdBrightness;
                    keyboardInterface.call("SetBrightness", kbdBrightness);

                    //Hotkeys->show(QIcon::fromTheme("keyboard-brightness"), tr("Keyboard Brightness"), ((float) kbdBrightness / (float) maxKbdBrightness) * 100);
                } else if (button->detail == XKeysymToKeycode(QX11Info::display(), XF86XK_KbdBrightnessDown)) { //Decrease keyboard brightness by 5%
                    kbdBrightness -= (((float) maxKbdBrightness / 100) * 5);
                    if (kbdBrightness < 0) kbdBrightness = 0;
                    keyboardInterface.call("SetBrightness", kbdBrightness);

                    //Hotkeys->show(QIcon::fromTheme("keyboard-brightness"), tr("Keyboard Brightness"), ((float) kbdBrightness / (float) maxKbdBrightness) * 100);
                } else if (button->detail == XKeysymToKeycode(QX11Info::display(), XF86XK_PowerOff)) {
                    powerPressed = true;
                    if (!powerButtonTimer->isActive()) {
                        powerButtonTimer->start();
                    }
                }
            }
        } else if (event->response_type == XCB_KEY_RELEASE) {
            xcb_key_release_event_t* button = static_cast<xcb_key_release_event_t*>(message);

            if (button->detail == XKeysymToKeycode(QX11Info::display(), XF86XK_Eject)) { //Eject Disc
                QProcess* eject = new QProcess(this);
                eject->start("eject");
                connect(eject, SIGNAL(finished(int)), eject, SLOT(deleteLater()));

                //Hotkeys->show(QIcon::fromTheme("media-eject"), tr("Eject"), tr("Attempting to eject disc..."));
            } else if (button->detail == XKeysymToKeycode(QX11Info::display(), XF86XK_PowerOff)) { //Power Off
                powerPressed = false;
                if (powerButtonTimer->isActive()) {
                    powerButtonTimer->stop();
                    handlePowerButton();
                }
            } else if (button->detail == XKeysymToKeycode(QX11Info::display(), XK_Delete) && (button->state == (ControlMask | Mod1Mask))) {
                if (!isEndSessionBoxShowing) {
                    handlePowerButton();
                }
            } else if (button->detail == XKeysymToKeycode(QX11Info::display(), XF86XK_Sleep)) { //Suspend
                QList<QVariant> arguments;
                arguments.append(true);

                QDBusMessage message = QDBusMessage::createMethodCall("org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.login1.Manager", "Suspend");
                message.setArguments(arguments);
                QDBusConnection::systemBus().send(message);
            } else if (button->detail == XKeysymToKeycode(QX11Info::display(), XK_Return) && (button->state == Mod4Mask)) {
                /*QString newKeyboardLayout = MainWin->getInfoPane()->setNextKeyboardLayout();
                Hotkeys->show(QIcon::fromTheme("input-keyboard"), tr("Keyboard Layout"), tr("Keyboard Layout set to %1").arg(newKeyboardLayout), 5000);
                ignoreSuper = true;*/
            }
        }
    }
    return false;
}

void NativeEventFilter::handlePowerButton() {
    emit PowerPressed();
}
