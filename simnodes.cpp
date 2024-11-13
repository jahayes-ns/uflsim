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



// various graphical objects we display in the scene
#include "simscene.h"
#include <QCursor>

using namespace std;

// Fiber and Cell share a lot, this is the common stuff
BaseNode::BaseNode(QGraphicsItem *parent) : QGraphicsItem(parent)
{
}

BaseNode::~BaseNode()
{
}

void BaseNode::commonInit()
{
   pop = new QGraphicsSimpleTextItem(this);
   comment = new QGraphicsSimpleTextItem(this);
   pop->setFlags(QGraphicsItem::ItemStacksBehindParent);
   comment->setFlags(QGraphicsItem::ItemStacksBehindParent);
   itemFont = QFont("Arial",7);
   itemFont.setLetterSpacing(QFont::AbsoluteSpacing,1);
   fontInfo = QFontMetrics(itemFont);
}


void BaseNode::updateText(QString& txt1, QString& txt2)
{
   QBrush brush;
   QPen pen;
   QRect rect = boundingRect().toRect();
   QRect fit;
   int h;

   SimScene *par = dynamic_cast<SimScene *>(scene());
   h = fontInfo.height()/2;
   brush.setColor(par->getFG());
   brush.setStyle(Qt::SolidPattern);
   pop->setBrush(brush);
   pop->setFont(itemFont);
   fit = fontInfo.boundingRect(rect,Qt::AlignHCenter | Qt::AlignVCenter,txt1);
   pop->setPos(fit.x(),fit.y());
   pop->setText(txt1);
   comment->setBrush(brush);
   comment->setFont(itemFont);
   fit = fontInfo.boundingRect(rect,Qt::AlignHCenter | Qt::AlignBottom,txt2);
   comment->setPos(fit.x(), fit.y()+h);
   comment->setText(txt2);
}

void BaseNode::mouseMoveEvent(QGraphicsSceneMouseEvent *me)
{
   me->accept();
   QPointF pt = me->scenePos();
   QPointF prev = me->lastScenePos();
   qreal dx, dy;
   dx = pt.x() - prev.x();
   dy = pt.y() - prev.y();
   SimScene *par = dynamic_cast<SimScene *>(scene());
   if (par)
      par->sceneMove();

   QRectF sourceRect;
   QList<QGraphicsItem *> sel = scene()->selectedItems();
   if (sel.size() > 0)
   {
      prepareGeometryChange();
      foreach(QGraphicsItem *current, sel)
        sourceRect = sourceRect.united(current->sceneBoundingRect());
      if (sourceRect.x() <= SCRN_LO_OFF)
         dx = 1;
      else if (sourceRect.x() >= SCRN_HI_OFF)
         dx = -1;
      if (sourceRect.y() <= SCRN_LO_OFF)
         dy = 1;
      else if (sourceRect.y() >= SCRN_HI_OFF)
         dy = -1;
      foreach(QGraphicsItem *current, sel)
      {
         Connector *conn = dynamic_cast <Connector *>(current);
         if (!conn)
            current->moveBy(dx,dy);
         else
            conn->mouseMove(dx,dy);
      }
      QPainterPath limit; 
      limit.addRect(scene()->sceneRect()); // keep mouse pointer inside scene
      if (!limit.contains(sourceRect))
      {
         QPointF mpt = scene()->views().first()->viewport()->mapToGlobal(prev.toPoint());
         QCursor::setPos(mpt.x(),mpt.y());
      }
   }
}

QVariant BaseNode::itemChange(GraphicsItemChange change, const QVariant &value)
{
   if (!scene())
      return value;
   if (change == QGraphicsItem::ItemPositionChange)
   {
      // value is the new position.
      QPointF newPos = value.toPointF();
      QPointF oldPos = pos();
      qreal dx, dy;
      dx = newPos.x() - oldPos.x();
      dy = newPos.y() - oldPos.y();
      for (connIter iter = fromLines.begin(); iter != fromLines.end(); ++iter)
         (*iter)->moveStart(dx,dy);
      for (connIter iter = toLines.begin(); iter != toLines.end(); ++iter)
         (*iter)->moveEnd(dx,dy);
      scene()->invalidate(scene()->sceneRect());
      return newPos;
   }
   return QGraphicsItem::itemChange(change, value);
}


