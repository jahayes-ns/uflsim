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


// A collection of classes to manage the qtableviews in the launch child window

#include <iostream>
#include <QValidator>
#include <QLabel>
#include <QCheckBox>
#include "launch_model.h"
#include <string>
#include <typeinfo>
#include "c_globals.h"

using namespace std;

plotModel::plotModel(QObject *parent) : QAbstractTableModel(parent)
{
   plotData.resize(MAX_LAUNCH);
}

plotModel::~plotModel()
{
}

int plotModel::rowCount(const QModelIndex & /*parent*/) const
{
   return plotData[currModel].size();
}

int plotModel::columnCount(const QModelIndex & /*parent*/) const
{
    return PLOT_NUM_COL;
}

void plotModel::dupModel(int from, int to)
{
   plotData[to] = plotData[from];
   emit notifyChg(true);
}

// add new rec at row
void plotModel::addRow(plotRec& row_data, int row)
{
   plotRowIter iter;

   if (row == -1)   // append at end?
   {
      iter = plotData[currModel].end();
      row = plotData[currModel].size();
   }
   else
   {
      iter = plotData[currModel].begin();
      advance(iter, row);
   }
    // will be 0 if already have a member #
   if (row_data.rec[PLOT_MEMB] == "0")
   {
      // Duplicates are okay for plots, you may even want a dup and plot
      // different types of info.
      int rand_memb; 
      rand_memb = rand() % row_data.maxRndVal+1;
      QString rand_str;
      rand_str.setNum(rand_memb);
      row_data.rec[PLOT_MEMB] = rand_str;
   }
   beginInsertRows(QModelIndex(), row, row);
   plotData[currModel].insert(iter,row_data);
   endInsertRows();
   emit notifyChg(true);
}

void plotModel::reset()
{
   beginResetModel();
   for (auto &page : plotData)
   {
      if (page.size())
      {
         beginRemoveRows(QModelIndex(), 0, page.size()-1);
         page.clear();
         endRemoveRows();
      }
   }
   endResetModel();
}

void plotModel::delRow(int row)
{
   removeRows(row,1);
}

// need this for drag and drop and deleting, we insert with our own api
bool plotModel::removeRows(int row, int count, const QModelIndex &/*parent*/)
{
   plotRowIter iter;
   iter = plotData[currModel].begin();
   advance(iter, row);

   beginRemoveRows(QModelIndex(), row, row + count - 1);
   while (count--)
      iter = plotData[currModel].erase(iter);
   endRemoveRows();
   emit notifyChg(true);
   return true;
}

// access to underlying data struct
bool plotModel::readRec(plotRec& sRec, int row)
{
   bool found = false;
   int size = plotData[currModel].size();
   if (row >= 0 && row < size)
   {
      plotRowIter iter;
      iter = plotData[currModel].begin();
      advance(iter, row);
      sRec = *iter;
      found = true;
   }
   return found;
}

// access to underlying data struct
bool plotModel::updateRec(plotRec& sRec, int row)
{
   bool found = false;
   int size = plotData[currModel].size();
   if (row >= 0 && row < size)
   {
      plotRowIter iter;
      iter = plotData[currModel].begin();
      advance(iter, row);
      *iter = sRec;
      found = true;
      emit notifyChg(true);
      QModelIndex idxl = createIndex(row,PLOT_MEMB,this);
      QModelIndex idxr = createIndex(row,PLOT_TYPE,this);
      dataChanged(idxl,idxr);
   }
   return found;
}

bool plotModel::canDropMimeData(const QMimeData *data, Qt::DropAction /*action*/, int /*row*/, int /*column*/, const QModelIndex &/*parent*/) const
{
   QStringList types = mimeTypes();
   QString format = types.at(currModel);
   if (data->hasFormat(format))
      return true;
   else
      return false;
}

bool plotModel::dropMimeData(const QMimeData *data, Qt::DropAction /*action*/, int row, int /*column*/, const QModelIndex &/*parent*/)
{
   const plotDropData* dragRec = dynamic_cast <const plotDropData*>(data);
   if (row == -1)
      row = rowCount();
   if (dragRec)
   {
      QStringList types = mimeTypes();
      QString format = types.at(currModel);
      if (data->hasFormat(format))
      {
         plotRec rec = dragRec->one_rec;
         addRow(rec,row);
         return true;
      }
   }
   return false;
}

QStringList plotModel::mimeTypes() const
{
   QStringList types;
   types << "application/text.list";
   return types;
}

QMimeData* plotModel::mimeData(const QModelIndexList &indexes) const
{
   QStringList types = mimeTypes();
   QString format = types.at(currModel);
   plotDropData *rec = new plotDropData;
   QByteArray encodedData;
   QDataStream stream(&encodedData, QIODevice::WriteOnly);
   plotRowConstIter iter;
   iter = plotData[currModel].cbegin();
   int row = indexes[currModel].row(); // same row for all indexes
   advance(iter, row);         // copy this row's values
   (*rec).one_rec = *iter;
   stream << rec;
   rec->setData(format, encodedData);
   return rec;
}

