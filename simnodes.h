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


#ifndef SIMNODES_H
#define SIMNODES_H

#include <vector>


const int ITEM_SIZE = 41;
const int ITEM_CENTER = (ITEM_SIZE/2);
const int NODE_WIDTH = 2.0;
const int LINE_WIDTH = 2.0;
const int SNAP_DIST = 20.0;  // straight line tolerence
const int SNAP_NODE = 10;    // nodes
const int HOT_ZONE = 12;
const int MAX_PTS = 16;      // conform with newsned
const int LINE_OFF = 8;

// size values from .c code
const int END_OFF = 5;  // circle at end of line offset from end pt.
const int END_CIRCLE=11;

const int SCENE_SIZE = 4000;
const int SCRN_LO_OFF = -HOT_ZONE;    // you can come this close to the edge of screen
const int SCRN_HI_OFF = SCENE_SIZE-SCRN_LO_OFF;
const double SCENE_CENTER = SCENE_SIZE/2.0;

const QColor defSceneBG=QColor(20,20,20,255);
const QColor defSceneFG=Qt::white;

const int nodeZ = 2;   // by default, nodes  in front
const int connZ = 1;   // connectors behind
const int textZ = 0;   // text behind
const int onTopZ = 10; // REALLY on top

enum class connMode {OFF,START,DRAWING,CANCEL};
enum class connDir {NONE,RETRO,ANTERO};

#include "simwin.h"

class Connector;
class SimScene;

using lineSegs = std::list <QPointF>;
using lineSegsIter = lineSegs::iterator;

using Conn = std::vector <Connector *>;
using connIter = Conn::iterator;

using synDrawColors = std::vector <QColor>;

const synDrawColors synColors = { Qt::green, Qt::red, Qt::blue, Qt::yellow, Qt::magenta, Qt::cyan, Qt::white, Qt::gray, QColor(255,80,13) };

class Connector : public QGraphicsItem
{
   friend SimScene;

   public:
      explicit Connector()=delete;
      explicit Connector(QPointF, QGraphicsItem* parent=0);
      virtual ~Connector();
      void addPt(QPointF);
      void moveStart(qreal dx,qreal dy); 
      void moveEnd(qreal dx,qreal dy); 
      int numPts() {return connPts.size();}
      void addCell() {drawCircle = true;}
      void setSynType(int s_type) {synapseType = s_type;}
      int getSynType() {return synapseType;}
      void setId(int id) {objId = id;}
      int getId() {return objId;}
      void setTargIdx(int idx) {targIdx = idx;}
      int getTargIdx() {return targIdx;}
      void setColor(QColor c) {color = c;}
      void setExcite(bool ex) {excititory = ex;}
      void setSrcTarg(connDir state) {showAsSrcTarg = state;}
      void mouseMove(qreal,qreal);
      void setHaveBound(bool);
      void calcShape();
      void setStartItem(QGraphicsItem* item) {start = item;};
      void setEndItem(QGraphicsItem* item) {end = item;};
      void setRAMode(bool mode) { showingRA = mode;};
      bool dupPt(QPointF);

   protected:
      QRectF boundingRect() const override;
      QPainterPath shape() const override;
      void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override ;
      void mouseMoveEvent(QGraphicsSceneMouseEvent *) override;
      void mousePressEvent(QGraphicsSceneMouseEvent *mouseEvent) override;
      void mouseReleaseEvent(QGraphicsSceneMouseEvent *mouseEvent) override;
      void allLinesToColor(QColor);
      bool sceneEvent(QEvent*) override;

   private:
      lineSegs connPts;
      bool drawCircle = false;
      int excititory = true;
      int synapseType;
      int singleIdx = -1;
      QColor color;
      connDir showAsSrcTarg = connDir::NONE;
      bool showingRA=false;
      mutable QPolygonF boundingPoly;
      int objId;
      int targIdx;
      bool filterMouse=false;
      QGraphicsItem *start = nullptr;
      QGraphicsItem *end = nullptr;
};



// This contains what is common to both cells and fibers
class BaseNode : public QGraphicsItem
{
   friend SimScene;

    // Q_OBJECT  // if you need signals/slots
   public:
      explicit BaseNode(QGraphicsItem *parent=0);
      virtual ~BaseNode();
      void setDidx(int idx) {d_idx = idx;}  // global D array index
      int getDidx() {return d_idx;}
      void setPop(int pop) {popNum = pop;}
      int getPop() {return popNum;}
      void removeLines(Conn&);
      void updateText(QString&,QString&);

   protected:
      virtual QPainterPath shape() const override = 0;
      virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override = 0;
      virtual QRectF boundingRect() const override = 0;
      QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
      virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *ev) override = 0;
      void hoverLeaveEvent(QGraphicsSceneHoverEvent *ev) override;
      void mouseMoveEvent(QGraphicsSceneMouseEvent *mouseEvent) override;
      void commonInit();
      virtual QRectF rect() const = 0;
      virtual void setRect(QRectF&) = 0;
      QGraphicsSimpleTextItem *pop,*comment;
      int d_idx;
      int popNum;

   private:
      Conn fromLines;
      Conn toLines;
      QPointF sceneOrg;
      QFont itemFont;
      QFontMetrics fontInfo=QFontMetrics(itemFont);
};


class FiberNode : public BaseNode 
{
   public:
      explicit FiberNode(QGraphicsItem *parent=0);
      explicit FiberNode(const QRectF &rect,QGraphicsItem *parent = 0);
      virtual ~FiberNode();

   protected:
      QPainterPath shape() const override;
      void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override ;
      QRectF boundingRect() const override;
      virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *ev) override;
      QRectF rect() const override { return baseRect;};
      void setRect(QRectF& r) override  { baseRect = r;};

   private:
      QRectF baseRect;

};

class CellNode : public BaseNode 
{
   friend SimScene;
   public:
      explicit CellNode(QGraphicsItem *parent=0);
      explicit CellNode(const QRectF &rect,QGraphicsItem *parent = 0);
      explicit CellNode(qreal x, qreal y,qreal width, qreal height,QGraphicsItem *parent = 0);
      virtual ~CellNode();

   protected:
      QPainterPath shape() const override;
      void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override ;
      QRectF boundingRect() const override;
      virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *ev) override;
      QRectF rect() const override { return baseRect;};
      void setRect(QRectF& r) override  { baseRect = r;};
      bool contains(const QPointF&) const override;

   private:
      QRectF baseRect;
};

#endif // SIMNODES_H