// if we have this item ptr in our connector from/to arrays, remove it
void BaseNode::removeLines(Conn &lines)
{
   prepareGeometryChange();
   for (connIter a_line = lines.begin(); a_line != lines.end(); ++a_line)
   {
      for (connIter iter = fromLines.begin(); iter != fromLines.end(); )
         if (*iter == *a_line)
            iter = fromLines.erase(iter);
         else
            ++iter;
      for (connIter iter = toLines.begin(); iter != toLines.end(); )
         if (*iter == *a_line)
            iter = toLines.erase(iter);
         else
            ++iter;
   }
}

void BaseNode::hoverLeaveEvent(QGraphicsSceneHoverEvent * /*ev*/)
{
   auto par = dynamic_cast<SimWin*>(scene()->parent());
   if (par->getConnMode() == connMode::START || par->getConnMode() == connMode::DRAWING)
      setSelected(false);
}


// FIBER
FiberNode::FiberNode(QGraphicsItem *parent) : BaseNode(parent)
{
   commonInit();
}

FiberNode::FiberNode(const QRectF &rect,QGraphicsItem *parent) : BaseNode(parent)
{
   commonInit();
   baseRect = rect;
}

FiberNode::~FiberNode()
{
   prepareGeometryChange();
}

QPainterPath FiberNode::shape() const
{
   QRectF fiber = boundingRect();
   QPainterPath path;
   path.addRect(fiber);
   return path;
}

QRectF FiberNode::boundingRect() const
{
   QRectF fiber = rect();
   qreal x,y,w,h;
   x = fiber.x();
   y = fiber.y();
   w = fiber.width();
   h = fiber.height();
   fiber.setX(x-HOT_ZONE);
   fiber.setY(y-HOT_ZONE);
   fiber.setHeight(h+HOT_ZONE*2);
   fiber.setWidth(w+HOT_ZONE*2);
   return fiber;
}

void FiberNode::paint(QPainter *painter, const QStyleOptionGraphicsItem * /* option */ , QWidget * /*widget*/ )
{
   QRectF cell = rect();
   SimScene *par = dynamic_cast<SimScene *>(scene());
   QColor fg = par->getFG();
   QBrush brush(fg,Qt::NoBrush);
   QPen pen(brush,NODE_WIDTH);
   pen.setColor(fg);
   painter->setPen(pen);
   painter->drawRect(cell);
   F_NODE* fiber = &D.inode[d_idx].unode.fiber_node;
   if (fiber->pop_subtype == AFFERENT)
   {
      cell = cell.adjusted(-3,-3,3,3);
      painter->drawRect(cell);
   }
   brush.setColor(fg);
   brush.setStyle(Qt::SolidPattern);
   pop->setBrush(brush);
   comment->setBrush(brush);
   if (isSelected())
   {
      QRectF dash = boundingRect();
      SimScene *par = dynamic_cast<SimScene *>(scene());
      if (par->isMono())
         painter->setPen(QPen(fg, 1, Qt::DashLine));
      else
         painter->setPen(QPen(Qt::green, 1, Qt::DashLine));
      painter->drawRect(dash);
   }
}

void FiberNode::hoverEnterEvent(QGraphicsSceneHoverEvent* /* ev */)
{
   auto par = dynamic_cast<SimWin*>(scene()->parent());
   if (par->getConnMode() == connMode::START)
      setSelected(true);
}


// CELL
CellNode::CellNode(QGraphicsItem *parent) : BaseNode(parent)
{
   commonInit();
}

CellNode::CellNode(const QRectF &rect,QGraphicsItem *parent) : BaseNode(parent)
{
   baseRect = rect;
   commonInit();
}