// composite key sort, order by type, pop, member, plot type
// Sort ascending
// f 1 18 e
// f 1 20 e
static bool plotCompareRowsA(const plotRec& row1, const plotRec& row2)
{
   int r1c0, r2c0, r1c1,r1c2,r2c1,r2c2;  // numerical compares, not text
   r1c0 = row1.rec[PLOT_CELL_FIB].toInt();
   r1c1 = row1.rec[PLOT_POP].toInt();
   r1c2 = row1.rec[PLOT_MEMB].toInt();
   r2c0 = row2.rec[PLOT_CELL_FIB].toInt();
   r2c1 = row2.rec[PLOT_POP].toInt();
   r2c2 = row2.rec[PLOT_MEMB].toInt();
   if (r1c0 < r2c0)
      return true;
   if (r1c0 == r2c0)
   {
      if (r1c1 < r2c1)
         return true;
      if (r1c1 == r2c1)
      {
         if (r1c2 < r2c2)
            return true;
         if (r1c2 == r2c2)
            return row1.rec[PLOT_TYPE] < row2.rec[PLOT_TYPE]; // text comp
      }
   }
   return false;
}

// Sort descending
static bool plotCompareRowsD(const plotRec& row1, const plotRec& row2)
{
   int r1c0,r2c0,r1c1,r1c2,r2c1,r2c2;  // numerical compares, not text
   r1c0 = row1.rec[PLOT_CELL_FIB].toInt();
   r1c1 = row1.rec[PLOT_POP].toInt();
   r1c2 = row1.rec[PLOT_MEMB].toInt();
   r2c0 = row2.rec[PLOT_CELL_FIB].toInt();
   r2c1 = row2.rec[PLOT_POP].toInt();
   r2c2 = row2.rec[PLOT_MEMB].toInt();
   if (r1c0 > r2c0)
      return true;
   if (r1c0 == r2c0)
   {
      if (r1c1 > r2c1)
         return true;
      if (r1c1 == r2c1)
      {
         if (r1c2 > r2c2)
            return true;
         if (r1c2 == r2c2)
            return row1.rec[PLOT_TYPE] > row2.rec[PLOT_TYPE]; // text comp
      }
   }
   return false;
}


void plotModel::sort(int /*column*/, Qt::SortOrder order)
{
   layoutAboutToBeChanged();
   if (order == Qt::AscendingOrder)
      plotData[currModel].sort(plotCompareRowsA);
   else
      plotData[currModel].sort(plotCompareRowsD);
   layoutChanged(QList<QPersistentModelIndex>(),QAbstractItemModel::VerticalSortHint);
   emit notifyChg(true);
}

QVariant plotModel::data(const QModelIndex &index, int role) const
{
    QVariant ret;
    int row, col, idx = 0;
    row = index.row();
    col = index.column();
    
    switch (role)
    {
       case Qt::DisplayRole:
       case Qt::EditRole:
          if (plotData[currModel].size())
          {
             auto iter = plotData[currModel].begin();
             advance(iter,row);
             plotRec value = *iter;
             if (col == PLOT_CELL_FIB)
             {
                if (value.rec[col] == STD_CELL)
                   ret = "CELL";
                else if (value.rec[col] == STD_FIBER)
                {
                   if ((*iter).popType == STD_FIBER)
                      ret = "FIBER";
                   else
                      ret = "AFF FIBER";
                }
                else if (value.rec[col] == LUNG_CELL)
                   ret = "LUNG";
             }
             else if (col == PLOT_TYPE)  // use index # to pick item in combobox
             {
                int index = value.rec[PLOT_TYPE].toInt();
                switch ((*iter).popType)
                {
                   case BURSTER_CELL:
                      ret = bursterCombo[index];
                      break;
                   case LUNG_CELL:
                      idx = -(index+1);
                      ret = lungCombo[idx];
                      break;
                   case STD_CELL:
                   case PSR_CELL:
                      ret = stdCombo[index];
                      break;
                   case STD_FIBER:
                      ret = fiberPlotType;
                      break;
                   case AFFERENT_EVENT:
                      if (index > AFFERENT_EVENT || index < AFFERENT_BIN)
                         index = AFFERENT_EVENT;
                      ret = afferentCombo[-(index-AFFERENT_EVENT)];
                      break;
                   default:
                      cout << "Unexpected case #1 in plot table" << endl;
                      ret = stdCombo[index];
                      break;
                }
             }
             else
                ret = value.rec[col];
          }
          break;

       case Qt::TextAlignmentRole:
          switch(col)
          {
             case PLOT_CELL_FIB:
                ret = Qt::AlignLeft + Qt::AlignVCenter;
                break;
             case PLOT_TYPE:
                ret = Qt::AlignLeft + Qt::AlignVCenter;
                break;
             case PLOT_BINWID:
             case PLOT_POP:
             case PLOT_SCALE:
             case PLOT_MEMB:
             default:
                ret = Qt::AlignRight + Qt::AlignVCenter;
                break;
          }
          break;

       case Qt::ToolTipRole:
          switch (col)
          {
             case PLOT_TYPE:
                ret = QString("Select the first four entries\nor select the fifth entry to use\nthe bin width and scale values. This is the activity for the entire population, not a single member.");
                break;
             default:
                break;
          }
          break;
      default:
         break;
    }
    return ret;
}

