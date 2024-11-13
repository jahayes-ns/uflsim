#ifndef AFFMODEL_H
#define AFFMODEL_H

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
#include <QItemDelegate>
#include <QLineEdit>
#include <QValidator>
#include <QMimeData>
#include <QDoubleSpinBox>

// column offsets 
const int AFF_VAL=0;
const int AFF_PROB=1;
const int AFF_NUM_COL = AFF_PROB+1;

// The model 
struct affRec { QVariant rec[AFF_NUM_COL]; };

using affRows = std::list<affRec>;
using affRowsIter = affRows::iterator;
using affRowsConstIter = affRows::const_iterator;

/*
class affDropData : public QMimeData
{
   public:
      affDropData() {};
      ~affDropData() {};
      affRec one_rec;
};
*/

class affModel : public QAbstractTableModel
{
   Q_OBJECT

   public:
      affModel(QObject *parent);
      virtual ~affModel();
      void addRow(affRec&, int row=-1);
      bool readRec(affRec&,int);
      bool updateRec(affRec&,int);
      int  numRecs() { return affData.size();}
      void delRow(int);
      void sortRows() {sort(0,Qt::AscendingOrder);}
      void reset();

   signals:
      void notifyChg(bool);

   protected:
      void sort(int,Qt::SortOrder) override;
      bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

   private:
      int rowCount(const QModelIndex &parent = QModelIndex()) const override;
      int columnCount(const QModelIndex &parent = QModelIndex()) const override;
      QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
      bool setData(const QModelIndex & index, const QVariant & value, int role = Qt::EditRole) override;
      QVariant headerData(int section,Qt::Orientation orientation, int role) const override;
      Qt::ItemFlags flags(const QModelIndex&) const override;
      static bool compareRows(affRec&, affRec&);

      affRows affData;
};


// for afferent table
class affSrcDelegate : public QItemDelegate
{
   public:
      QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const override;
};

class affProbDelegate : public QItemDelegate
{
   public:
      QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const override;
};
#endif