CellNode::CellNode(qreal x, qreal y, qreal w, qreal h,QGraphicsItem *parent) : BaseNode(parent)
{
   baseRect = QRectF(x,y,w,h);
   commonInit();
}

CellNode::~CellNode()
{
   prepareGeometryChange();
}

bool CellNode::contains(const QPointF& pt) const
{
   QPainterPath limit = shape();
   return limit.contains(pt);
}

QPainterPath CellNode::shape() const
{
   QRectF cell = boundingRect();
   QPainterPath path;
   path.addEllipse(cell);
   return path;
}

QRectF CellNode::boundingRect() const
{
   QRectF cell = rect();
   qreal x,y,w,h;
   x = cell.x();
   y = cell.y();
   w = cell.width();
   h = cell.height();
   cell.setX(x-HOT_ZONE);
   cell.setY(y-HOT_ZONE);
   cell.setHeight(h+HOT_ZONE*2);
   cell.setWidth(w+HOT_ZONE*2);
   return cell;
}

void CellNode::paint(QPainter *painter, const QStyleOptionGraphicsItem * /* option */ , QWidget * /*widget*/ )
{
   QRectF cell = rect();
   SimScene *par = dynamic_cast<SimScene *>(scene());
   QColor fg = par->getFG();
   QBrush brush(fg,Qt::NoBrush);
   QPen pen(brush,NODE_WIDTH);
   pen.setColor(fg);
   painter->setPen(pen);
   painter->drawEllipse(cell);
   brush.setColor(fg);
   brush.setStyle(Qt::SolidPattern);
   pop->setBrush(brush);
   comment->setBrush(brush);
   if (isSelected())
   {
      QRectF dash = boundingRect();
      SimScene *par = dynamic_cast<SimScene *>(scene());
      if (par->isMono())
         painter->setPen(QPen(fg, 1, Qt::DashLine));
      else
         painter->setPen(QPen(Qt::green, 1, Qt::DashLine));
      painter->drawEllipse(dash);
   }
}

void CellNode::hoverEnterEvent(QGraphicsSceneHoverEvent * /*ev*/ )
{
   auto par = dynamic_cast<SimWin*>(scene()->parent());
   if (par->getConnMode() == connMode::START || par->getConnMode() == connMode::DRAWING)
      setSelected(true);
}

// CONNECTOR
Connector::Connector(QPointF start, QGraphicsItem* parent) : QGraphicsItem(parent)
{
   connPts.push_back(start);
}

Connector::~Connector()
{
   prepareGeometryChange();
}

// We have to move connector lines since they are marked not
// movable. This is required because we want to keep the end pts
// attached to fibers & cells.
void Connector::mouseMove(qreal dx, qreal dy)
{
   for (unsigned int idx = 1; idx < connPts.size()-1; ++idx)
   {
      prepareGeometryChange();
      auto iter = connPts.begin();
      std::advance(iter, idx);
      QPointF mid= *iter;
      mid.setX(mid.x() + dx);
      mid.setY(mid.y() + dy);
      *iter = mid;
   }
}

// are we picking all the line segments, or just a vertex?
void Connector::mousePressEvent(QGraphicsSceneMouseEvent *me)
{
   QPointF pt = me->scenePos();
   singleIdx = -1;
   int idx = 0;
   for (lineSegsIter iter = connPts.begin(); iter != connPts.end(); ++iter,++idx)
   {
      if (abs(pt.x() - iter->x()) < LINE_OFF && (abs(pt.y() - iter->y()) < LINE_OFF))
      {
         singleIdx = idx;
         filterMouse=false;
         break;
      }
   }
   QGraphicsItem::mousePressEvent(me);
}

void Connector::mouseReleaseEvent(QGraphicsSceneMouseEvent *me)
{
   singleIdx = -1;
   prepareGeometryChange();
   calcShape();
   QGraphicsItem::mouseReleaseEvent(me);
}

bool Connector::sceneEvent(QEvent* evt) 
{
   return QGraphicsItem::sceneEvent(evt);
}