bool plotModel::setData(const QModelIndex & index, const QVariant & value, int role)
{
   if (role == Qt::EditRole)
   {
      if (plotData[currModel].size())
      {
         int row = index.row();
         int col = index.column();
         auto iter = plotData[currModel].begin();
         advance(iter,row);
         QVariant newval;
         if (col == PLOT_TYPE)  // use text to find index
         {
            int idx;
            switch ((*iter).popType)
            {
               case STD_CELL:
               case PSR_CELL:
                  newval = stdCombo.indexOf(QRegExp(value.toString()));
                  break;
               case BURSTER_CELL:
                  newval = bursterCombo.indexOf(QRegExp(value.toString()));
                  break;
               case LUNG_CELL:
                  idx = lungCombo.indexOf(QRegExp(value.toString()));
                  idx = -(idx+1);
                  newval = idx;
                  break;
               case STD_FIBER:
                  newval = STD_FIBER;
                  break;
               case AFFERENT_EVENT:
                  idx = afferentCombo.indexOf(QRegExp(value.toString()));
                  newval = -(-AFFERENT_EVENT + idx);
                  break;
               default:
                  cout << "Unexpected case #2 in plot table" << endl;
                  newval = stdCombo.indexOf(QRegExp(value.toString()));
                  break;
            }
         }
         else
            newval = value;

         (*iter).rec[col]=newval.toString();
         dataChanged(index,index);
         emit notifyChg(true);
      }
   }
   return true;
}

Qt::DropActions plotModel::supportedDropActions() const
{
   return Qt::MoveAction;
}

Qt::ItemFlags plotModel::flags(const QModelIndex& index) const
{
   Qt::ItemFlags flagval = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
   if (index.isValid())
   {
      flagval |= Qt::ItemIsDragEnabled;
      if (index.column() != PLOT_POP && index.column() != PLOT_CELL_FIB)
         flagval |= Qt::ItemIsEditable;
   }
   else
      flagval |= Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled; // can only drop between rows
   return flagval;
}

void plotModel::updHeader(const QModelIndex& curr)
{
   int row = curr.row();
   if (row >= 0)
   {
      auto iter = plotData[currModel].begin();
      advance(iter,row);
      plotRec rec = *iter;
      if ((*iter).popType == LUNG_CELL)
      {
         colFour = "Offset/10000";
         colFive  = "Scaling/10000";
      }
      else
      {
         if (rec.rec[PLOT_TYPE] >= 4 || (rec.rec[PLOT_TYPE] == AFFERENT_BIN))
         {
            colFour = "Bin Width (ms)";
            colFive = "Scaling";
         }
         else if (rec.rec[PLOT_TYPE] == 3 || rec.rec[PLOT_TYPE] == AFFERENT_INST)
         {
            colFour = "";
            colFive = "Scaling";
         }
         else
         {
            colFour = "";
            colFive = "";
         }
      }
   }
}

QVariant plotModel::headerData(int section,Qt::Orientation orientation, int role) const
{
   if (role == Qt::DisplayRole)
   {
      if (orientation == Qt::Horizontal)
      {
         switch (section)
         {
            case PLOT_CELL_FIB:
               return QString("Type");
            case PLOT_POP:
               return QString("Population");
            case PLOT_MEMB:
               return QString("Member");
            case PLOT_TYPE:
               return QString("Plot Variable");
            case PLOT_BINWID:
               return colFour;
            case PLOT_SCALE:
               return colFive;
            default:
               return QString("");
         }
      }
      else if (orientation == Qt::Vertical)
      {
         return section+1;
      }
   }
   return QVariant();
}


////////////////////////
// BDT / EDT table model
////////////////////////

bdtModel::bdtModel(QObject *parent) : QAbstractTableModel(parent)
{
   bdtData.resize(MAX_LAUNCH);
}

bdtModel::~bdtModel()
{
}

int bdtModel::rowCount(const QModelIndex & /*parent*/) const
{
   return bdtData[currModel].size();
}

int bdtModel::columnCount(const QModelIndex & /*parent*/) const
{
    return BDT_VISABLE_NUM_COL;
}

