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

#ifndef SIMSCENE_H
#define SIMSCENE_H

#include <iostream>
#include <vector>
#include <array>
#include <map>
#include <set>

#include <QPoint>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QGraphicsEllipseItem>
#include <QPainter>
#include <QPainterPath>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsItemGroup>
#include <QFont>
#include <QFontMetrics>
#include <QtMath>
#include <QKeyEvent>
#include <QLabel>
#include "colormap.h"
#include "simnodes.h"
#include "selectaxonsyn.h"
#include "build_model.h"
#include "synview.h"


using remapTarg = std::map<int,int>;
using remapIter = remapTarg::iterator;
using exportObjs = std::set<int>;
using exportObjsIter = exportObjs::iterator;
using graphicsItemList = std::list <QGraphicsItem*>;

class SimWin;
class SimScene;

// This is a dummy parent class that lets us group items and operate on them as
// a group, such as zorder or show/hide.
class QGraphicsItemLayer : public QGraphicsItem
{
   public:
      QRectF boundingRect() const {return QRectF(0,0,0,0);}
      void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *) {}
};


// Not currently used. This is a trick to delay a delete operation on a
// graphics object by wrapping it in a QObject. There are cases where you
// cannot delete an item inside an event hander, this defers that action 
// until the next event loop processing.
template <typename T>
class DelayedDelete : public QObject
{
   public:
      explicit DelayedDelete(T *&item) : item(item)
      {
         deleteLater();
      }
      virtual ~DelayedDelete()
      {
         delete item;
      }
private:
        T *item;
};

// lists don't have a contains function, make one
template <typename T>
bool listContains(std::list<T> & itemList, const T & item)
{
	auto it = std::find(itemList.begin(), itemList.end(), item);
	return it != itemList.end();
}

class SimScene : public QGraphicsScene
{
   Q_OBJECT
   friend SimWin;
   friend BaseNode;
   friend Connector;
   friend synIntSpin;
   friend synSpin;

   public:
      explicit SimScene(SimWin *parent=0);
      virtual ~SimScene();
      QColor getBG(){return sceneBG;}
      QColor getFG(){return sceneFG;}
      bool   isMono();

   protected:
      QColor sceneFG=defSceneFG;
      QColor sceneBG=defSceneBG;
      void removePath();
      void deleteSelected();
      int snapTo(int pt) {int rem = pt % SNAP_NODE; if (rem == 0) return pt; else return (pt-rem); }
      void synapseTypeChg();
      void showCellRetro(bool);
      void showCellAntero(bool);
      void showFiberTarg(bool);
      void toggleLines(bool);
      void selAll();
      void deSelAll();
      void drawFile();
      void fiberViewSendToLauncherBdt();
      void fiberViewSendToLauncherPlot();
      void cellViewSendToLauncherBdt();
      void cellViewSendToLauncherPlot();
      void clear();
      void cellChkDirty();
      void setCellDirty(bool);
      void setFiberDirty(bool);
      void setSynapseDirty(bool);
      void setBuildDirty(bool);
      void setGlobalsDirty(bool);
      void setFileDirty(bool);
      void setSendControls();
      void cellApply();
      void cellUndo();
      void fiberChkDirty();
      void cellRecToUi(int, int sel = -1);
      void fiberApply();
      void fiberUndo();
      void fiberRecToUi(int, int sel = -1);
      void synapseApply();
      void synapseUndo();
      void synapseChkDirty();
      void addSynType();
      void delSynType();
      void buildApply();
      void buildUndo();
      void buildChkDirty();
      void globalsApply();
      void globalsUndo();
      void globalsRecToUi();
      void globalsChkDirty();
      void sceneMove();
      bool fileChkDirty();
      bool isFileDirty() { return fileDirty;}
      void resetModels();
      void initControls();
      bool allChkDirty();
      void chkControlsDirty();
      void clearAllDirty();
      void setPrePost();
      void saveSnd();
      void addSynPre();
      void addSynPost();
      void addSynLearn();
      void createGroup();
      void importGroup();
      void exportGroup();
      void selGroup();
      void unGroup();
      void fiberTypeFixed();
      void fiberTypeFuzzy();
      void centerOn(int);

      void mousePressEvent(QGraphicsSceneMouseEvent *mouseEvent) override;
      void mouseMoveEvent(QGraphicsSceneMouseEvent *mouseEvent) override;
      void mouseReleaseEvent(QGraphicsSceneMouseEvent *mouseEvent) override;
      void keyReleaseEvent(QKeyEvent *evt) override;
      void addFiber(QPointF,int,bool fromfile=false);
      void addCell(QPointF,int,bool fromfile=false);
      void addConnector(QPointF,int, bool, int idx=0, bool fromfile=false);
      void addSynapse(Connector&);
      void deleteSynapse(Connector&);
      void mngTabs();
      void fiberUiToRec();
      void cellUiToRec();
      void synapseRecToUi(connRec&);
      void synapseUiToRec();
      void buildUiToRec();
      void buildRecToUi();
      void globalsUiToRec();
      void setRAMode(bool);
      void axonSynChoices(QGraphicsSceneMouseEvent *me);
      void coordsToRec();
      void outOfNodes();
      int chkSynInUse(int);
      void deleteSynType(int);
      int countSynapses();
      void updateInfoText();
      void buildRowChg(const QModelIndex &, const QModelIndex &);
      void buildChg(const QModelIndex &, const QModelIndex &);
      void updateExcite();
      void selectGroupChk();
      void connInGroup(graphicsItemList&);
      void importGroupChk(QGraphicsSceneMouseEvent *);
      void saveSubsystem(inode_global&);
      void remapObjs(graphicsItemList&);
      void getExpTargs(graphicsItemList&);
      void exportFiber(FiberNode*, inode_global&);
      void exportCell(CellNode*, inode_global&);
      void exportSynapses(inode_global&);
      void exportGlobals(inode_global&);
      void moveImport(QGraphicsSceneMouseEvent *);
      void addImport();
      void drawImport();
      BaseNode* getObjNode(int);
      void doMonochrome(bool);

   public slots:
      void axonSynSel(connRec&);
      void buildDataChg(const QModelIndex &, const QModelIndex &);
      void synSelected(const QModelIndex &); 

   private:
      SimWin *par=nullptr;
      Connector *currConn = nullptr;
      selectAxonSyn *axonSynDlg;
      QFont itemFont;
      QFontMetrics fontInfo=QFontMetrics(itemFont);
      const std::array<QColor,4> lineColor={{Qt::red,Qt::yellow,Qt::green,Qt::blue}};
      QGraphicsItemLayer *lineParent;
      QGraphicsSimpleTextItem *globalText;
      QLabel *importLabel;
      inode_global *importModel = nullptr;
      const int nodeOrder = nodeZ;
      int connOrder = connZ;
      bool moving=false;
      connRec selectedConn;
      bool cellDirty=false;
      bool fiberDirty=false;
      bool synapseDirty=false;
      bool buildDirty=false;
      bool globalsDirty=false;
      bool fileDirty=false;
      bool showGroup=false;
      bool waitForImportClick=false;
      synModel *synBuildModel=nullptr;
      synEdit *synNum=nullptr;
      synEdit *synName=nullptr;
      synEdit* synType=nullptr; 
      synSpin *synEqPot=nullptr;
      synSpin *synTc=nullptr;
      synIntSpin *synWindow=nullptr;
      synSpin *synLrnDelta=nullptr;
      synSpin *synLrnMax=nullptr;
};

#endif // SIMSCENE_H
