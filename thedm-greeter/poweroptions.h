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
#ifndef POWEROPTIONS_H
#define POWEROPTIONS_H

#include <QWidget>

namespace Ui {
    class PowerOptions;
}

class PowerOptions : public QWidget
{
        Q_OBJECT

    public:
        explicit PowerOptions(QWidget *parent = nullptr);
        ~PowerOptions();

    signals:
        void dismiss();
        void showCover();

    private slots:
        void on_cancelButton_clicked();

        void on_hibernateButton_clicked();

        void on_turnOffScreenButton_clicked();

        void on_suspendButton_clicked();

        void on_rebootButton_clicked();

        void on_powerOffButton_clicked();

    private:
        Ui::PowerOptions *ui;
};

#endif // POWEROPTIONS_H