void bdtModel::dupModel(int from, int to)
{
   bdtData[to] = bdtData[from];
   emit notifyChg(true);
}


// Users has selected a cell or fiber and sent it to us to add to bdt/edt list.
// We pick a random pop since we want to avoid dups, and it is easier for us to
// check the table than the parent. There cannot be duplicates in a .bdt/.edt
// file because this breaks downstream code if there is. Each population can
// have a max entries that is equal to the number of cells in the pop. If,
// e.g., there are 5 cells in a population, you can only have 5 (different)
// entries from that pop. We inform user and refuse to add additional entries to
// the bdt table for that population.
void bdtModel::addRow(bdtRec& row_data, int row)
{
   bdtRowIter iter;

   if (row == -1)   // append at end?
   {
      iter = bdtData[currModel].end();
      row = bdtData[currModel].size();
   }
   else
   {
      iter = bdtData[currModel].begin();
      advance(iter, row);
   }

   if (row_data.rec[BDT_MEMB] == "0")  // "0" means create random member
   {
      // first, see if we have already used up all of the cell number range
      bdtRowIter dup_chk;
      int pop_used = 0;
      for (dup_chk = bdtData[currModel].begin(); dup_chk != bdtData[currModel].end(); ++dup_chk)
      {
         if ((*dup_chk).rec[BDT_CELL_FIB].toInt() == row_data.rec[BDT_CELL_FIB]
               && (*dup_chk).rec[BDT_POP].toInt() == row_data.rec[BDT_POP])
            ++pop_used;
         if (pop_used == row_data.maxRndVal)
            return; // have used up all of the unique pop nums for this pop, ignore the add
      }

      int rand_memb; // ensure no random dups
      bool found = false, try_again;
      while (!found)
      {
         rand_memb = rand() % row_data.maxRndVal+1;
         try_again = false;
         for (dup_chk = bdtData[currModel].begin(); dup_chk != bdtData[currModel].end(); ++dup_chk)
         {
            if ( ((*dup_chk).rec[BDT_CELL_FIB].toInt() == row_data.rec[BDT_CELL_FIB])
                && ((*dup_chk).rec[BDT_POP].toInt() == row_data.rec[BDT_POP])
                && (rand_memb == (*dup_chk).rec[BDT_MEMB].toInt()))
            {
               try_again = true;
               break;   // get another rnd #
            }
         }
         if (try_again)
            continue;
         found = true;
      }
      QString rand_str;
      rand_str.setNum(rand_memb);
      row_data.rec[BDT_MEMB] = rand_str;
   }
   beginInsertRows(QModelIndex(), row, row);
   bdtData[currModel].insert(iter,row_data);
   endInsertRows();
   emit notifyChg(true);
}

void bdtModel::reset()
{
   beginResetModel();
   for (auto &page : bdtData)
   {
      if (page.size())
      {
         beginRemoveRows(QModelIndex(), 0, page.size()-1);
         page.clear();
         endRemoveRows();
      }
   }
   endResetModel();
}

void bdtModel::delRow(int row)
{
   removeRows(row,1);
}


// need this for drag and drop, we insert with our own api
bool bdtModel::removeRows(int row, int count, const QModelIndex &/*parent*/)
{
   bdtRowIter iter;
   iter = bdtData[currModel].begin();
   advance(iter, row);

   beginRemoveRows(QModelIndex(), row, row + count - 1);
   while (count--)
      iter = bdtData[currModel].erase(iter);
   endRemoveRows();
   emit notifyChg(true);
   return true;
}

// access to underlying data struct
bool bdtModel::readRec(bdtRec& bRec, int row)
{
   bool found = false;
   int size = bdtData[currModel].size();
   if (row >= 0 && row < size)
   {
      bdtRowIter iter;
      iter = bdtData[currModel].begin();
      advance(iter, row);
      bRec = *iter;
      found = true;
   }
   return found;
}

// access to underlying data struct
bool bdtModel::updateRec(bdtRec& bRec, int row)
{
   bool found = false;
   int size = bdtData[currModel].size();
   if (row >= 0 && row < size)
   {
      bdtRowIter iter;
      iter = bdtData[currModel].begin();
      advance(iter, row);
      *iter = bRec;
      found = true;
      emit notifyChg(true);
      QModelIndex idxl = createIndex(row,BDT_MEMB,this);
      QModelIndex idxr = createIndex(row,BDT_TYPE,this);
      dataChanged(idxl,idxr);
   }
   return found;
}

bool bdtModel::canDropMimeData(const QMimeData *data, Qt::DropAction /*action*/, int /*row*/, int /*column*/, const QModelIndex &/*parent*/) const
{
   QStringList types = mimeTypes();
   QString format = types.at(0);
   if (data->hasFormat(format))
      return true;
   else
      return false;
}

