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


#ifndef SELECTAXONSYN_H
#define SELECTAXONSYN_H

#include <QDialog>
#include <QTableWidgetItem>
#include <vector>

namespace Ui {
   class selectAxonSyn;
   }

class Connector;
struct connRec
{
  QString synType;
  QString axonPop;
  QString axonComment;
  QString synPop;
  QString synComment;
  QString Comment;
  QString source_num;
  QString target_num;
  QString mode;
  Connector *lineitem=nullptr;
};

enum CONN_REC_COL {aPop,aComment,sType,sPop,sComment,pSrc,pTarg,pMode,connIdx};

class selectAxonSyn : public QDialog
{
      Q_OBJECT

   public:
      explicit selectAxonSyn(QWidget *parent = 0);
      ~selectAxonSyn();
      void addRec(connRec&);
      void newList();
      void sortBy(int);
      void enableSort(bool);
      connRec getRec();

   private slots:
      void on_selAxonSynCancel_clicked();
      void on_connList_cellActivated(int row, int column);
      void on_connList_cellClicked(int row, int column);

   private:
      Ui::selectAxonSyn *ui;
      std::vector <Connector *> linelist;
      connRec retRec;
};

class QCustTableWidgetItem : public QTableWidgetItem
{
   public:
     QCustTableWidgetItem(const QString& text, int type=QTableWidgetItem::Type);
     virtual ~QCustTableWidgetItem();

   protected:
     virtual bool operator < (const QTableWidgetItem&) const override;
};


#endif // SELECTAXONSYN_H
