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



// Functions used for drawing a model from a file.

#include <stdlib.h>
#include "simwin.h"
#include "simscene.h"
#include "sim2build.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "simulator.h"
#include "inode.h"
#ifdef __cplusplus
}
#endif

#include "c_globals.h"
#include "sim2build.h"

template <typename T>
typename std::enable_if<std::is_unsigned<T>::value, int>::type
inline constexpr signum(T const x) {
    return T(0) < x;  
}

template <typename T>
typename std::enable_if<std::is_signed<T>::value, int>::type
inline constexpr signum(T const x) {
    return (T(0) < x) - (x < T(0));  
}

using namespace std;


// We have a connector end point that is not inside the hot zone of a cell/fiber.
// Given the center of the object and the endpoint, find a point that is inside
// the hotzone and return it.
QPointF movePt(double cx, double cy, QPointF& pt)
{
   QPointF ret;
   double ax,ay,bx,by;
   double m;
   int dist = ITEM_CENTER;
   if (cx - pt.x() == 0) // vertical
   {
      bx = ax = cx;
      ay = cy + dist;
      by = cy - dist;
   }
   else
   {
      m = (cy-pt.y())/(cx-pt.x());
      if (m == 0) // horizontal
      {
         ax = cx + dist;
         bx = cx - dist;
         by = ay = cy;
      }
      else
      {
         double dx,dy;
         dx = dist/sqrt(1+(m*m));
         dy = m * dx;
         ax = cx + dx;
         ay = cy + dy;
         bx = cx - dx;
         by = cy - dy;
      }
   }
   // pick shortest from connector endpoint, both are same distance from center
   double d0,d1;
   d0 = sqrt(pow(pt.y()-ay,2) + pow(pt.x()-ax,2));
   d1 = sqrt(pow(pt.y()-by,2) + pow(pt.x()-bx,2));
   if (d0 > d1)
   {
      ret.setX(bx);
      ret.setY(by);
   }
   else
   {
      ret.setX(ax);
      ret.setY(ay);
   }
   return ret;
}



