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
#include "pamchauthtok.h"
#include "ui_pamchauthtok.h"

PamChauthtok::PamChauthtok(bool needOldPassword, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PamChauthtok)
{
    ui->setupUi(this);

    ui->currentBox->setVisible(needOldPassword);
}

PamChauthtok::~PamChauthtok()
{
    delete ui;
}

void PamChauthtok::on_okayButton_clicked()
{
    emit respond(ui->currentBox->text(), ui->newPasswordBox->text(), ui->confirmPasswordBox->text());
    emit dismiss();
}

void PamChauthtok::on_cancelButton_clicked()
{
    emit dismiss();
}
