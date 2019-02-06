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
#ifndef PAMQUESTION_H
#define PAMQUESTION_H

#include <QWidget>

namespace Ui {
    class PamQuestion;
}

class PamQuestion : public QWidget
{
        Q_OBJECT

    public:
        explicit PamQuestion(bool echo, QString question, QWidget *parent = nullptr);
        ~PamQuestion();

    signals:
        void dismiss();
        void respond(QString response);

    private slots:
        void on_cancelButton_clicked();

        void on_okayButton_clicked();

    private:
        Ui::PamQuestion *ui;
};

#endif // PAMQUESTION_H