void Connector::mouseMoveEvent(QGraphicsSceneMouseEvent *me)
{
   QPainterPath limit;
   if (connPts.size() < 2)  // at least 2 pts to draw a line
      return;
   SimScene *par = dynamic_cast<SimScene *>(scene());
   if (par)
      par->sceneMove();

   QPointF pt = me->scenePos();
   QPointF prev = me->lastScenePos();
   QPointF pos = me->pos();
   qreal dx, dy;
   dx = pt.x() - prev.x();
   dy = pt.y() - prev.y();

   QRectF sourceRect;
   QList<QGraphicsItem *> sel = scene()->selectedItems();
   foreach(QGraphicsItem *current, sel) // build bbox for all items
      sourceRect = sourceRect.united(current->sceneBoundingRect());

   if (sourceRect.x() <= SCRN_LO_OFF) // if moved outside, move back in
      dx = 1;
   else if (sourceRect.x() >= SCRN_HI_OFF)
      dx = -1;
   if (sourceRect.y() <= SCRN_LO_OFF)
      dy = 1;
   else if (sourceRect.y() >= SCRN_HI_OFF)
      dy = -1;

   prepareGeometryChange();
   foreach(QGraphicsItem *current, sel)
   {
      Connector *conn = dynamic_cast <Connector *>(current);
      if (!conn)
         current->moveBy(dx,dy);
      else if (conn != this)
         conn->mouseMove(dx,dy);
   }

    // If click near end of a segment, move just one segment, including
    // ends.  The lines-on-top must be the state to get hits on the end
    // points because they are usually behind the nodes.
   if (singleIdx >=0)   // single move, which segment?
   {
      auto iter = connPts.begin();
      std::advance(iter, singleIdx);
      QPointF mid=*iter;
      QPointF newpt(mid.x() + dx,mid.y() + dy);
      QPointF itemPos;

      if (singleIdx == 0)
      {
         limit = start->shape();
         newpt = mapToItem(start,newpt);
         itemPos = mapToItem(start,pos);
      }
      else if (singleIdx == int(connPts.size()-1))
      {
         limit = end->shape();
         newpt = mapToItem(end,newpt);
         itemPos = mapToItem(end,pos);
      }
      else
         limit.addRect(scene()->sceneRect());

      if (limit.contains(newpt) && !filterMouse) // Okay to move the end point
      {
         mid.setX(mid.x() + dx);
         mid.setY(mid.y() + dy);
         *iter= mid;
      }
//else
//cout << " line move: out of bounds " << newpt.x() << " " << newpt.y() << " " << itemPos.x() << " " << itemPos.y() << " inout: " << limit.contains(itemPos) << endl;


      if (!limit.contains(itemPos) && !filterMouse) // Don't let the mouse pointer wander
      {                                             // away as long as the button is down
               // mapToGlobal does not seem to take the viewport scaling into
               // account, just translations. When not zoomed, scene and
               // viewport transform the same, if scaled up or down, they are
               // different.
          QTransform viewt = scene()->views().first()->viewportTransform();
          QPointF tran = viewt.map(mid);
          QPointF newpos = scene()->views().first()->viewport()->mapToGlobal(tran.toPoint());
          QCursor::setPos(newpos.x(),newpos.y()); // move mouse pointer
//cout << " force mouse: " << tran.x() << " " << tran.y() << endl;
           // when we tell the OS to move the mouse pointer, it generates a new mouse
           // move event, which then causes us to keep moving the mouse for reasons
           // I don't totally understand. This tells the is-move-in-bounds check above
           // to ignore the next mouse event.
         filterMouse = true;
      }
      else
         filterMouse = false;
   }
   else
   {    // move all segments
      for (unsigned int idx = 1; idx < connPts.size()-1; ++idx)
      {
         auto iter = connPts.begin();
         std::advance(iter, idx);
         QPointF mid=*iter;
         mid.setX(mid.x() + dx);
         mid.setY(mid.y() + dy);
         *iter=mid;
      }
      limit.addRect(scene()->sceneRect());
      if (!limit.contains(sourceRect))
      {
         QPointF keeppos = me->lastScenePos();
         QPointF mpt = scene()->views().first()->viewport()->mapToGlobal(keeppos.toPoint());
         QCursor::setPos(mpt.x(),mpt.y());
//cout << "*** setpos\n";
      }
   }
   calcShape();
   scene()->invalidate(scene()->sceneRect());
}

