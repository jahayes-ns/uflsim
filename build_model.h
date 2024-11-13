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

#ifndef BUILD_MODEL_H
#define BUILD_MODEL_H

#include <QAbstractTableModel>
#include <QTableView>
#include <QItemDelegate>
#include <QStyledItemDelegate>
#include <QStandardItemModel>
#include <QDataWidgetMapper>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QStringList>
#include <limits>
#include <array>

#ifdef __cplusplus
extern "C" {
#endif
#include "inode.h"
#ifdef __cplusplus
}
#endif



// column offsets of build synapse table
const int SYN_NUM=0;       // index into various data structures 1-100
const int SYN_NAME=1;      // text name
const int SYN_TYPE=2;      // normal,pre,post
const int SYN_EQPOT=3;
const int SYN_TC=4;
const int SYN_LRN_WINDOW=5;
const int SYN_LRN_DELTA=6;
const int SYN_LRN_MAX=7;
const int SYN_PARENT=8;
const int SYN_NUM_COL=SYN_PARENT+1;

const QStringList synTypeNames(
      {
        "Normal",
        "Presynaptic",
        "Postsynaptic",
        "Learning"
       });


class synRec 
{
   public:
     QVariant rec[SYN_NUM_COL];
};

using synRow = std::list<synRec>;
using synRowIter = synRow::iterator;
using synRowConstIter = synRow::const_iterator;


class synDoubleSpin : public QDoubleSpinBox
{
   Q_OBJECT
   public:
      synDoubleSpin(QWidget *parent = 0);
      virtual ~synDoubleSpin();
      double old = std::numeric_limits<double>::quiet_NaN();
      QLineEdit* getLe(){return lineEdit();};
};

class synIntegerSpin : public QSpinBox
{
   Q_OBJECT
   public:
      synIntegerSpin(QWidget *parent = 0);
      virtual ~synIntegerSpin();
      int old = std::numeric_limits<int>::quiet_NaN();
      QLineEdit* getLe(){return lineEdit();};
};

// custom class for a spinbox in a cell for double precision
class synSpin : public QStyledItemDelegate
{
   Q_OBJECT
   public:
      synSpin(QObject *parent = 0, double min = -20000.0, double max = 20000.0);
      virtual ~synSpin();

   private slots:
     void synSpinValueChg(double);

   protected:
      QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                            const QModelIndex &index) const override;
      void setEditorData(QWidget *editor, const QModelIndex &index) const override;
      void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
      void updateEditorGeometry(QWidget *editor,
     const QStyleOptionViewItem &option, const QModelIndex &index) const override;
     QString displayText(const QVariant &, const QLocale &) const override;

   private:
      double spinMin;
      double spinMax;
      int decimals=4;
};

// custom class for a spinbox in a cell for ints
class synIntSpin : public QStyledItemDelegate
{
   Q_OBJECT
   public:
      synIntSpin(QObject *parent = 0, int min = -2000, int max = 2000);
      virtual ~synIntSpin();

   private slots:
     void synIntValueChg(int);

   protected:
      QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                            const QModelIndex &index) const override;
      void setEditorData(QWidget *editor, const QModelIndex &index) const override;
      void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
      void updateEditorGeometry(QWidget *editor,
     const QStyleOptionViewItem &option, const QModelIndex &index) const override;
     QString displayText(const QVariant &, const QLocale &) const override;

   private:
      int spinMin;
      int spinMax;
};

#if 0
// custom class for a combo in a cell
class synCombo : public QStyledItemDelegate
{
   Q_OBJECT 
   public:
      synCombo(QObject *parent=0);
      virtual ~synCombo();
   private slots:
      void valChg();
   protected:
     QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const override;
     void setEditorData(QWidget *editor, const QModelIndex &index) const override;
     void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
     void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const override;
   private:
      mutable bool chg = false;
};
#endif


// custom class for a line edit in a cell
class synEdit : public QStyledItemDelegate
{
   Q_OBJECT 
   public:
      synEdit(QObject *parent=0,bool roMode = false);
      virtual ~synEdit();


   protected:
     QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const override;
     void setEditorData(QWidget *editor, const QModelIndex &index) const override;
     void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
     void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const override;
   private:
      bool readOnly;
};


class SimScene;
class synModel;

// each row in the treeview is one of these
class synItem : QObject
{
   Q_OBJECT
   friend SimScene;
   friend synModel;

   public:
     explicit synItem(synRec& rec, synItem *parentItem = 0);
    ~synItem();

    synItem *child(int row);
    synItem *parent();
    int childCount() const;
    int childNumber() const;
    int columnCount() const;
    int synType() { return synItemData.rec[SYN_TYPE].toInt();};
    int synId() { return synItemData.rec[SYN_NUM].toInt();};
    bool insertChild(int position, synRec& rec);
    bool insertChildren(int position, int count, synRow& rows);
    bool removeChild(int position);
    bool removeChildren(int position, int count);
    QVariant data(int column) const;
    bool setData(int column, const QVariant&);
    synItem *parentItem();

   private:
     std::list<synItem*> childItems;
     synRec synItemData;
     synItem *parItem;
};


// Manages the build synapse treeview.
class synModel : public QAbstractItemModel
{
   Q_OBJECT
   friend synEdit;
   friend synSpin;
   friend synIntSpin;

   public:
      synModel(QObject *parent);
      virtual ~synModel();
      bool addRow(synRec&,QModelIndex&);
      bool readRec(synRec&,const QModelIndex&);
      int  numRecs() { if (rootItem) return rootItem->childCount(); else return 0;};
      void delRow(int, const QModelIndex&);
      void reset();
      void prepareUndo();
      void undo();
      void clearUndo();
      bool insertRows(int, int, const QModelIndex &parent = QModelIndex()) override;
      bool removeRows(int, int, const QModelIndex &parent = QModelIndex()) override;
      int rowCount(const QModelIndex &parent = QModelIndex()) const override;
      int columnCount(const QModelIndex &parent = QModelIndex()) const override;
      QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
      bool setData(const QModelIndex & index, const QVariant & value, int role = Qt::EditRole) override;
      QVariant headerData(int section,Qt::Orientation orientation, int role) const override;
      Qt::ItemFlags flags(const QModelIndex&) const override;
      QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
      QModelIndex parent(const QModelIndex &) const override;
      synItem* getItem(const QModelIndex&) const;
      void recycleSynapses(const QModelIndex&);
      bool hasPre(const QModelIndex&);
      bool hasPost(const QModelIndex&);
      QModelIndex idToIndex(int);

   protected:

   private:
      synItem *rootItem = nullptr;
      S_NODE dbCopy;
};


#endif
