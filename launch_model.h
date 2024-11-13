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

#ifndef LAUNCH_MODEL_H
#define LAUNCH_MODEL_H

#include <QAbstractTableModel>
#include <QItemDelegate>
#include <QStyledItemDelegate>
#include <QStandardItemModel>
#include <QDataWidgetMapper>
#include <QComboBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QStringList>
#include <QMimeData>
#include <list>
#include "common_def.h"


// column offsets of bdt table
const int BDT_CELL_FIB=0;
const int BDT_POP=1;
const int BDT_MEMB=2;
const int BDT_TYPE=3;
const int BDT_NUM_COL = BDT_TYPE+1;
const int BDT_VISABLE_NUM_COL = BDT_MEMB+1;

// column offsets of plot table
const int PLOT_CELL_FIB=0;
const int PLOT_POP=1;
const int PLOT_MEMB=2;
const int PLOT_TYPE=3;
const int PLOT_BINWID=4;
const int PLOT_SCALE=5;
const int PLOT_NUM_COL=PLOT_SCALE+1;

// widget offsets for analog widgets
const int ANALOG_ID=0;
const int ANALOG_CELLPOP=1;
const int ANALOG_RATE=2;
const int ANALOG_TIME=3;
const int ANALOG_SCALE=4;
const int ANALOG_P1CODE=5;
const int ANALOG_P1START=6;
const int ANALOG_P1STOP=7;
const int ANALOG_P2CODE=8;
const int ANALOG_P2START=9;
const int ANALOG_P2STOP=10;
const int ANALOG_IE_SMOOTH=11;
const int ANALOG_IE_SAMPLE=12;
const int ANALOG_IE_PLUS=13;
const int ANALOG_IE_MINUS=14;
const int ANALOG_IE_FREQ=15;
const int ANALOG_NUM_COL = ANALOG_IE_FREQ+1;

// widget offset for hostname widget
const int HOSTNAME_ID=0;
const int HOSTNAME_NUM_COL = HOSTNAME_ID+1;

const QStringList stdCombo(
      {
        "Membrane Potential",
        "K Conductance",
        "Threshold",
        "Instantaneous Pop Activity",
        "Use Bin Width and Scale ==>"
       });

const QStringList bursterCombo(
      {
        "Membrane Potential",
        "Hybrid IF H value",
        "Threshold",
        "Instantaneous Pop Activity",
        "Use Bin Width and Scale ==>"
       });


// the numeric ids for these are -1 to -16
// so  id is -(comboidx+1) since the combo index starts at 0
const QStringList lungCombo(
      {
        "Lung volume",
        "Tracheal flow",
        "Alveolar pressure",
        "Diaphragm activation",
        "Abdominal muscle activation",
        "Net laryngeal muscle activation",
        "Diaphragm volume",
        "Abdominal volume",
        "Derivative of Diphram volume",
        "Derivative of Abdominal volume",
        "Transdiaphragmatic pressure",
        "Abdominal pressure",
        "Transpulmonary pressure",
        "Diaphragm activation limited, 0 → 1",
        "Abdominal muscle activation limited, 0 → 1",
        "Net laryngeal muscle activation limited, −1 → 1"
      });

const QStringList afferentCombo(
      {
        "Events",
        "Signal",
        "Events And Signal",
        "Instantaneous Pop Activity",
        "Use Bin Width and Scale ==>"
      });

const QString fiberPlotType("Event");

struct plotRec 
{
   QVariant rec[PLOT_NUM_COL];
   int maxRndVal;
   CellType popType;
};

using plotRow = std::list<plotRec>;
using plotRowIter = plotRow::iterator;
using plotRowConstIter = plotRow::const_iterator;

using plotPages = std::vector<plotRow>;
using plotPagesIter = plotPages::iterator;
using plotPagesConstIter = plotPages::const_iterator;

struct bdtRec 
{
   QVariant rec[BDT_NUM_COL];
   int maxRndVal;
};

using bdtRow = std::list<bdtRec>;
using bdtRowIter = bdtRow::iterator;
using bdtRowConstIter = bdtRow::const_iterator;

using bdtPages = std::vector<bdtRow>;
using bdtPagesIter = bdtPages::iterator;
using bdtPagesConstIter = bdtPages::const_iterator;

struct analogRec
{
   QVariant rec[ANALOG_NUM_COL];
};

class analogWidgetModel : public QAbstractTableModel
{
   Q_OBJECT
   
   public:
      analogWidgetModel(QObject *parent);
      virtual ~analogWidgetModel();
      void setCurrentModel(int curr) { currModel = curr;};
      int  numRecs() {return analogData.size();}
      void dupModel(int,int);
      void addRec(analogRec&,int);
      bool readRec(analogRec&,int);
      void reset();

   signals:
      void notifyChg(bool);

   private:
      int rowCount(const QModelIndex &parent = QModelIndex()) const  override;
      int columnCount(const QModelIndex &parent = QModelIndex()) const override;
      QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
      bool setData(const QModelIndex & index, const QVariant & value, int role = Qt::EditRole) override;

      std::vector<analogRec> analogData;
      int currModel = 0;
};