// This trusts the caller not to over-fill the pt array
void Connector::addPt(QPointF next)
{
#if 0
   // Why was I doing this? pts too close means duplicate at least one
   // maybe both coordinates. Doesn't this mean two points have same
   // coordinates? Maybe the bounding rectangles for the points were wrong (???)
   if (connPts.size() > 0)
   {
      auto iter = connPts.rbegin();
      QPointF prevPt = *iter;
      if ( abs(prevPt.x() - next.x()) < 12)
         next.setX(prevPt.x());
      if ( abs(prevPt.y() - next.y()) < 12)
         next.setY(prevPt.y());
   }
#endif
   connPts.push_back(next);
   prepareGeometryChange();
   shape();
}

// It makes things unhappy if you have duplicate adjacent points 
// assumes the new pt will be added after current end
bool Connector::dupPt(QPointF pt)
{
   bool ret = false;
   lineSegsIter iter = connPts.begin();
   std::advance(iter, connPts.size()-1);

   if (*iter == pt)
   {
      cout << "adjacent points identical" << endl;
      ret = true;
   }
   return ret;
}

void Connector::moveStart(qreal dx,qreal dy)
{
   if (connPts.size() >= 2)
   {
      auto iter = connPts.begin();
      if (iter != connPts.end())
      {
         QPointF start=*iter;
         start.setX(start.x() + dx);
         start.setY(start.y() + dy);
         *iter = start;
         prepareGeometryChange();
         calcShape();
      }
      else
         cout << "Start moving a connector without any points?" << endl;
   }
}

void Connector::moveEnd(qreal dx,qreal dy)
{
   if (connPts.size() >= 2)
   {
      auto iter = connPts.rbegin();
      if (iter != connPts.rend())
      {
         QPointF end=*iter;
         end.setX(end.x() + dx);
         end.setY(end.y() + dy);
         *iter = end;
         prepareGeometryChange();
         calcShape();
      }
      else
         cout << "End moving a connector without any points?" << endl;
   }
}

QRectF Connector::boundingRect() const
{
   qreal min_x = 650000;
   qreal min_y = 650000;
   qreal max_x = -1;
   qreal max_y = -1;
   QRect bound;

   if (connPts.size()  > 1)
   {
      for (auto pt : connPts)
      {
         if (pt.x() < min_x)
            min_x = pt.x();
         if (pt.x() > max_x)
            max_x = pt.x();
         if (pt.y() < min_y)
            min_y = pt.y();
         if (pt.y() > max_y)
            max_y = pt.y();
         min_x -= LINE_OFF;
         min_y -= LINE_OFF;
         max_x += LINE_OFF;
         max_y += LINE_OFF;
         bound.setX(min_x);
         bound.setY(min_y);
         bound.setWidth(max_x-min_x);
         bound.setHeight(max_y-min_y);
      }
   }
   return bound;
}


// Select the pre-created bounding poly for this connector.
QPainterPath Connector::shape() const
{
   QPainterPath path;
   path.addPolygon(boundingPoly);
   return path;
}

