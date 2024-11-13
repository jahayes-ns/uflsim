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
#include <QDropEvent>
#include <QHeaderView>
#include "affmodel.h"
#include "inode.h"

using namespace std;


affModel::affModel(QObject *parent) : QAbstractTableModel(parent)
{
}

affModel::~affModel()
{
}

int affModel::rowCount(const QModelIndex & /*parent*/) const
{
   return affData.size();
}

int affModel::columnCount(const QModelIndex & /*parent*/) const
{
    return AFF_NUM_COL;
}

void affModel::addRow(affRec& row_data, int row)
{
   affRowsIter iter;

   if (row == -1)   // append at end?
   {
      iter = affData.end();
      row = affData.size();
   }
   else
   {
      iter = affData.begin();
      advance(iter, row);
   }

   beginInsertRows(QModelIndex(), row, row);
   affData.insert(iter,row_data);
   endInsertRows();
   emit notifyChg(true);
}

void affModel::reset()
{
   beginResetModel();
   affData.clear();
   endResetModel();
}

void affModel::delRow(int row)
{
   removeRows(row,1);
}


// needed this for drag and drop, but removed dnd, kept this
bool affModel::removeRows(int row, int count, const QModelIndex &/*parent*/)
{
   affRowsIter iter;
   iter = affData.begin();
   advance(iter, row);

   beginRemoveRows(QModelIndex(), row, row + count - 1);
   while (count--)
      iter = affData.erase(iter);
   endRemoveRows();
   emit notifyChg(true);
   return true;
}

// access to underlying data struct
bool affModel::readRec(affRec& aRec, int row)
{
   bool found = false;
   int size = affData.size();
   if (row >= 0 && row < size)
   {
      affRowsIter iter;
      iter = affData.begin();
      advance(iter, row);
      aRec = *iter;
      found = true;
   }
   return found;
}

// access to underlying data struct
bool affModel::updateRec(affRec& bRec, int row)
{
   bool found = false;
   int size = affData.size();
   if (row >= 0 && row < size)
   {
      affRowsIter iter;
      iter = affData.begin();
      advance(iter, row);
      *iter = bRec;
      found = true;
      emit notifyChg(true);
      QModelIndex idxl = createIndex(row,AFF_VAL,this);
      QModelIndex idxr = createIndex(row,AFF_PROB,this);
      dataChanged(idxl,idxr);
   }
   return found;
}

bool affModel::compareRows(affRec& row1, affRec& row2)
{
   double val1, val2;  // numerical compares, not text
   val1 = row1.rec[AFF_VAL].toDouble();
   val2 = row2.rec[AFF_VAL].toDouble();

   if (val1 < val2)
      return true;
   else
      return false;
}

void affModel::sort(int /*column*/, Qt::SortOrder /*order*/)
{
   layoutAboutToBeChanged();
   affData.sort(compareRows);
   layoutChanged(QList<QPersistentModelIndex>(),QAbstractItemModel::VerticalSortHint);
}


QVariant affModel::data(const QModelIndex &index, int role) const
{
    QVariant ret;
    int row, col;
    row = index.row();
    col = index.column();

    switch (role)
    {
       case Qt::DisplayRole:
       case Qt::EditRole:
          if (affData.size())
          {
             auto iter = affData.begin();
             advance(iter,row);
             affRec value = *iter;
             ret = value.rec[col];
          }
          break;

       case Qt::TextAlignmentRole:
          ret = Qt::AlignHCenter + Qt::AlignRight;
       break;

       case Qt::ToolTipRole:
          switch (col)
          {
            case AFF_VAL:
               ret = QString("<html><head/><body><p>Start of afferent value range. At least two values are required to specify a range.</p></body></html>");
               break;
            case AFF_PROB:
               ret = QString("<html><head/><p>Firing Probability for this range.\nThe probabilities for each value in a range are linearly interpolated, with an optional scaling value applied.</p></body></html>");
               break;
            default:
               break;
         }
      default:
         break;
   }
   return ret;
}

bool affModel::setData(const QModelIndex & index, const QVariant & value, int role)
{
   if (role == Qt::EditRole)
   {
      if (affData.size())
      {
         int row = index.row();
         int col = index.column();
         auto iter = affData.begin();
         advance(iter,row);
         (*iter).rec[col]=value.toString();
         dataChanged(index,index);
         emit notifyChg(true);
      }
   }
   return true;
}

Qt::ItemFlags affModel::flags(const QModelIndex& index) const
{
   Qt::ItemFlags flagval = Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
   if (index.isValid())
      flagval |= Qt::ItemIsDragEnabled;
   else
      flagval |= Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
   return flagval;
}


QVariant affModel::headerData(int section,Qt::Orientation orientation, int role) const
{
   if (role == Qt::DisplayRole)
   {
      if (orientation == Qt::Horizontal)
      {
         switch (section)
         {
            case AFF_VAL:
               return QString("Afferent Value");
               break;
            case AFF_PROB:
               return QString("Firing Probability");
               break;
         }
      }
      else if (orientation == Qt::Vertical)
      {
         return section+1;
      }
   }
   return QVariant();
}



// editors for afferent table
QWidget* affSrcDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &/*not used*/, const QModelIndex &/*nothing */) const
{
   QLineEdit *lineEdit = new QLineEdit(parent);
   lineEdit->setAlignment(Qt::AlignRight);
   QDoubleValidator *validator  = new QDoubleValidator(parent);
   validator->setDecimals(4);
   validator->setNotation(QDoubleValidator::StandardNotation);
   lineEdit->setValidator(validator);
   return lineEdit;
}

QWidget* affProbDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &/*not used*/, const QModelIndex &/*nothing */) const
{
   QLineEdit *lineEdit = new QLineEdit(parent);
   lineEdit->setAlignment(Qt::AlignRight);
   QDoubleValidator *validator  = new QDoubleValidator(0.0, 1.0, 4, parent);
   validator->setNotation(QDoubleValidator::StandardNotation);
   lineEdit->setValidator(validator);
   return lineEdit;
}

