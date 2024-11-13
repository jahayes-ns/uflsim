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
#include "selectaxonsyn.h"
#include "ui_selectaxonsyn.h"

using namespace std;

selectAxonSyn::selectAxonSyn(QWidget *parent) : QDialog(parent), ui(new Ui::selectAxonSyn)
{
   ui->setupUi(this);
   ui->connList->hideColumn(pSrc);
   ui->connList->hideColumn(pTarg);
   ui->connList->hideColumn(pMode);
   ui->connList->hideColumn(connIdx);
}

selectAxonSyn::~selectAxonSyn()
{
   delete ui;
}

void selectAxonSyn::addRec(connRec& row)
{
   int new_row = ui->connList->rowCount();
   ui->connList->insertRow(new_row);
   ui->connList->setItem(new_row,sType,new QCustTableWidgetItem(row.synType));
   ui->connList->setItem(new_row,aPop,new QCustTableWidgetItem(row.axonPop));
   ui->connList->setItem(new_row,aComment,new QCustTableWidgetItem(row.axonComment));
   ui->connList->setItem(new_row,sPop,new QCustTableWidgetItem(row.synPop));
   ui->connList->setItem(new_row,sComment,new QCustTableWidgetItem(row.synComment));
   ui->connList->setItem(new_row,pSrc,new QCustTableWidgetItem(row.source_num));
   ui->connList->setItem(new_row,pTarg,new QCustTableWidgetItem(row.target_num));
   ui->connList->setItem(new_row,pMode,new QCustTableWidgetItem(row.mode));
   ui->connList->setItem(new_row,connIdx,new QCustTableWidgetItem(QString("%1").arg(new_row)));
   linelist.push_back(row.lineitem);
}

void selectAxonSyn::on_selAxonSynCancel_clicked()
{
   retRec={};
   reject();
}

// Turn off sorting when adding rows, it messes up the row order
void selectAxonSyn::enableSort(bool onoff)
{
   ui->connList->setSortingEnabled(onoff);
}

void selectAxonSyn::newList()
{
   ui->connList->setRowCount(0);
   linelist.clear();
   retRec={};
}

void selectAxonSyn::sortBy(int col)
{
   ui->connList->sortItems(col);
}

connRec selectAxonSyn::getRec()
{
   return retRec;
}

void selectAxonSyn::on_connList_cellActivated(int row, int /*column*/)
{
   retRec.synType    = ui->connList->item(row,sType)->text();
   retRec.axonPop    = ui->connList->item(row,aPop)->text();
   retRec.synPop     = ui->connList->item(row,sPop)->text();
   retRec.axonComment= ui->connList->item(row,aComment)->text();
   retRec.synComment = ui->connList->item(row,sComment)->text();
   retRec.source_num = ui->connList->item(row,pSrc)->text();
   retRec.target_num = ui->connList->item(row,pTarg)->text();
   retRec.mode       = ui->connList->item(row,pMode)->text();
   int connitem       = ui->connList->item(row,connIdx)->text().toInt();
   retRec.lineitem = linelist[connitem];
   accept();
}

void selectAxonSyn::on_connList_cellClicked(int row, int column)
{
   on_connList_cellActivated(row,column);
}

QCustTableWidgetItem::QCustTableWidgetItem(const QString& text, int type) : QTableWidgetItem(text,type)
{
//  cout << "add " << this->text().toLatin1().data() << endl;
}

QCustTableWidgetItem::~QCustTableWidgetItem()
{
//  cout << "remove " << text().toLatin1().data() << endl;
}

// sorting criteria
bool QCustTableWidgetItem::operator < (const QTableWidgetItem& other) const
{
   int us;
   int them;

   switch (this->column())
   {
      case sType:
      default:
         us = this->text() >= other.text(); // just text
         them = this->text() < other.text();
         break;

      case aPop:
      case sPop:
         us = (this->text().section(' ',2)).toInt();  // use pop num for numerical sorting,
         them = (other.text().section(' ',2)).toInt(); // so text 4 is not > text 21 
         break;
   }
   return us < them;
}