// The shape around a line is a rectangle enclosing the line
// The ends on connected line segments overlap, giving the user
// a small region to click on to select a single connection point to drag
void Connector::calcShape()
{
   QPointF start,end,vec,ortho;
   boundingPoly.clear();
   double dx, dy, dist;

   for (auto iter = connPts.begin(); iter != std::prev(connPts.end()); ++iter)
   {
      start=*iter;
      end=*std::next(iter,1);
      vec=end-start;
      dy = end.y()-start.y();
      dx = end.x()-start.x();
      if (dx == 0 && dy == 0) // duplicate point, skip it
         continue;
      dist = sqrt((dx*dx) + (dy*dy));
      vec=vec*(LINE_OFF/dist);
      ortho = QPoint(vec.y(),-vec.x());
      boundingPoly << start-vec+ortho;
      boundingPoly << start-vec-ortho;
      boundingPoly << end+vec-ortho;
      boundingPoly << end+vec+ortho;
   }
   for (auto iter = connPts.rbegin(); iter != std::prev(connPts.rend()); ++iter)
   {
      start=*std::next(iter,1);
      end=*iter;
      QPointF vec=end-start;
      dy = end.y()-start.y();
      dx = end.x()-start.x();
      if (dx == 0 && dy == 0) // duplicate point, skip it
         continue;
      dist = sqrt((dx*dx) + (dy*dy));
      vec=vec*(LINE_OFF/dist);
      ortho = QPointF(vec.y(),-vec.x());
      boundingPoly << end+vec-ortho;
      boundingPoly << end+vec+ortho;
      boundingPoly << start-vec+ortho;
      boundingPoly << start-vec-ortho;
   }
}

void Connector::paint(QPainter *painter, const QStyleOptionGraphicsItem * /*option*/, QWidget* /*widget */)
{
   SimScene *par = dynamic_cast<SimScene *>(scene());
   QColor fg, ra, bg;
   bool mono = par->isMono();
   if (mono)
   {
      ra = fg = par->getFG();
   }
   else
   {
      fg = color;
      ra = Qt::blue;
   }
   bg = par->getBG();

   if (connPts.size() > 1)
   {
      QBrush brush(fg,Qt::NoBrush);

      QPen pen(brush,NODE_WIDTH);
      if (showAsSrcTarg == connDir::RETRO)
      {
         pen.setWidth(4.0);
         if (mono)
         {
            pen.setColor(fg);
            brush.setColor(fg);
         }
         else
         {
            pen.setColor(QColor(255,164,0));
            brush.setColor(QColor(255,164,0));
         }
         setZValue(onTopZ);  // force to top
      }
      else if (showAsSrcTarg == connDir::ANTERO)
      {
         pen.setWidth(4.0);
         if (mono)
         {
            pen.setColor(fg);
            brush.setColor(fg);
         }
         else
         {
            pen.setColor(QColor(255,255,128));
            brush.setColor(QColor(255,255,128));
         }
         setZValue(onTopZ);
      }
      else
      {
         if (showingRA)
         {
            brush.setColor(ra);
            pen.setColor(ra);
         }
         else
         {
            brush.setColor(fg);
            pen.setColor(fg);
         }
         setZValue(connZ);
      }
      painter->setPen(pen);
      painter->setBrush(brush);
      vector<QPointF> vec;
      vec.reserve(connPts.size());
      std::copy(connPts.begin(),connPts.end(),back_inserter(vec));
      painter->drawPolyline(vec.data(),vec.size());
      if (drawCircle)
      {
         brush.setStyle(Qt::SolidPattern);
         pen.setStyle(Qt::SolidLine);
         painter->setPen(pen);
         painter->setBrush(brush);
         auto iter = connPts.rbegin();
         QPointF end = *iter;

         painter->drawEllipse(end.x()-END_OFF,end.y()-END_OFF,END_CIRCLE,END_CIRCLE);
         pen.setColor(bg);
         painter->setPen(pen);
         painter->drawLine(end.x()-4,end.y(),end.x()+4,end.y());
         if (excititory)
            painter->drawLine(end.x(),end.y()-4,end.x(),end.y()+4);
      }
      if (isSelected())
      {
         brush.setStyle(Qt::NoBrush);
         painter->setBrush(brush);
         QPen pen(par->getFG(),1);
         pen.setCosmetic(true);  // this really speeds up rubberbanding
         painter->setPen(pen);
         painter->drawPolygon(boundingPoly);
      }
   }
}