/* The D array has been read in from a file. Draw it.
   We do this by using the (x,y) locations of graphics objects to generate fake
   mouse clicks and let the draw functions do the work as if a user was
   creating a model by hand.  Do all fiber and cell nodes first, then draw the
   connectors by simulating clicks on the nodes.
*/
void SimScene::drawFile()
{
   int src = 0, tar = 0;
   int locx, locy;
   int synapse_type = 0;
   int target_num = 0;
   int coord = 0, node_num;
   bool excite = 0;
   QString txt1, txt2;
   QColor axonColor;
   QPointF pt;
   AXON *axon;
   F_NODE* f;
   C_NODE* c;
   QTextStream strm1(&txt1);
   QTextStream strm2(&txt2);

   clear();
   setGlobalsDirty(false);
   par->ui->simView->unZoom();

   auto fixFiberStart = [&] {
      QPointF adjustPt = movePt(f->fiber_x, f->fiber_y, pt); // f current from node
      axon->x_coord[0] = adjustPt.x();
      axon->y_coord[0] = adjustPt.y();
      addConnector(adjustPt,synapse_type,excite,tar,true);
      QString msg = "WARNING: The connector start position for fiber population " + QString::number(D.inode[src].node_number) + " missed the node. The start location has been moved closer to the node.";
      par->printMsg(msg);
      setFileDirty(true);
   };
   auto fixCellStart = [&] {
      QPointF adjustPt = movePt(c->cell_x, c->cell_y, pt); // c current from node
      axon->x_coord[0] = adjustPt.x();
      axon->y_coord[0] = adjustPt.y();
      addConnector(adjustPt,synapse_type,excite,tar,true);
      QString msg = "WARNING: The connector start position for cell population " + QString::number(D.inode[src].node_number) + " missed the node. The start location has been moved to be closer to the node.";
      par->printMsg(msg);
      setFileDirty(true);
   };
   auto fixCellEnd = [&] {
      int src = findTargetIdx(target_num);
      C_NODE *c_end = &D.inode[src].unode.cell_node;
      QPointF adjustPt = movePt(c_end->cell_x, c_end->cell_y, pt);
      axon->x_coord[coord] = adjustPt.x(); // axon current c or f pt list
      axon->y_coord[coord] = adjustPt.y();
      ++axon->num_coords;
      addConnector(adjustPt,synapse_type,excite,tar,true);
      QString msg = "WARNING: The connector end position for target cell population " + QString::number(target_num) + " missed the node. The end location has been moved to be closer to the node.";
      par->printMsg(msg);
      setFileDirty(true);
   };

    // draw all cells & fibers first so we can fake mouse clicks on them.
   if (D.inode[GLOBAL_INODE].node_type == GLOBAL)
   {
      globalsRecToUi();
      updateInfoText();
   }
   for (src = FIRST_INODE; src <= LAST_INODE; ++src)   // 1 - 98 are possible fibers/cells
   {
      txt1.clear();
      txt2.clear();
      if (D.inode[src].node_type == FIBER)
      {
         f = &D.inode[src].unode.fiber_node;
         locx = f->fiber_x;
         locy = f->fiber_y;
         node_num = D.inode[src].node_number;
         fibersInUse[node_num] = true;
         strm1 << "Pop" << node_num << endl;
         strm1 << f->f_pop << endl;
         strm1 << D.inode[src].unode.fiber_node.f_prob;
         strm2 << D.inode[src].comment1;
         strm2 << "\n" << D.inode[src].unode.fiber_node.group;
         pt.rx() = locx;
         pt.ry() = locy;
         addFiber(pt,src,true);
      }
      else if (D.inode[src].node_type == CELL)
      {
         c = &D.inode[src].unode.cell_node;
         locx = c->cell_x;
         locy = c->cell_y;
         node_num = D.inode[src].node_number;
         cellsInUse[node_num] = true;
         strm1 << "Pop" << node_num << endl;
         strm1 << c->c_pop << endl;
         strm2 << D.inode[src].comment1;
         strm2 << "\n" << D.inode[src].unode.cell_node.group;
         pt.rx() = locx;
         pt.ry() = locy;
         addCell(pt,src,true);
       }
   }

      // fake line clicks to add connectors
   S_NODE *s = &D.inode[SYNAPSE_INODE].unode.synapse_node;
   for (src = FIRST_INODE; src <=LAST_INODE; ++src) // for each possible node
   {
      if (D.inode[src].node_type == FIBER)
      {
         f = &D.inode[src].unode.fiber_node;
         for (tar = 0; tar <= LAST_INODE; tar++)
         {
            target_num = f->f_target_nums[tar];
            if (target_num == 0)
               continue;
            synapse_type = f->f_synapse_type[tar];
            excite = s->s_eq_potential[synapse_type] > 0;
            axon = &f->fiber_axon[tar];
            if (axon->num_coords)
            {
               par->setConnMode(connMode::START);
               for (coord = 0 ; coord < axon->num_coords; ++coord)
               {
                  pt.rx() = axon->x_coord[coord];
                  pt.ry() = axon->y_coord[coord];
                  addConnector(pt,synapse_type,excite,tar,true);
                  // This can happen in some hard-to-duplicate instances,
                  // usually a result of moving an endpoint jussssst outside
                  // of a node's shape.  source:
                  if (coord == 0 && par->getConnMode() == connMode::START)
                     fixFiberStart();
               }
                // same thing, ran out of lines, didn't hit target
               if (coord && par->getConnMode() == connMode::DRAWING)
                  fixCellEnd();
            }
         }
      }
      else if (D.inode[src].node_type == CELL)
      {
         c = &D.inode[src].unode.cell_node;
         for (tar = 0; tar <= LAST_INODE; tar++)
         {
            target_num = c->c_target_nums[tar];
            if (target_num == 0)
               continue;
            synapse_type = c->c_synapse_type[tar];
            excite = s->s_eq_potential[synapse_type] > 0;
            axon = &c->cell_axon[tar];
            if (axon->num_coords)
            {
               par->setConnMode(connMode::START);
               for (coord = 0 ; coord < axon->num_coords; ++coord)
               {
                  pt.setX(axon->x_coord[coord]);
                  pt.setY(axon->y_coord[coord]);
                  addConnector(pt,synapse_type,excite,tar,true);
                  if (coord == 0 && par->getConnMode() == connMode::START)
                     fixCellStart();
               }
               if (coord && par->getConnMode() == connMode::DRAWING)
                  fixCellEnd();
            }
         }
      }
      else
         continue;
   }
}


