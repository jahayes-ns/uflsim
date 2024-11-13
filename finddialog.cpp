/*(Copyright 2005-2020 Kendall F. Morris

This file is part of the USF Neural Simulator suite.

    The Neural Simulator suite is free software: you can redistribute
    it and/or modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation, either
    version 3 of the License, or (at your option) any later version.

    The suite is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with the suite.  If not, see <https://www.gnu.org/licenses/>.
*/


#include <iostream>
#include "finddialog.h"
#include "ui_finddialog.h"

using namespace std;

findDialog::findDialog(QWidget *parent) :
   QDialog(parent),
   ui(new Ui::findDialog)
{
   ui->setupUi(this);
}

findDialog::~findDialog()
{
   delete ui;
}

void findDialog::initList(pickList& c_list, pickList& f_list)
{
   ui->findList->clear();
   for (pickListIter iter = c_list.begin(); iter != c_list.end(); ++iter)
      ui->findList->addItem(iter->second);
   for (pickListIter iter = f_list.begin(); iter != f_list.end(); ++iter)
      ui->findList->addItem(iter->second);
   if (ui->findList->count())
      ui->findButton->setEnabled(true);
   else
      ui->findButton->setEnabled(false);
}

void findDialog::on_findButton_clicked()
{
   auto sel = ui->findList->currentItem();
   if (sel)
      emit findThisOne(*sel);
}

void findDialog::on_closeButton_clicked()
{
   hide();
}