bool bdtModel::dropMimeData(const QMimeData *data, Qt::DropAction /*action*/, int row, int /*column*/, const QModelIndex &/*parent*/)
{
   const bdtDropData* dragRec = dynamic_cast <const bdtDropData*>(data);
   if (row == -1)
      cout << "unexpected -1 for row " << endl;

   if (dragRec)
   {
      QStringList types = mimeTypes();
      QString format = types.at(0);
      if (data->hasFormat(format))
      {
         bdtRec rec = dragRec->one_rec;
         addRow(rec,row);
         return true;
      }
   }
   return false;
}

QStringList bdtModel::mimeTypes() const
{
   QStringList types;
   types << "application/text.list";
   return types;
}

QMimeData* bdtModel::mimeData(const QModelIndexList &indexes) const
{
   QStringList types = mimeTypes();
   QString format = types.at(0);
   bdtDropData *rec = new bdtDropData;
   QByteArray encodedData;
   QDataStream stream(&encodedData, QIODevice::WriteOnly);
   bdtRowConstIter iter;
   iter = bdtData[currModel].cbegin();
   int row = indexes[currModel].row(); // same row for all indexes
   advance(iter, row);         // copy this row's values
   (*rec).one_rec = *iter;
   stream << rec;
   rec->setData(format, encodedData);
   return rec;
}


// Sort ascending.  This is kind of a strange sort in that the 
// first column is always cell, then fiber, because that is how
// scope will show the chans.
static bool bdtCompareRowsA(const bdtRec& row1, const bdtRec& row2)
{
   int type1, type2;
   int r1c2,r1c3;
   int r2c2,r2c3;  // numerical compares, not text

   r1c2 = row1.rec[BDT_POP].toInt();
   r1c3 = row1.rec[BDT_MEMB].toInt();
   r2c2 = row2.rec[BDT_POP].toInt();
   r2c3 = row2.rec[BDT_MEMB].toInt();
   type1 = row1.rec[BDT_CELL_FIB].toInt();
   type2 = row2.rec[BDT_CELL_FIB].toInt();

   if (type1 > type2)
      return true;
   else if (type1 == type2)
   {
      if (r1c2 > r2c2)
         return true;
      else if (r1c2 == r2c2)
         if (r1c3 > r2c3)
            return true;
   }
   return false;
}

// Sort descending
static bool bdtCompareRowsD(const bdtRec& row1, const bdtRec& row2)
{
   int type1, type2;
   int r1c2,r1c3;
   int r2c2,r2c3;

   r1c2 = row1.rec[BDT_POP].toInt();
   r1c3 = row1.rec[BDT_MEMB].toInt();
   r2c2 = row2.rec[BDT_POP].toInt();
   r2c3 = row2.rec[BDT_MEMB].toInt();
   type1 = row1.rec[BDT_CELL_FIB].toInt();
   type2 = row2.rec[BDT_CELL_FIB].toInt();
   if (type1 > type2) 
      return true;
   else if (type1 == type2)
   {
      if (r1c2 < r2c2)
         return true;
      else if (r1c2 == r2c2)
         if (r1c3 < r2c3)
           return true;
   }
   return false;
}

void bdtModel::sort(int /*column*/, Qt::SortOrder order)
{
   layoutAboutToBeChanged();
   if (order == Qt::AscendingOrder)
      bdtData[currModel].sort(bdtCompareRowsA);
   else
      bdtData[currModel].sort(bdtCompareRowsD);
   layoutChanged(QList<QPersistentModelIndex>(),QAbstractItemModel::VerticalSortHint);
   emit notifyChg(true);
}


QVariant bdtModel::data(const QModelIndex &index, int role) const
{
    QVariant ret;
    int row, col;
    row = index.row();
    col = index.column();

    switch (role)
    {
       case Qt::DisplayRole:
       case Qt::EditRole:
          if (bdtData[currModel].size())
          {
             auto iter = bdtData[currModel].begin();
             advance(iter,row);
             bdtRec value = *iter;
             if (col == BDT_CELL_FIB)
             {
                if (value.rec[col] == CELL)
                   ret = "CELL";
                else
                   ret = "FIBER";
             }
             else
                ret = value.rec[col];
          }
          break;

       case Qt::TextAlignmentRole:
       {
          switch(col)
          {
             case BDT_CELL_FIB:
                ret = Qt::AlignHCenter + Qt::AlignVCenter;
                break;

             case BDT_POP:
             case BDT_MEMB:
             default:
                ret = Qt::AlignRight + Qt::AlignVCenter;
                break;
          }
       }
       break;

       case Qt::ToolTipRole:
          switch (col)
          {
             default:
                break;
          }
          break;
      default:
         break;
    }
    return ret;
}

