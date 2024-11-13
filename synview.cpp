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
#include "synview.h"
#include "build_model.h"

using namespace std;

synView::synView(QWidget *parent) : QTreeView(parent)
{
   expandAll();
}

synView::~synView()
{
}

void synView::currentChanged(const QModelIndex &curr, const QModelIndex &/* old*/ )
{
   emit rowChg(curr);
}

QModelIndex synView::indexFromType(int type)
{
   QModelIndex idx;
   int row, num_rows, childRow, rowType;
   synItem *parentItem, *childItem;
   synModel *bModel = dynamic_cast<synModel*>(model());
   if (!bModel)
      return idx;
   num_rows = bModel->rowCount();
   for (row = 0; row < num_rows; ++row) 
   {
      idx = bModel->sibling(row,SYN_NUM,QModelIndex());
      if (idx.isValid())
      {
         rowType = (bModel->data(idx)).toInt();
         if (rowType == type) // parent row, normal type
            return idx;
         parentItem = bModel->getItem(idx);
         if (parentItem)
         {
            for (childRow = 0; childRow < parentItem->childCount(); ++childRow)
            {
               childItem = parentItem->child(childRow);
               if (childItem)
               {
                  rowType = childItem->synId();
                  if (rowType == type) // child row, pre, post, maybe other
                  {
                     idx = bModel->index(childRow, 0, idx);
                     return idx;
                  }
               }
            }
         }
      }
   }
   return idx;
}
