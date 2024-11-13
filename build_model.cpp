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
#include <QValidator>
#include "launch_model.h"
#include <string>
#include <typeinfo>
#include <math.h>
#include "build_model.h"
#include "simscene.h"
#include "c_globals.h"

using namespace std;

synModel::synModel(QObject *parent) : QAbstractItemModel(parent)
{
   synRec rec;
   rootItem = new synItem(rec);
}

synModel::~synModel()
{
   delete rootItem;
}

int synModel::rowCount(const QModelIndex & parent) const
{
   synItem *parentItem = getItem(parent);
   if (parentItem)
      return parentItem->childCount();
   else
      return 0;
}

int synModel::columnCount(const QModelIndex & /*parent*/) const
{
    return SYN_NUM_COL;
}

QModelIndex synModel::index(int row, int column, const QModelIndex &parent) const
{
   QModelIndex ret;

   if (!hasIndex(row,column,parent)) // Qt asks for illegal indices if the last
      return ret;                    // row is selected and is deleted.
   if (parent.isValid() && parent.column() != 0)
      return ret;
   synItem *parentItem = getItem(parent);
   synItem *childItem = parentItem->child(row);
   if (childItem)
   {
      ret = createIndex(row, column, childItem);
//      cout << "index: create index " << row << " " << column << " for " << childItem << " with " << ret.internalPointer() << endl;
   }

   return ret;
}

QModelIndex synModel::parent(const QModelIndex &index) const
{
   QModelIndex ret;

   if (!index.isValid())
      return ret;
   synItem *childItem = getItem(index);
   synItem *parentItem = childItem->parent();
   if (parentItem == rootItem)
      return ret;
   else if (parentItem)
   {
      int num = parentItem->childNumber();
      ret = createIndex(num, 0, parentItem);
//      cout << "parent: create index " << num << " " << 0 << " for " << parentItem << " with " << ret.internalPointer() << endl;
      return ret;
   }
   return ret;
}

synItem *synModel::getItem(const QModelIndex &index) const
{
   if (index.isValid())
   {
      synItem *item = static_cast<synItem*>(index.internalPointer());
      if (item)
         return item;
   }
    return rootItem;
}


// add new rec at row
// rec.node tells us where to add it
// rec.syn_type tells us if it is norm parent, learn, or pre/post child
// if child, rec.parent tells the parent, pre or post determines order
bool synModel::addRow(synRec& rec, QModelIndex& newidx)
{
   int type = rec.rec[SYN_TYPE].toInt();
   int root_row, root_id, row;
   QModelIndex parent, idx;
   bool success = true;
   synItem* parentItem;

   if (type == SYN_NORM || type == SYN_LEARN)
   {
      parentItem = getItem(parent); // root item, children are norm syns
      root_row = rec.rec[SYN_NUM].toInt()-1;
      if (root_row > rowCount(parent)) // add at end
         root_row = rowCount(parent);
      beginInsertRows(parent, root_row, root_row);
      success = parentItem->insertChild(root_row,rec);
      endInsertRows();
      newidx = index(root_row,0,parent);
   }
   else
   {
      root_id = rec.rec[SYN_PARENT].toInt(); // parent index
      idx = idToIndex(root_id);
      parentItem = getItem(idx);
      if (type == SYN_PRE)
         row = 0;
      else if (type == SYN_POST)
         row = 1;
      else
      {
         cout << "some other kind of child row" << endl;
         return false;
      }
      beginInsertRows(idx, row, row);
      success = parentItem->insertChild(row,rec);
      endInsertRows();
      newidx = index(row,0,parent);
   }
   return success;
}



// To get an index to a root row record, we have to know the
// row number. We only have synapse id/index. So, scan all
// the recs to get the row # and return its index
QModelIndex synModel::idToIndex(int id)
{
   QModelIndex idx, row_idx;
   int num_rows = rowCount(idx); // # normal rows
   synRec rec; 
   int row;

   for (row=0; row < num_rows; ++row) 
   {
      row_idx = index(row,0,idx);
      if (row_idx.isValid())
      {
         readRec(rec,row_idx);
         if (rec.rec[SYN_NUM].toInt() == id)
            return row_idx;
      }
   }
   return idx;
}


void synModel::delRow(int row, const QModelIndex& idx)
{
   removeRows(row,1,idx);
}

void synModel::reset()
{
   if (rootItem)
      removeRows(0, rowCount(QModelIndex()), QModelIndex());
}


// access to underlying data struct
bool synModel::readRec(synRec& rec, const QModelIndex& idx)
{
   synItem *item;
   item = getItem(idx);
   rec = item->synItemData;
   return true;
}