bool bdtModel::setData(const QModelIndex & index, const QVariant & value, int role)
{
   if (role == Qt::EditRole)
   {
      if (bdtData[currModel].size())
      {
         int row = index.row();
         int col = index.column();
         auto iter = bdtData[currModel].begin();
         advance(iter,row);
         (*iter).rec[col]=value.toString();
         dataChanged(index,index);
         emit notifyChg(true);
      }
   }
   return true;
}
Qt::DropActions bdtModel::supportedDropActions() const
{
   return Qt::MoveAction;
}

Qt::ItemFlags bdtModel::flags(const QModelIndex& index) const
{
   Qt::ItemFlags flagval = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
   if (index.isValid())
   {
      flagval |= Qt::ItemIsDragEnabled;
      if (index.column() == BDT_MEMB)
         flagval |= Qt::ItemIsEditable;
   }
   else
      flagval |= Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
   return flagval;
}


QVariant bdtModel::headerData(int section,Qt::Orientation orientation, int role) const
{
   if (role == Qt::DisplayRole)
   {
      if (orientation == Qt::Horizontal)
      {
         switch (section)
         {
            case BDT_CELL_FIB:
               return QString("Type");
            case BDT_POP:
               return QString("Population");
            case BDT_MEMB:
               return QString("Member");
         }
      }
      else if (orientation == Qt::Vertical)
      {
         return section+1;
      }
   }
   return QVariant();
}


plotCombo::plotCombo(QObject *parent) : QStyledItemDelegate(parent)
{
}

plotCombo::~plotCombo()
{
}

QWidget* plotCombo::createEditor(QWidget *parent, const QStyleOptionViewItem & /*option*/, const QModelIndex & index) const
{
   int row = index.row();
   const plotModel* model = dynamic_cast<const plotModel*>(index.model());
   auto iter = model->plotData[model->currModel].begin();
   advance(iter,row);
   int cell_type = (*iter).popType;
   if (cell_type == STD_FIBER)
   {
      QLabel* editor = new QLabel(parent);  // this gets r/o text
      return editor;
   }
   else // all other cases get a combo box
   {
      QComboBox* editor = new QComboBox(parent);
      editor->setFrame(true);
      if (cell_type == BURSTER_CELL)
         editor->addItems(bursterCombo);
      else if (cell_type == LUNG_CELL)
         editor->addItems(lungCombo);
      else if (cell_type == AFFERENT_EVENT)
         editor->addItems(afferentCombo);
      else
         editor->addItems(stdCombo);
      return editor;
   }
}

void plotCombo::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    const plotModel* model = dynamic_cast<const plotModel*>(index.model());
    auto iter = model->plotData[model->currModel].begin();
    int row = index.row();
    advance(iter,row);
    int value = (*iter).rec[PLOT_TYPE].toInt();
    if (value == AFFERENT_INST)
    {
       (*iter).rec[PLOT_BINWID] = "";
    }
    else if (value != AFFERENT_BIN && value >= 0 && value < 4)
    {
       (*iter).rec[PLOT_BINWID] = "";
       (*iter).rec[PLOT_SCALE] = "";
    }

    if ((*iter).popType == LUNG_CELL)
      value = -(value+1);
    if ((*iter).popType == AFFERENT_EVENT)
      value = -(value-AFFERENT_EVENT);

    QComboBox *combo = dynamic_cast<QComboBox*>(editor);
    if (combo)
       combo->setCurrentIndex(value);
}

void plotCombo::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    QComboBox *combo = dynamic_cast<QComboBox*>(editor);
    if (combo)
    {
       QString value = combo->currentText();
       model->setData(index, value, Qt::EditRole);
    }
}

void plotCombo::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const
{
    editor->setGeometry(option.rect);
}


plotSpin::plotSpin(QObject *parent) : QStyledItemDelegate(parent)
{
}

plotSpin::~plotSpin()
{
}


// Overloading these fields to (more or less) conform to newsned files makes this
// rather ugly and a maintenance burden.
QWidget* plotSpin::createEditor(QWidget *parent, const QStyleOptionViewItem & /*option*/, const QModelIndex& index) const
{
    const plotModel* model = dynamic_cast<const plotModel*>(index.model());
    auto iter = model->plotData[model->currModel].begin();
    int row = index.row();
    int col = index.column();
    advance(iter,row);
    int value = (*iter).rec[PLOT_TYPE].toInt();
    if (value >= 4 || value == AFFERENT_BIN) // bin width and scale
    {
       QSpinBox* editor = new QSpinBox(parent);
       editor->setFrame(true);
       editor->setAlignment(Qt::AlignRight);
       if (col == PLOT_BINWID)
       {
          editor->setMinimum(1);
          editor->setMaximum(1000);
       }
       else if (col == PLOT_SCALE)
       {
          editor->setMinimum(1);
          editor->setMaximum(10000);
       }
       return editor;
    }
    else if (value == 3 || value == AFFERENT_INST) // scale
    {
       if (col == PLOT_SCALE)
       {
          QSpinBox* editor = new QSpinBox(parent);
          editor->setFrame(true);
          editor->setAlignment(Qt::AlignRight);
          editor->setMinimum(1);
          editor->setMaximum(10000);
          return editor;
       }
    }
    else if (value < 0 && value > STD_FIBER) // lung model, no obvious limits to this in newsned code
    {
       QSpinBox* editor = new QSpinBox(parent);
       editor->setFrame(true);
       editor->setAlignment(Qt::AlignRight);
       editor->setMinimum(-1000000);
       editor->setMaximum( 1000000);
       return editor;
    }

    return nullptr; // rest of types have no info in these columns
}