struct hostNameRec
{
   QVariant rec;
};

class hostNameWidgetModel : public QAbstractTableModel
{
   Q_OBJECT
   
   public:
      hostNameWidgetModel(QObject *parent);
      virtual ~hostNameWidgetModel();
      void setCurrentModel(int curr) { currModel = curr;};
      int  numRecs() {return hostNameData.size();}
      void dupModel(int,int);
      void addRec(hostNameRec&,int);
      bool readRec(hostNameRec&,int);
      void reset();

   signals:
      void notifyChg(bool);

   private:
      int rowCount(const QModelIndex &parent = QModelIndex()) const  override;
      int columnCount(const QModelIndex &parent = QModelIndex()) const override;
      QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
      bool setData(const QModelIndex & index, const QVariant & value, int role = Qt::EditRole) override;

      std::vector<hostNameRec> hostNameData;
      int currModel = 0;
};

// custom class for a spinbox in a cell
class plotSpin : public QStyledItemDelegate
{
   Q_OBJECT

   public:
      plotSpin(QObject *parent = 0);
      virtual ~plotSpin();

    protected:
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override;
    void updateEditorGeometry(QWidget *editor,
        const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};



// custom class for a combo in a cell
class plotCombo : public QStyledItemDelegate
{
   Q_OBJECT 

   public:
      plotCombo(QObject *parent=0);
      virtual ~plotCombo();

   protected:
     QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const override;
     void setEditorData(QWidget *editor, const QModelIndex &index) const override;
     void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
     void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const override;

};


// custom class for a line edit in a cell
class plotEdit : public QStyledItemDelegate
{
   friend plotCombo;

   Q_OBJECT 

   public:
      plotEdit(QObject *parent=0);
      virtual ~plotEdit();

   protected:
     QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const override;
     void setEditorData(QWidget *editor, const QModelIndex &index) const override;
     void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
     void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const override;

};



class plotDropData : public QMimeData
{
   public:
      plotDropData() {};
      ~plotDropData() {};
      plotRec one_rec;
};

// Manages the launch window plot desktop tables This is a table of tables, each
// one a launch/instance number.
class plotModel : public QAbstractTableModel
{
   Q_OBJECT
   friend plotEdit;
   friend plotCombo;
   friend plotSpin;

   public:
      plotModel(QObject *parent);
      virtual ~plotModel();
      void addRow(plotRec&, int row=-1);
      bool readRec(plotRec&,int);
      bool updateRec(plotRec&,int);
      int  numRecs() { return plotData[currModel].size();}
      void delRow(int);
      void dupModel(int,int);
      void setCurrentModel(int curr) { currModel = curr;};
      void reset();
      void updHeader(const QModelIndex& curr);

   signals:
      void notifyChg(bool);

   protected:
      void sort(int,Qt::SortOrder) override;
      Qt::DropActions supportedDropActions() const override;
      bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) override;
      bool canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) const override;
      QMimeData* mimeData(const QModelIndexList &indexes) const override;
      QStringList mimeTypes() const override;
      bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

   private:
      int rowCount(const QModelIndex &parent = QModelIndex()) const override;
      int columnCount(const QModelIndex &parent = QModelIndex()) const override;
      QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
      bool setData(const QModelIndex & index, const QVariant & value, int role = Qt::EditRole) override;
      QVariant headerData(int section,Qt::Orientation orientation, int role) const override;
      Qt::ItemFlags flags(const QModelIndex&) const override;
      int currModel = 0;
      mutable QString colFour= "Bin Width (ms)";
      mutable QString colFive = "Bin Scaling";
      mutable plotPages plotData;
};

class bdtDropData : public QMimeData
{
   public:
      bdtDropData() {};
      ~bdtDropData() {};
      bdtRec one_rec;
};


// manages the bdt window table
class bdtModel : public QAbstractTableModel
{
   Q_OBJECT

   friend  plotEdit;

   public:
      bdtModel(QObject *parent);
      virtual ~bdtModel();
      void addRow(bdtRec&, int row=-1);
      void delRow(int);
      bool readRec(bdtRec&,int);
      bool updateRec(bdtRec&,int);
      int  numRecs() { return bdtData[currModel].size();}
      void dupModel(int,int);
      void setCurrentModel(int curr) { currModel = curr;};
      void reset();

   signals:
      void notifyChg(bool);

   protected:
      void sort(int,Qt::SortOrder) override;
      Qt::DropActions supportedDropActions() const override;
      bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) override;
      bool canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) const override;
      QMimeData* mimeData(const QModelIndexList &indexes) const override;
      QStringList mimeTypes() const override;
      bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

   private:
      int rowCount(const QModelIndex &parent = QModelIndex()) const override;
      int columnCount(const QModelIndex &parent = QModelIndex()) const override;
      QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
      bool setData(const QModelIndex & index, const QVariant & value, int role = Qt::EditRole) override;
      QVariant headerData(int section,Qt::Orientation orientation, int role) const override;
      Qt::ItemFlags flags(const QModelIndex&) const override;
      int currModel = 0;

      bdtPages bdtData;
};


#endif