void synModel::recycleSynapses(const QModelIndex& idx)
{
   int id;
   synItem *us = getItem(idx);
   for (auto iter = us->childItems.begin(); iter != us->childItems.end(); ++iter)
   {
      id = (*iter)->synId();
      freeSynapse(id);
   }
   id = us->synId();
   freeSynapse(id);
}


QVariant synModel::data(const QModelIndex &index, int role) const
{
   QVariant ret;
   if (!index.isValid())
      return ret;

   int col = index.column();
   synItem *item;
   int type;

   switch (role)
   {
      case Qt::DisplayRole:
      case Qt::EditRole:
         item = getItem(index);
         type =item->synType();
         if ((col == SYN_NAME || col == SYN_TYPE) && type != SYN_NORM && type != SYN_LEARN)
            ret = "     " + item->data(col).toString();
         else if (col == SYN_NUM && (type == SYN_NORM || type == SYN_LEARN))
            ret = item->data(col).toString() + "     ";
         else if (type != SYN_LEARN && (col == SYN_LRN_WINDOW || col == SYN_LRN_DELTA || col == SYN_LRN_MAX))
            ; // do nothing
         else if (col == SYN_EQPOT || col == SYN_TC || col == SYN_LRN_DELTA || col == SYN_LRN_MAX)
         {
            double num;
            num = item->data(col).toDouble();
            ret = QString::number(num,'f',4);
         }
         else
            ret = item->data(col);
         break;

      case Qt::TextAlignmentRole:
         switch(col)
         {
            case SYN_NAME:
            case SYN_TYPE:
               ret = Qt::AlignLeft + Qt::AlignVCenter;
               break;
            case SYN_PARENT:
            default:
               ret = Qt::AlignRight + Qt::AlignVCenter;
               break;
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

bool synModel::setData(const QModelIndex & index, const QVariant & value, int role)
{
   if (role != Qt::EditRole)
      return false;

    synItem *item = getItem(index);
    bool result = item->setData(index.column(), value);
    if (result)
    {
        emit dataChanged(index, index);
    }
    return result;
}

Qt::ItemFlags synModel::flags(const QModelIndex& index) const
{
   Qt::ItemFlags flagval = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
   if (index.isValid())
   {
      if (index.column() != SYN_NUM && index.column() != SYN_TYPE)
         flagval |= Qt::ItemIsEditable;
   }
   return flagval;
}

bool synModel::hasPre(const QModelIndex& index)
{
   bool ret = false;
   synItem *item = getItem(index);
   if (item)
   {
      for (auto iter = item->childItems.begin(); iter != item->childItems.end(); ++iter)
      {
         if ((*iter)->synItemData.rec[SYN_TYPE] == SYN_PRE)
         {
            ret = true;
            break;
         }
      }
   }
   return ret;
}

bool synModel::hasPost(const QModelIndex& index)
{
   bool ret = false;
   synItem *item = getItem(index);
   if (item)
   {
      for (auto iter = item->childItems.begin(); iter != item->childItems.end(); ++iter)
      {
         if ((*iter)->synItemData.rec[SYN_TYPE] == SYN_POST)
         {
            ret = true;
            break;
         }
      }
   }
   return ret;
}


// only H headers, not using V headers
QVariant synModel::headerData(int section,Qt::Orientation orientation, int role) const
{
   if (orientation == Qt::Horizontal)
   {
      if (role == Qt::DisplayRole)
      {
        switch (section)
        {
           case SYN_NUM:
              return QString("Number");
           case SYN_NAME:
              return QString("Name");
           case SYN_TYPE:
              return QString("Type");
           case SYN_EQPOT:
              return QString("Equilibrium\n Potential (mv)");
           case SYN_TC:
              return QString("Time\nConstant (ms)");
           case SYN_LRN_WINDOW:
              return QString("Learn\nWindow (ticks)");
           case SYN_LRN_DELTA:
              return QString("Strength\nMultiplier");
           case SYN_LRN_MAX:
              return QString("Max/Min\nStrength");
           case SYN_PARENT:
              return QString();
              //return QString("(debug)parent");
        }
     }
     else if (role == Qt::TextAlignmentRole)
        return Qt::AlignHCenter + Qt::AlignVCenter;
   }
   return QVariant();
}

// We are showing the build/edit synapse type page.
// Unlike most of the edit node windows, where we deal
// with one rec at a time, we really do need to let 
// changes be made to the Synapse Type db. To revert
// to previous state, we make a copy of the current
// DB recs. Undo over-writes any changes to the DB
// with the saved copy.
void synModel::prepareUndo()
{
   dbCopy = D.inode[SYNAPSE_INODE].unode.synapse_node;
}

// Copy previous to current, will force view update

void synModel::undo()
{
   beginResetModel();
   D.inode[SYNAPSE_INODE].unode.synapse_node = dbCopy;
   get_maxes(&D);
   endResetModel();
}

// Don't need undo recs any more
void synModel::clearUndo()
{
  dbCopy={};
}

bool synModel::removeRows(int position, int rows, const QModelIndex &parent)
{
   if (rows == 0)
      return true;

   synItem *parentItem = getItem(parent);
   bool success = true;
   beginResetModel(); // if you don't do this, random stray pointers sometimes cause crash
   beginRemoveRows(parent, position, position + rows - 1);
   success = parentItem->removeChildren(position, rows);
   endRemoveRows();
   endResetModel();
   return success;
}

// do not need, but needs to be defined 
bool synModel::insertRows(int /*position*/, int /*rows*/, const QModelIndex &/*parent*/)
{
   cout << " *********** insertrows unexpectedly called" << endl;
#if 0
    synItem *parentItem = getItem(parent);
    bool success;

    beginInsertRows(parent, position, position + rows - 1);
    success = parentItem->insertChildren(position, rows, synData);
    endInsertRows();
    return success;
#endif
    return true;
}


synItem::synItem(synRec& rec, synItem *parent) : synItemData(rec), parItem(parent)
{
}

synItem::~synItem()
{
//   for (auto iter = childItems.begin(); iter != childItems.end(); ++iter)
//     delete *iter;
   qDeleteAll(childItems);
}

synItem* synItem::parent()
{
   return parItem;
}

int synItem::columnCount() const
{
    return SYN_NUM_COL;
}

synItem * synItem::child(int row)
{
   auto iter = childItems.begin();
   advance(iter,row);
   if (iter != childItems.end())
      return *iter;
   else
      return nullptr;
}


int synItem::childCount() const
{
   return childItems.size();
}

int synItem::childNumber() const
{
   int index = 0;
   if (parItem)
   {
      for (auto it = childItems.begin(); it != childItems.end(); ++it, ++index)
         if (*it == this)
            return index;
   }
   return 0;
}

QVariant synItem::data(int col) const
{
   QVariant ret;
   if (col == SYN_TYPE)
   {
      int index = synItemData.rec[SYN_TYPE].toInt() - 1; // zero based
      ret = synTypeNames[index];
   }
   else
      ret = synItemData.rec[col];
   return ret;
}

bool synItem::setData(int column, const QVariant &value)
{
    if (column < 0 || column >= SYN_NUM_COL)
        return false;
    synItemData.rec[column] = value;
    return true;
}



bool synItem::insertChild(int position, synRec& rec)
{
   synItem *item = new synItem(rec, this);
   auto iter = childItems.begin();
   advance(iter,position);
   childItems.insert(iter,item);
   return true;
}

// assumes there are count # of rows
bool synItem::insertChildren(int position, int count, synRow& rows)
{
   if (position < 0 || position > int(childItems.size()))
      return false;

   synRowIter iter = rows.begin();
   for (int row = 0; row < count; ++row,++iter)
   {
      if (iter != rows.end())
      {
         synItem *item = new synItem(*iter, this);
         childItems.insert(childItems.end(), item);
         ++iter;
      }
      else
         cout << " ran out of rows" << endl;
   }
    return true;
}

bool synItem::removeChild(int position)
{
// if the rootItem, children are normal synapses
// if a 1st order child, children are pre/post/maybe other types one day
   if (position < 0 || position > int(childItems.size()))
      return false;

    auto iter = childItems.begin();
    advance(iter,position);

    if (iter != childItems.end())
    {
       delete *iter;
       childItems.erase(iter);
    }
    return true;
}

bool synItem::removeChildren(int position, int count)
{
   if (count == 0)
      return true;

   if (position < 0 || position + count > int(childItems.size()))
      return false;

   auto iter = childItems.begin();
   advance(iter,position);
   for (int row = 0; iter != childItems.end() && row < count; ++row)
   {
      delete *iter;
      iter = childItems.erase(iter);
   }
   return true;
}


synDoubleSpin::synDoubleSpin(QWidget *parent): QDoubleSpinBox(parent)
{
}

synDoubleSpin::~synDoubleSpin()
{
}

synSpin::synSpin(QObject *parent, double min, double max) : QStyledItemDelegate(parent),spinMin(min),spinMax(max)
{
}

synSpin::~synSpin()
{
}

QWidget* synSpin::createEditor(QWidget *parent, const QStyleOptionViewItem & /*option*/, const QModelIndex& /* index */) const
{
   synDoubleSpin* editor = new synDoubleSpin(parent);
   editor->setFrame(true);
   editor->setRange(spinMin,spinMax);
   editor->setAlignment(Qt::AlignRight);
   editor->setDecimals(decimals);
//   editor->setKeyboardTracking(false);  // no event for each keypress
   connect(editor, QOverload<double>::of(&QDoubleSpinBox::valueChanged),this, &synSpin::synSpinValueChg);
   return editor;;
}

void synSpin::setEditorData(QWidget *editor, const QModelIndex &index) const
{
   double value = index.model()->data(index, Qt::EditRole).toDouble();
   synDoubleSpin *spin = dynamic_cast<synDoubleSpin*>(editor);
   if (spin)
   {
      if (isnan(spin->old))
         spin->old = value;
      spin->blockSignals(true);   // program versus user changes
      spin->setValue(value);
      spin->blockSignals(false);
   }
}

void synSpin::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    synDoubleSpin *spin = dynamic_cast<synDoubleSpin*>(editor);
    if (spin)
    {
       double value = spin->value();
       if (spin->old != value)
          model->setData(index, value, Qt::EditRole);
    }
}

void synSpin::synSpinValueChg(double)
{
   SimScene* par = dynamic_cast<SimScene*>(parent());
   if (par)
      par->setBuildDirty(true);
}

void synSpin::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const
{
    editor->setGeometry(option.rect);
}

QString synSpin::displayText(const QVariant &value, const QLocale &locale) const
{
    return locale.toString(value.toDouble(), 'f', decimals);
}


synIntegerSpin::synIntegerSpin(QWidget *parent): QSpinBox(parent)
{
}

synIntegerSpin::~synIntegerSpin()
{
}

synIntSpin::synIntSpin(QObject *parent, int min, int max) : QStyledItemDelegate(parent),spinMin(min),spinMax(max)
{
}

synIntSpin::~synIntSpin()
{
}


QWidget* synIntSpin::createEditor(QWidget *parent, const QStyleOptionViewItem & /*option*/, const QModelIndex& index) const
{
   const synModel* model = dynamic_cast<const synModel*>(index.model());
   synItem *curr;
   if (model)
      curr = model->getItem(index);
   else
      return nullptr;
   if (curr->synType() != SYN_LEARN)
      return nullptr;
   synIntegerSpin* editor = new synIntegerSpin(parent);
   editor->setFrame(true);
   editor->setRange(spinMin,spinMax);
   editor->setAlignment(Qt::AlignRight);
//   editor->setKeyboardTracking(false);  // no event for each keypress
   connect(editor, QOverload<int>::of(&QSpinBox::valueChanged),this, &synIntSpin::synIntValueChg);
   return editor;;
}

void synIntSpin::setEditorData(QWidget *editor, const QModelIndex &index) const
{
   int value = index.model()->data(index, Qt::EditRole).toInt();
   synIntegerSpin *spin = dynamic_cast<synIntegerSpin*>(editor);
   if (spin)
   {
      if (isnan(spin->old))
         spin->old = value;
      spin->blockSignals(true);
      spin->setValue(value);
      spin->blockSignals(false);
   }
}

void synIntSpin::synIntValueChg(int)
{
   SimScene* par = dynamic_cast<SimScene*>(parent());
   if (par)
      par->setBuildDirty(true);
}

void synIntSpin::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    synIntegerSpin *spin = dynamic_cast<synIntegerSpin*>(editor);
    if (spin)
    {
       int value = spin->value();
       if (spin->old != value)
          model->setData(index, value, Qt::EditRole);
    }
}

void synIntSpin::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const
{
    editor->setGeometry(option.rect);
}

QString synIntSpin::displayText(const QVariant &value, const QLocale &locale) const
{
    return locale.toString(value.toInt());
}

synEdit::synEdit(QObject *parent, bool roMode) : QStyledItemDelegate(parent), readOnly(roMode)
{
}

synEdit::~synEdit()
{
}

QWidget* synEdit::createEditor(QWidget *parent, const QStyleOptionViewItem & /*option*/, const QModelIndex& /* index*/ ) const
{
   QLineEdit* editor = new QLineEdit(parent);
   editor->setAlignment(Qt::AlignRight);
   if (readOnly)
      editor->setReadOnly(readOnly);
   return editor;
}

void synEdit::setEditorData(QWidget *editor, const QModelIndex &index) const
{
   QString value = index.model()->data(index, Qt::EditRole).toString();
   QLineEdit *line = dynamic_cast<QLineEdit*>(editor);
   if (line)
      line->setText(value);
}

void synEdit::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    QLineEdit *line = dynamic_cast<QLineEdit*>(editor);
    if (line)
    {
       QString value = line->text();
       model->setData(index, value, Qt::EditRole);
    }
}

void synEdit::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const
{
    editor->setGeometry(option.rect);
}