void plotSpin::setEditorData(QWidget *editor, const QModelIndex &index) const
{
   int value = index.model()->data(index, Qt::EditRole).toInt();

   QSpinBox *spin = dynamic_cast<QSpinBox*>(editor);
   if (spin)
      spin->setValue(value);

}

void plotSpin::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    QSpinBox *spin = dynamic_cast<QSpinBox*>(editor);
    if (spin)
    {
       spin->interpretText();
       int value = spin->value();
       model->setData(index, value, Qt::EditRole);
    }
}

void plotSpin::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const
{
    editor->setGeometry(option.rect);
}



plotEdit::plotEdit(QObject *parent) : QStyledItemDelegate(parent)
{
}

plotEdit::~plotEdit()
{
}


QWidget* plotEdit::createEditor(QWidget *parent, const QStyleOptionViewItem & /*option*/, const QModelIndex& index) const
{
   QLineEdit* editor = new QLineEdit(parent);
   int maxcell=100;

   const plotModel* model1 = dynamic_cast<const plotModel*>(index.model());
   const bdtModel* model2 = dynamic_cast<const bdtModel*>(index.model());
   if (model1)
   {
      if (index.column() == PLOT_MEMB)
      {
         int row = index.row();
         {
            auto iter = model1->plotData[model1->currModel].begin();
            advance(iter, row);
            maxcell = (*iter).maxRndVal;
         }
      }
   }
   else if (model2)
   {
      if (index.column() == BDT_MEMB)
      {
         int row = index.row();
         {
            auto iter = model2->bdtData[model2->currModel].begin();
            advance(iter, row);
            maxcell = (*iter).maxRndVal;
         }
      }
   }
   else
      cout << "There seems to be an unknown model in the line edit delegate" << endl;

   QValidator* validator = new QIntValidator(1,maxcell,parent);
   editor->setAlignment(Qt::AlignRight);
   editor->setValidator(validator);
   return editor;
}

void plotEdit::setEditorData(QWidget *editor, const QModelIndex &index) const
{
   QString value = index.model()->data(index, Qt::EditRole).toString();
   QLineEdit *line = dynamic_cast<QLineEdit*>(editor);
   if (line)
      line->setText(value);
}

void plotEdit::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    QLineEdit *line = dynamic_cast<QLineEdit*>(editor);
    if (line)
    {
       QString value = line->text();
       model->setData(index, value, Qt::EditRole);
    }
}

void plotEdit::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const
{
    editor->setGeometry(option.rect);
}


analogWidgetModel::analogWidgetModel(QObject *parent) : QAbstractTableModel(parent)
{
   analogData.resize(MAX_LAUNCH);
}

analogWidgetModel::~analogWidgetModel()
{
}

int analogWidgetModel::rowCount(const QModelIndex & /*parent*/ ) const
{
   return analogData.size();
}

int analogWidgetModel::columnCount(const QModelIndex &/* parent*/ ) const
{
    return ANALOG_NUM_COL;
}

void analogWidgetModel::dupModel(int from, int to)
{
   analogData[to] = analogData[from];
   emit notifyChg(true);
}

void analogWidgetModel::reset()
{
   blockSignals(true);
   beginResetModel();
   for (int page = 0; page < rowCount(); ++page)
   {
         beginRemoveRows(QModelIndex(), 0, page);
         removeRows(0,rowCount(),QModelIndex());
         endRemoveRows();
   }
   endResetModel();
   blockSignals(false);
}


