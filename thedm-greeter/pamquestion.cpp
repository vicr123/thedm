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
#include "pamquestion.h"
#include "ui_pamquestion.h"

PamQuestion::PamQuestion(bool echo, QString question, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PamQuestion)
{
    ui->setupUi(this);

    ui->pamMessage->setText(question);
    if (echo) {
        ui->responseBox->setEchoMode(QLineEdit::Normal);
    } else {
        ui->responseBox->setEchoMode(QLineEdit::Password);
    }
}

PamQuestion::~PamQuestion()
{
    delete ui;
}

void PamQuestion::on_cancelButton_clicked()
{
    emit dismiss();
}

void PamQuestion::on_okayButton_clicked()
{
    emit respond(ui->responseBox->text());
    emit dismiss();
}

void PamQuestion::setTitle(QString text) {
    ui->titleLabel->setText(text);
}

void PamQuestion::setPlaceholder(QString placeholder) {
    ui->responseBox->setPlaceholderText(placeholder);
}