// We don't try to keep cell, fiber, and line segment numbers current in the
// D array in real time.  When it is time to save info to a file, we scan the
// graphic objects and put the current graphical info into the D array.
// We also do not keep the axon coord lists up to date, so zero them out.
// If a synapse/connector is in the drawing, we will pick it up when we
// scan the objects.

void SimScene::coordsToRec()
{
   int d_idx,offset, targ_idx;
   FiberNode *fiber;
   CellNode* cell;
   Connector* conn;
   F_NODE *fnode;
   C_NODE *cnode;
   lineSegsIter iter;

   for (int node = FIRST_INODE; node <= LAST_INODE; ++node) 
      if (D.inode[node].node_type == CELL)
         memset(&D.inode[node].unode.cell_node.cell_axon,0,sizeof(D.inode[node].unode.cell_node.cell_axon));
      else if (D.inode[node].node_type == FIBER)
         memset(&D.inode[node].unode.fiber_node.fiber_axon,0,sizeof(D.inode[node].unode.fiber_node.fiber_axon));

   auto list = items();
   QPointF pt1;
   QPoint  pt2;
   foreach (QGraphicsItem *item, list)
   {
      fiber = dynamic_cast <FiberNode *>(item);
      cell = dynamic_cast <CellNode *>(item);
      conn = dynamic_cast <Connector *>(item);
      if (fiber)
      {
         d_idx = fiber->getDidx();
         pt1 = fiber->pos();
         pt2 = (pt1 + fiber->sceneOrg).toPoint();
         D.inode[d_idx].unode.fiber_node.fiber_x = pt2.x();
         D.inode[d_idx].unode.fiber_node.fiber_y = pt2.y();
      }
      else if (cell)
      {
         d_idx = cell->getDidx();
         pt1 = cell->pos();
         pt2 = (pt1 + cell->sceneOrg).toPoint();
         D.inode[d_idx].unode.cell_node.cell_x = pt2.x();
         D.inode[d_idx].unode.cell_node.cell_y = pt2.y();
      }
      else if (conn)
      {
         fiber = dynamic_cast <FiberNode *>(conn->start);
         cell = dynamic_cast <CellNode *>(conn->start);
         targ_idx = conn->getTargIdx();
         if (fiber)
         {
            d_idx = fiber->getDidx();
            fnode = &D.inode[d_idx].unode.fiber_node;
            AXON* faxon = &fnode->fiber_axon[targ_idx];
            faxon->num_coords = conn->connPts.size();
            for (iter = conn->connPts.begin(), offset = 0; iter != conn->connPts.end(); ++iter,++offset)
            {
               faxon->x_coord[offset] = iter->toPoint().x();
               faxon->y_coord[offset] = iter->toPoint().y();
            }
         }
         else
         {
            d_idx = cell->getDidx();
            cnode = &D.inode[d_idx].unode.cell_node;
            AXON* caxon = &cnode->cell_axon[targ_idx];
            caxon->num_coords = conn->connPts.size();
            for (iter = conn->connPts.begin(),offset = 0; iter != conn->connPts.end(); ++iter,++offset)
            {
               caxon->x_coord[offset] = iter->toPoint().x();
               caxon->y_coord[offset] = iter->toPoint().y();
            }
         }
      }
        // else not an item to be saved
   }
}