void analogWidgetModel::addRec(analogRec& rec,int row)
{
   beginInsertRows(QModelIndex(), row, row);
   analogData[row].rec[ANALOG_ID] = rec.rec[ANALOG_ID].toInt();
   analogData[row].rec[ANALOG_CELLPOP] = rec.rec[ANALOG_CELLPOP].toInt();
   analogData[row].rec[ANALOG_RATE] = rec.rec[ANALOG_RATE].toInt();
   analogData[row].rec[ANALOG_TIME] = rec.rec[ANALOG_TIME].toFloat();
   analogData[row].rec[ANALOG_SCALE] = rec.rec[ANALOG_SCALE].toFloat();
   analogData[row].rec[ANALOG_P1CODE] = rec.rec[ANALOG_P1CODE].toInt();
   analogData[row].rec[ANALOG_P1START] = rec.rec[ANALOG_P1START].toInt();
   analogData[row].rec[ANALOG_P1STOP] = rec.rec[ANALOG_P1STOP].toInt();
   analogData[row].rec[ANALOG_P2CODE] = rec.rec[ANALOG_P2CODE].toInt();
   analogData[row].rec[ANALOG_P2START] = rec.rec[ANALOG_P2START].toInt();
   analogData[row].rec[ANALOG_P2STOP] = rec.rec[ANALOG_P2STOP].toInt();
   analogData[row].rec[ANALOG_IE_SMOOTH] = rec.rec[ANALOG_IE_SMOOTH].toInt();
   analogData[row].rec[ANALOG_IE_SAMPLE] = rec.rec[ANALOG_IE_SAMPLE].toInt();
   analogData[row].rec[ANALOG_IE_PLUS] = rec.rec[ANALOG_IE_PLUS].toFloat();
   analogData[row].rec[ANALOG_IE_MINUS] = rec.rec[ANALOG_IE_MINUS].toFloat();
   analogData[row].rec[ANALOG_IE_FREQ] = rec.rec[ANALOG_IE_FREQ].toInt();
   endInsertRows();
   emit notifyChg(true);
}

bool analogWidgetModel::readRec(analogRec& rec,int row)
{
   bool found = false;
   int size = analogData.size();
   if (row >= 0 && row < size)
   {
      rec = analogData[row];
      found = true;
   }
   return found;
}

// fetch model data to display
QVariant analogWidgetModel::data(const QModelIndex &index, int role) const
{
    QVariant ret;
    int row, col;
    row = index.row();
    col = index.column();
   switch (role)
   {
      case Qt::DisplayRole:
      case Qt::EditRole:
         ret = analogData[row].rec[col];
         break;

      default:
         break;
   }
   return ret;
}

// save display data to model
bool analogWidgetModel::setData(const QModelIndex & index, const QVariant & value, int role)
{
   if (role == Qt::EditRole)
   {
      int row = index.row();
      int col = index.column();
      analogData[row].rec[col] = value.toInt();
      switch(col)
      {
         case ANALOG_TIME:
         case ANALOG_SCALE:
         case ANALOG_IE_PLUS:
         case ANALOG_IE_MINUS:
            analogData[row].rec[col] = value.toFloat();
            break;
         default:
            analogData[row].rec[col] = value.toInt();
            break;
      }
      dataChanged(index,index);
      emit notifyChg(true);
   }
   return true;
}

hostNameWidgetModel::hostNameWidgetModel(QObject *parent) : QAbstractTableModel(parent)
{
   hostNameData.resize(MAX_LAUNCH);
}

hostNameWidgetModel::~hostNameWidgetModel()
{
}

int hostNameWidgetModel::rowCount(const QModelIndex & /*parent*/ ) const
{
   return hostNameData.size();
}

int hostNameWidgetModel::columnCount(const QModelIndex &/* parent*/ ) const
{
    return HOSTNAME_NUM_COL;
}

void hostNameWidgetModel::dupModel(int from, int to)
{
   hostNameData[to] = hostNameData[from];
   emit notifyChg(true);
}

void hostNameWidgetModel::reset()
{
   blockSignals(true);
   beginResetModel();
   for (int page = 0; page < rowCount(); ++page)
   {
      beginRemoveRows(QModelIndex(), 0, page);
      removeRows(0,rowCount(),QModelIndex());
      endRemoveRows();
   }
   endResetModel();
   blockSignals(false);
}


void hostNameWidgetModel::addRec(hostNameRec& rec,int row)
{
   beginInsertRows(QModelIndex(), row, row);
   hostNameData[row].rec = rec.rec;
   endInsertRows();
   emit notifyChg(true);
}

bool hostNameWidgetModel::readRec(hostNameRec& rec,int row)
{
   bool found = false;
   int size = hostNameData.size();
   if (row >= 0 && row < size)
   {
      rec = hostNameData[row];
      found = true;
   }
   return found;
}

// fetch model data to display
QVariant hostNameWidgetModel::data(const QModelIndex &index, int role) const
{
    QVariant ret;
    int row;
    row = index.row();
   switch (role)
   {
      case Qt::DisplayRole:
      case Qt::EditRole:
         ret = hostNameData[row].rec;
         break;

      default:
         break;
   }
   return ret;
}

// save display data to model
bool hostNameWidgetModel::setData(const QModelIndex & index, const QVariant & value, int role)
{
   if (role == Qt::EditRole)
   {
      int row = index.row();
      int col = index.column();
      hostNameData[row].rec = value.toString();
      switch(col)
      {
         case HOSTNAME_ID:
            hostNameData[row].rec = value.toString();
            break;
         default:
            break;
      }
      dataChanged(index,index);
      emit notifyChg(true);
   }
   return true;
}

