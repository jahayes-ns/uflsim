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

#ifndef FINDDIALOG_H
#define FINDDIALOG_H

#include <QDialog>
#include <QString>
#include <map>
#include <QListWidgetItem>

using pickList = std::map<int, QListWidgetItem*>;
using pickListIter = pickList::iterator;

namespace Ui {
   class findDialog;
   }

class findDialog : public QDialog
{
      Q_OBJECT

   public:
      explicit findDialog(QWidget *parent = nullptr);
      ~findDialog();
      void initList(pickList& c_list, pickList& f_list);

   private slots:
      void on_findButton_clicked();
      void on_closeButton_clicked();

   signals:
      void findThisOne(QListWidgetItem&);

   private:
      Ui::findDialog *ui;
};

#endif // FINDDIALOG_H
