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


/* Custom graphics scene.
   This is logically just an extension of the main parent window.  This knows
   about the graphical objects, the window owns the controls, but info about
   both are needed to show/edit data. So, it directly manages the data entry
   controls rather than have a zillion call-back to parent window functions. This
   makes keeping the graphics and info about them in sync a lot easier.
*/

#include <stdlib.h>
#include <QString>
#include <QTextStream>
#include <QMessageBox>
#include <QTreeView>
#include <QItemSelectionModel>
#include <QModelIndex>
#include <QToolTip>
#include <QLabel>
#include <qgraphicsscene.h>
#include <list>
#include <memory>
#include "simwin.h"
#include "simscene.h"

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

using namespace std;

SimScene::SimScene(SimWin *parent) : QGraphicsScene(parent)
{
   par = parent;
   itemFont = QFont("Arial",8);
   itemFont.setLetterSpacing(QFont::AbsoluteSpacing,1);
   setFont(itemFont);
   fontInfo = QFontMetrics(itemFont);

   // dialog for selecting which axon when lines are stacked on top of each other
   SimWin* mainwin=dynamic_cast <SimWin *>(parent);
   axonSynDlg = new selectAxonSyn(mainwin);

       // make sure there is always one of each of these.
   lineParent = new QGraphicsItemLayer;       // all lines are it's children
   globalText = new QGraphicsSimpleTextItem;  // text in UL corner
   addItem(lineParent);
   addItem(globalText);

   synBuildModel = new synModel(this);
   synNum = new synEdit(this);
   synName = new synEdit(this);
   synType = new synEdit(this,true);
   synEqPot = new synSpin(this);
   synTc = new synSpin(this);
   synWindow = new synIntSpin(this,1,2000);
   synLrnDelta = new synSpin(this);
   synLrnMax = new synSpin(this);

   par->ui->buildSynapsesView->setModel(synBuildModel);
   par->ui->buildSynapsesView->setItemDelegateForColumn(SYN_NUM ,synNum);
   par->ui->buildSynapsesView->setItemDelegateForColumn(SYN_NAME ,synName);
   par->ui->buildSynapsesView->setItemDelegateForColumn(SYN_TYPE ,synType);
   par->ui->buildSynapsesView->setItemDelegateForColumn(SYN_EQPOT,synEqPot);
   par->ui->buildSynapsesView->setItemDelegateForColumn(SYN_TC,synTc);
   par->ui->buildSynapsesView->setItemDelegateForColumn(SYN_LRN_WINDOW,synWindow);
   par->ui->buildSynapsesView->setItemDelegateForColumn(SYN_LRN_DELTA,synLrnDelta);
   par->ui->buildSynapsesView->setItemDelegateForColumn(SYN_LRN_MAX,synLrnMax);
   par->ui->buildSynapsesView->setAcceptDrops(false);
   par->ui->buildSynapsesView->expandAll();
   par->ui->buildSynapsesView->setColumnHidden(SYN_PARENT,true);
   par->ui->buildSynapsesView->header()->setDefaultAlignment(Qt::AlignCenter);
   connect(par->ui->buildSynapsesView->selectionModel(),
           &QItemSelectionModel::currentRowChanged, this, &SimScene::buildRowChg);
   connect(synBuildModel, &QAbstractItemModel::dataChanged, this, &SimScene::buildChg);

   par->ui->synapseList->setModel(synBuildModel);
#if 0
   par->ui->synapseList->hideColumn(SYN_EQPOT);
   par->ui->synapseList->hideColumn(SYN_TC);
   par->ui->synapseList->hideColumn(SYN_LRN_WINDOW);
   par->ui->synapseList->hideColumn(SYN_LRN_DELTA);
   par->ui->synapseList->hideColumn(SYN_LRN_MAX);
#endif
   par->ui->synapseList->hideColumn(SYN_PARENT);
   par->ui->buildSynapsesView->setAcceptDrops(false);
   connect(par->ui->synapseList->selectionModel(),
           &QItemSelectionModel::currentRowChanged, this, &SimScene::synSelected);
   par->ui->synapseList->expandAll();
   par->ui->synapseList->setCurrentIndex(QModelIndex());

   importLabel = new QLabel(QString(""),par,Qt::ToolTip);
   importLabel->setText("Click on where you want the upper-left corner\nof the imported group to be.");

   doMonochrome(par->showMono);

// sanity check to make sure c and c++ agree on size of these things
// printf("D: is %ld  inode is: %ld  %ld  %ld  %ld  %ld %ld\n", sizeof(D), sizeof(D.inode[GLOBAL_INODE]),
//      sizeof(GLOBAL_NETWORK), sizeof(C_NODE), sizeof(F_NODE), sizeof(S_NODE),
//      sizeof(U_NODE));

}

SimScene::~SimScene()
{

}


void SimScene::clear()
{
   QGraphicsScene::clear();  // remove all items
      // make sure there is always one of these
   lineParent = new QGraphicsItemLayer;
   globalText = new QGraphicsSimpleTextItem;  // text in UL corner
   addItem(lineParent);
   addItem(globalText);
   QPen pen(Qt::green);
   par->setConnMode(connMode::OFF);
   currConn = nullptr;
   updateInfoText();
}

// User has selected an axon/synapse to view/edit
// from the popup dialog shown when lines are coincident
// The selected item is the one on top. Hide it and show
// the selection from the popup.
void SimScene::axonSynSel(connRec& rec)
{
   QModelIndex idx = par->ui->synapseList->indexFromType(rec.lineitem->synapseType);
   if (idx.isValid())
   {
      par->ui->synapseList->scrollTo(idx,QAbstractItemView::PositionAtCenter);
      par->ui->synapseList->setCurrentIndex(idx);
   }
   auto list = selectedItems();
   foreach (QGraphicsItem *item, list)
      item->setSelected(false);
   rec.lineitem->setSelected(true);
}

void SimScene::buildDataChg(const QModelIndex &/*topLeft*/, const QModelIndex &/*botright*/)
{
   setBuildDirty(true);
}

// signal from view, new row selected
void SimScene::synSelected(const QModelIndex & /* idx*/ )
{
   synapseTypeChg();
}

void SimScene::keyReleaseEvent(QKeyEvent *evt)
{
   switch(evt->key())
   {
      default:
         QGraphicsScene::keyPressEvent(evt);
         break;
   }
}

// Click on a line. It can be one line, or a bunch of lines on top
// of each other.  If so, create a list to let user pick which connector
// they want. If just one, skip that. In both cases,  show params.
void SimScene::axonSynChoices(QGraphicsSceneMouseEvent *me)
{
   bool have_conn = false;
   int pop, d_idx, num_choices = 0;
   connRec one_conn, ret_rec;

   QPointF pt = me->scenePos();

   axonSynDlg->enableSort(false);
   axonSynDlg->newList();
   QList<QGraphicsItem*> undermouse = items(pt);
   for (int hit = 0; hit < undermouse.size(); ++hit)
   {
      Connector* conn = dynamic_cast<Connector*>(undermouse[hit]);
      if (conn)
      {
         FiberNode *fiber_start = dynamic_cast <FiberNode *>(conn->start);
         FiberNode *fiber_end = dynamic_cast <FiberNode *>(conn->end);
         CellNode* cell_start = dynamic_cast <CellNode *>(conn->start);
         CellNode* cell_end = dynamic_cast <CellNode *>(conn->end);

         one_conn.synType = D.inode[SYNAPSE_INODE].unode.synapse_node.synapse_name[conn->synapseType];
            // this assumes only 1 start and stop
         if (fiber_start)
         {
            d_idx = fiber_start->getDidx();
            one_conn.source_num = QString("%1").arg(d_idx);
            pop =D.inode[d_idx].node_number;
            one_conn.axonPop = QString("F Pop %1").arg(pop);
            one_conn.axonComment = D.inode[d_idx].comment1;
            one_conn.mode = QString("%1").arg(FIBER);
         }
         if (fiber_end)  // this should never happen
         {
            cout << "Fiber at end? Huh?" << endl;
         }
         if (cell_start)
         {
            d_idx = cell_start->getDidx();
            one_conn.source_num  = QString("%1").arg(d_idx);
            pop                  = D.inode[d_idx].node_number;
            one_conn.axonPop     = QString("C Pop %1").arg(pop);
            one_conn.mode        = QString("%1").arg(CELL);
            one_conn.axonComment = D.inode[d_idx].comment1;
         }
         if (cell_end)
         {
            d_idx               = cell_end->getDidx();
            pop                 = D.inode[d_idx].node_number;
            one_conn.target_num = QString("%1").arg(pop);
            one_conn.synPop     = QString("C Pop %1").arg(pop);
            one_conn.synComment = D.inode[d_idx].comment1;
         }
         one_conn.lineitem=conn;
         axonSynDlg->addRec(one_conn);
         have_conn = true;
         ++num_choices;
      }
   }
   axonSynDlg->enableSort(true);
   if (have_conn)
   {
      synapseChkDirty();
      if (num_choices > 1 && !moving)   // pick from list
      {
         deSelAll();  // may have hilighted a line, turn it off
         axonSynDlg->sortBy(0);
         QPoint moveleft = par->ui->selAll->mapToGlobal(par->ui->selAll->pos());
         axonSynDlg->move(moveleft);
         axonSynDlg->exec();
         one_conn = axonSynDlg->getRec();
         if (one_conn.source_num.length()) // may be no selection
         {
            axonSynSel(one_conn);
            selectedConn = one_conn;
            synapseRecToUi(one_conn);
         }
         else
            selectedConn.lineitem = nullptr;
      }
      else
      {
         selectedConn = one_conn;
         if (!one_conn.lineitem->isSelected())
            selectedConn.lineitem = nullptr;
         synapseRecToUi(one_conn);      // update if only 1 even if moving
      }
   }
}


// if left mouse click, what to do?
void SimScene::mousePressEvent(QGraphicsSceneMouseEvent *me)
{
   moving = false; // Keep track of clicking on connector to view params,
                   // and just moving lines.
    // any click turns off any show src/target line coloring
   setRAMode(false);
   auto list = items();
   for (auto item = list.cbegin() ; item != list.cend(); ++item)
   {
      Connector* conn = dynamic_cast <Connector *>(*item);
      if (conn)
         conn->setSrcTarg(connDir::NONE);
   }
   invalidate(sceneRect());

   if (QGuiApplication::keyboardModifiers() & Qt::ShiftModifier)
   {
      par->ui->simView->setDragMode(QGraphicsView::ScrollHandDrag);
      return;
   }
   QPointF pt = me->scenePos();
   if (!par->addFiberToggle && !par->addCellToggle && par->connectorMode == connMode::OFF)
   {
      QGraphicsScene::mousePressEvent(me);
      return;
   }

   if (me->button() != Qt::LeftButton)
   {
      QGraphicsScene::mousePressEvent(me);
      return;
   }

   if (par->addFiberToggle)
   {
      int id =  get_node(FIBER);
      updateInfoText();
      if (id == -1) // oops, no more to get
      {
         par->addFiberOff();
         outOfNodes();
         return;
      }
      D.inode[id].unode.fiber_node = def_fiber.unode.fiber_node; // default vals
      addFiber(pt,id);
      if (par->oneShotAddFlag)
         par->addFiberOff();
   }
   else if (par->addCellToggle)
   {
      int id =  get_node(CELL);
      updateInfoText();
      if (id == -1) // oops, no more to get
      {
         par->addCellOff();
         outOfNodes();
         return;
      }
      D.inode[id].unode.cell_node = def_cell.unode.cell_node;
      addCell(pt,id);
      if (par->oneShotAddFlag)
         par->addCellOff();
   }
   else if (par->getConnMode() != connMode::OFF)
   {
      QModelIndex syn_row = par->ui->synapseList->currentIndex();
      int syn_type = 1;
      if (syn_row.row()>=0)
         syn_type = syn_row.sibling(syn_row.row(),SYN_NUM).data().toInt();
      else
         par->ui->synapseList->setCurrentIndex(QModelIndex());
      bool excite = D.inode[SYNAPSE_INODE].unode.synapse_node.s_eq_potential[syn_type] > 0;
      addConnector(pt,syn_type,excite); // maybe start/add line
   }
   else
     QGraphicsScene::mousePressEvent(me);
}

void SimScene::addFiber(QPointF pt, int d_idx, bool fromfile)
{
   QString txt1, txt2;
   QTextStream strm1(&txt1);
   QTextStream strm2(&txt2);
   int popnum;
   F_NODE *fiberdata;

   fiberdata = &D.inode[d_idx].unode.fiber_node;
   popnum = D.inode[d_idx].node_number;

   strm1 << "Pop" << popnum << endl
         << fiberdata->f_pop << endl;
   if (fiberdata->pop_subtype == ELECTRIC_STIM)
      strm1 << "E Stim";
   else if (fiberdata->pop_subtype == AFFERENT)
      strm1 << " AFF ";
   else
      strm1 << fiberdata->f_prob;
   strm2 << D.inode[d_idx].comment1;

   if (!fromfile)
   {
      pt.setX(snapTo(pt.x()));
      pt.setY(snapTo(pt.y()));
      setFileDirty(true);
   }
   fiberdata->fiber_x = pt.x();
   fiberdata->fiber_y = pt.y();
   QRectF fiber(pt.x()-ITEM_CENTER,pt.y()-ITEM_CENTER,ITEM_SIZE,ITEM_SIZE);
   FiberNode *rect = new FiberNode(fiber);
   rect->sceneOrg = pt;
   rect->setFlag(QGraphicsItem::ItemIsSelectable, true);
   rect->setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
   rect->setFiltersChildEvents(true);
   rect->setAcceptHoverEvents(true);
   rect->setDidx(d_idx);
   rect->setPop(popnum);
   rect->setZValue(nodeOrder);
   addItem(rect);
   rect->updateText(txt1,txt2);
   par->buildFindList();
}

// Create a cell item and add it. The cell item is selectable. The
// text items are children of the cell and are not selected.
// Passed: point in scene, index of ths item in the D array,
// and  a flag to indicate if the user is creating this or it
// being created from a file.
void SimScene::addCell(QPointF pt, int d_idx, bool fromfile)
{
   QString txt1, txt2;
   QTextStream strm1(&txt1);
   QTextStream strm2(&txt2);
   int popnum;
   C_NODE *celldata;

   celldata = &(D.inode[d_idx].unode.cell_node);
   popnum = D.inode[d_idx].node_number;
   strm1 << "Pop" << popnum << endl;
   strm1 << celldata->c_pop;
   strm2 << D.inode[d_idx].comment1;

   if (!fromfile)
   {
      pt.setX(snapTo(pt.x()));
      pt.setY(snapTo(pt.y()));
      setFileDirty(true);
   }
   celldata->cell_x = pt.x();
   celldata->cell_y = pt.y();

   CellNode *cell = new CellNode(pt.x()-ITEM_SIZE/2, pt.y()-ITEM_SIZE/2,ITEM_SIZE,ITEM_SIZE);
   cell->sceneOrg = pt;
   cell->setFlag(QGraphicsItem::ItemIsSelectable, true);
   cell->setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
   cell->setAcceptHoverEvents(true);
   cell->setFiltersChildEvents(true); // ignore clicks on text
   cell->setZValue(nodeOrder);
   cell->setDidx(d_idx);
   cell->setPop(popnum);
   addItem(cell);
   cell->updateText(txt1,txt2);
   par->buildFindList();
}

void SimScene::addConnector(QPointF pt, int synType, bool excite, int targidx, bool fromFile)
{
   QGraphicsItem *item = itemAt(pt,QTransform());
   FiberNode *fiber_item = dynamic_cast <FiberNode *>(item);
   CellNode* cell_item = dynamic_cast <CellNode *>(item);
   QGraphicsSimpleTextItem *text_item = dynamic_cast <QGraphicsSimpleTextItem *>(item);
   QList<QGraphicsItem*> ontop = items(pt,Qt::ContainsItemBoundingRect);

    // Floating point coords cause errors reading in a file because the file has ints.
    // Round the points so the fraction is always 0.
   pt.setX(round(pt.x()));
   pt.setY(round(pt.y()));

   if (text_item) // if text inside item, use item
   {
      fiber_item = dynamic_cast <FiberNode *>(text_item->parentItem());
      cell_item = dynamic_cast <CellNode *>(text_item->parentItem());
      if (fiber_item && !fiber_item->contains(pt))
         fiber_item = nullptr;
      if (cell_item && !cell_item->contains(pt))
         cell_item = nullptr;
   }
   if (par->getConnMode() == connMode::START)
   {
        // if we are in bbox for src object, start line
      if (fiber_item || (cell_item && cell_item->contains(cell_item->mapFromScene(pt))))
      {
         currConn = new Connector(pt,lineParent); // this adds item to scene
         currConn->setSynType(synType);
         currConn->setExcite(excite);
         if (fromFile)
            currConn->setTargIdx(targidx);
         currConn->setColor(synapseColors[synType-1]);
         par->ui->synapseList->expandAll();
         par->ui->synapseList->setCurrentIndex(par->ui->synapseList->indexFromType(synType));
         par->setConnMode(connMode::DRAWING);
         if (fiber_item)
         {
            currConn->setId(fiber_item->getDidx()); // target info kept here
            currConn->setStartItem(fiber_item);
            fiber_item->fromLines.push_back(currConn);
         }
         else
         {
            currConn->setId(cell_item->getDidx());
            currConn->setStartItem(cell_item);
            cell_item->fromLines.push_back(currConn);
         }
      }
   }
   else if (currConn && par->getConnMode() == connMode::DRAWING) // can terminate at cell, but not fiber
   {
      if (currConn && currConn->numPts() > MAX_PTS)
      {
         removeItem(currConn);
         delete currConn;
         par->addConnectorOff();
         currConn = nullptr;
      }
      else if (!fromFile && currConn->dupPt(pt))
      {
         // Ignore duplicate adjacent pts from usr, but there are lots of them
         // in some files and we have to live with them.
         par->printMsg("Duplicate point ignored, click again.");
      }
      else if (cell_item && cell_item->contains(cell_item->mapFromScene(pt)))
      {
             // if we are in bbox of a dest object, done
         currConn->addPt(pt);
         currConn->calcShape();
         currConn->addCell();
         currConn->setEndItem(cell_item);
         currConn->setZValue(connOrder);
         currConn->setFlag(QGraphicsItem::ItemIsSelectable, true);
         cell_item->toLines.push_back(currConn);
         if (!fromFile)
         {
            addSynapse(*currConn); // this managed differently if reading in a file
            setFileDirty(true);
         }
         currConn = nullptr;
         par->addConnectorOff();
      }
      else  // just another segment
      {
         currConn->addPt(pt);
      }
   }
   else if (par->getConnMode() == connMode::CANCEL)
   {
      par->addConnectorOff();
   }
}

// The user has started drawing a connector and had decided to delete it.
// Remove them from the scene and also from the graphic object they are attached to.
void SimScene::removePath()
{
   if (currConn)
   {
      Conn delList(1,currConn);
      BaseNode *obj = dynamic_cast <BaseNode *>(currConn->start);
      if (obj)
         obj->removeLines(delList);
      removeItem(currConn);
      delete currConn;
      currConn = nullptr;
   }
}

void SimScene::fiberViewSendToLauncherBdt()
{
   bdtRec     bdt_row;
   FiberNode* fiber_item;
   int d_idx, num;
   QString pop_id;

   auto list = selectedItems();
   for (auto item = list.cbegin(); item != list.cend(); ++item)
   {
      fiber_item = dynamic_cast <FiberNode *>(*item);
      if (fiber_item)
      {
         d_idx = fiber_item->getDidx();
         pop_id.setNum(D.inode[d_idx].node_number);
         num = D.inode[d_idx].unode.fiber_node.f_pop;

         bdt_row.rec[BDT_CELL_FIB] = FIBER;
         bdt_row.rec[BDT_POP] = pop_id;
         bdt_row.rec[BDT_MEMB] = "0"; // this set later by model
         if (D.inode[d_idx].unode.fiber_node.pop_subtype == ELECTRIC_STIM)
            bdt_row.rec[BDT_TYPE] = QString::number(ELECTRIC_STIM);
         else if (D.inode[d_idx].unode.fiber_node.pop_subtype == AFFERENT)
            bdt_row.rec[BDT_TYPE] = QString::number(AFFERENT);
         else
            bdt_row.rec[BDT_TYPE] = "0";
         bdt_row.maxRndVal = num;
         par->launch->bdtListModel->addRow(bdt_row);
      }
   }
}


void SimScene::fiberViewSendToLauncherPlot()
{
   plotRec     plot_row;
   FiberNode* fiber_item;
   int d_idx, num;
   QString pop_id;

   auto list = selectedItems();
   for (auto item = list.cbegin(); item != list.cend(); ++item)
   {
      fiber_item = dynamic_cast <FiberNode *>(*item);
      if (fiber_item)
      {
         d_idx = fiber_item->getDidx();
         pop_id.setNum(D.inode[d_idx].node_number);
         num = D.inode[d_idx].unode.fiber_node.f_pop;

         plot_row.rec[PLOT_CELL_FIB] = STD_FIBER;
         plot_row.rec[PLOT_POP] = pop_id;
         plot_row.rec[PLOT_MEMB] = "0";  // set later in model
         if (D.inode[d_idx].unode.fiber_node.pop_subtype == AFFERENT)
         {
            plot_row.rec[PLOT_TYPE] = QString::number(AFFERENT_EVENT);
            plot_row.popType = AFFERENT_EVENT;
         }
         else
         {
            plot_row.rec[PLOT_TYPE] = QString::number(STD_FIBER);
            plot_row.popType = STD_FIBER;
         }
         plot_row.rec[PLOT_BINWID] = "";
         plot_row.rec[PLOT_SCALE] = "";
         plot_row.maxRndVal = num;
         par->launch->plotDeskModel->addRow(plot_row);
      }
   }
}

void SimScene::addSynType()
{
   synRec rec;
   int node = getSynapse();
   if (node > 0)
   {
      rec.rec[SYN_NUM] = node;
      rec.rec[SYN_NAME]  = "Synapse" + QString::number(node);
      rec.rec[SYN_TYPE]  = SYN_NORM;
      rec.rec[SYN_EQPOT] = 0.0;
      rec.rec[SYN_TC] = 0.0;
      rec.rec[SYN_PARENT] = 0;
      QModelIndex idx;
      synBuildModel->addRow(rec,idx);
      par->ui->buildSynapsesView->expandAll();
      par->ui->buildSynapsesView->setCurrentIndex(idx);
      updateInfoText();
      setBuildDirty(true);
   }
   else
      outOfNodes();
}

void SimScene::addSynPre()
{
   QModelIndex syn_row = par->ui->buildSynapsesView->currentIndex();
   if (syn_row.isValid())
   {
      synRec rec, newrec;
      synBuildModel->readRec(rec,syn_row);
      if (rec.rec[SYN_TYPE].toInt() == SYN_NORM)
      {
         int node = getSynapse();
         if (node > 0)
         {
            newrec.rec[SYN_NUM] = node;
            newrec.rec[SYN_NAME]  = "Synapse" + QString::number(node);
            newrec.rec[SYN_EQPOT] = 0.0;
            newrec.rec[SYN_TC] = 0.0;
            newrec.rec[SYN_TYPE]  = SYN_PRE;
            newrec.rec[SYN_PARENT] = rec.rec[SYN_NUM];
            QModelIndex idx;
            synBuildModel->addRow(newrec,idx);
            par->ui->buildSynapsesView->expandAll();
            updateInfoText();
            setBuildDirty(true);
            par->ui->buildAddPre->setEnabled(false);
         }
         else
            outOfNodes();
      }
   }
}

void SimScene::addSynPost()
{
   QModelIndex syn_row = par->ui->buildSynapsesView->currentIndex();
   if (syn_row.isValid())
   {
      synRec rec, newrec;
      synBuildModel->readRec(rec,syn_row);
      if (rec.rec[SYN_TYPE].toInt() == SYN_NORM)
      {
         int node = getSynapse();
         if (node > 0)
         {
            newrec.rec[SYN_NUM] = node;
            newrec.rec[SYN_NAME]  = "Synapse" + QString::number(node);
            newrec.rec[SYN_EQPOT] = 0.0;
            newrec.rec[SYN_TC] = 0.0;
            newrec.rec[SYN_TYPE]  = SYN_POST;
            newrec.rec[SYN_PARENT] = rec.rec[SYN_NUM];
            QModelIndex idx;
            synBuildModel->addRow(newrec,idx);
            par->ui->buildSynapsesView->expandAll();
            updateInfoText();
            setBuildDirty(true);
            par->ui->buildAddPost->setEnabled(false);
         }
         else
            outOfNodes();
      }
   }
}

void SimScene::addSynLearn()
{
   synRec rec;
   int node = getSynapse();
   if (node > 0)
   {
      rec.rec[SYN_NUM] = node;
      rec.rec[SYN_NAME]  = "Synapse" + QString::number(node);
      rec.rec[SYN_TYPE]  = SYN_LEARN;
      rec.rec[SYN_EQPOT] = 0.0;
      rec.rec[SYN_TC] = 0.0;
      rec.rec[SYN_LRN_WINDOW] = 4;
      rec.rec[SYN_LRN_DELTA] = 0.1;
      rec.rec[SYN_LRN_MAX] = 2;
      rec.rec[SYN_PARENT] = 0;
      QModelIndex idx;
      synBuildModel->addRow(rec,idx);
      par->ui->buildSynapsesView->expandAll();
      par->ui->buildSynapsesView->setCurrentIndex(idx);
      updateInfoText();
      setBuildDirty(true);
   }
   else
      outOfNodes();
}


void SimScene::delSynType()
{
   synRec rec;
   int syn_num, in_use;
   bool del = true;
   QMessageBox msgBox;
   QString msg, name;
   QTextStream warn(&msg);

    // this is index relative to parent, not same as row#
   QModelIndex index = par->ui->buildSynapsesView->selectionModel()->currentIndex();
   if (index.row() != -1)
   {
      synBuildModel->readRec(rec,index);
      syn_num = rec.rec[SYN_NUM].toInt();
      in_use = chkSynInUse(syn_num);
      if (in_use)
      {
         name = rec.rec[SYN_NAME].toString();
         warn << "WARNING: The synapse " << name.toLatin1().data() << " is used by " << in_use << " nodes." << endl
              << "Deleting this will remove it from all of these nodes." << endl
              << "This operation can not be undone. " << endl
              << "The file is not affected unless you over-write it." << endl
              << "Do you want to delete " << name.toLatin1().data() << "?";
         msgBox.setIcon(QMessageBox::Question);
         msgBox.setWindowTitle("Okay To Delete?");
         msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
         msgBox.setText(msg);
         if (msgBox.exec() == QMessageBox::Yes)
            deleteSynType(syn_num);
         else
            del = false;
      }
      if (del)
      {
         synBuildModel->recycleSynapses(index);
         synBuildModel->delRow(index.row(), index.parent());
         par->ui->buildSynapsesView->expandAll();
         updateInfoText();
         setBuildDirty(true);
      }
   }
}

void SimScene::buildChg(const QModelIndex &/*topLeft*/, const QModelIndex &/*bottomRight*/)
{
   setBuildDirty(true);
}

void SimScene::buildRowChg(const QModelIndex &curr, const QModelIndex &/*prev*/)
{
   if (curr.isValid())
   {
      synRec rec;
      synBuildModel->readRec(rec,curr);
      int type = rec.rec[SYN_TYPE].toInt();
      switch (type)
      {
         case SYN_NORM:
            if (synBuildModel->hasPre(curr))
               par->ui->buildAddPre->setEnabled(false);
            else
               par->ui->buildAddPre->setEnabled(true);

            if (synBuildModel->hasPost(curr))
               par->ui->buildAddPost->setEnabled(false);
            else
               par->ui->buildAddPost->setEnabled(true);
            break;

         case SYN_PRE:
         case SYN_POST:
         default:
            par->ui->buildAddPre->setEnabled(false);
            par->ui->buildAddPost->setEnabled(false);
            break;
      }
   }
}

// send info the the launcher
void SimScene::cellViewSendToLauncherBdt()
{
   bdtRec     bdt_row;
   CellNode* cell_item;
   int d_idx, num;
   QString pop_id;

   auto list = selectedItems();
   for (auto item = list.cbegin(); item != list.cend(); ++item)
   {
      cell_item = dynamic_cast <CellNode *>(*item);
      if (cell_item)
      {
         d_idx = cell_item->getDidx();
         pop_id.setNum(D.inode[d_idx].node_number);
         num = D.inode[d_idx].unode.cell_node.c_pop;
         bdt_row.rec[BDT_CELL_FIB] = CELL;
         bdt_row.rec[BDT_POP] = pop_id;
         bdt_row.rec[BDT_MEMB] = "0";
         bdt_row.rec[BDT_TYPE] = "0";
         bdt_row.maxRndVal = num;
         par->launch->bdtListModel->addRow(bdt_row);
      }
   }
}

void SimScene::cellViewSendToLauncherPlot()
{
   plotRec     plot_row;
   CellNode* cell_item;
   int d_idx, num;
   QString pop_id;

   auto list = selectedItems();
   for (auto item = list.cbegin(); item != list.cend(); ++item)
   {
      cell_item = dynamic_cast <CellNode *>(*item);
      if (cell_item)
      {
         d_idx = cell_item->getDidx();
         pop_id.setNum(D.inode[d_idx].node_number);
         num = D.inode[d_idx].unode.cell_node.c_pop;

         plot_row.rec[PLOT_CELL_FIB] = STD_CELL;
         plot_row.rec[PLOT_POP] = pop_id;
         plot_row.rec[PLOT_MEMB] = "0";  // set later in model
         plot_row.rec[PLOT_TYPE] = QString::number(0); // action potential
         plot_row.rec[PLOT_BINWID] = "5";
         plot_row.rec[PLOT_SCALE] = "1";
         plot_row.maxRndVal = num;
         if (D.inode[d_idx].unode.cell_node.pop_subtype == BURSTER_POP)
            plot_row.popType = BURSTER_CELL;
         else if (D.inode[d_idx].unode.cell_node.pop_subtype == PSR_POP)
            plot_row.popType = PSR_CELL;
         else
            plot_row.popType = STD_CELL;
         par->launch->plotDeskModel->addRow(plot_row);
      }
   }
}


// When selection in connector synapse list changes
void SimScene::synapseTypeChg()
{
   QColor color;
   int idx;
   bool excite;

   QModelIndex syn_row = par->ui->synapseList->currentIndex();
   if (syn_row.row() == -1) // nothing selected, nothing to do
      return;
   idx = syn_row.sibling(syn_row.row(),SYN_NUM).data().toInt();
   color = synapseColors[idx-1];
   auto list = selectedItems();
   if (list.size() == 1)
   {
      Connector* axonitem = dynamic_cast <Connector *>(list[0]);

      if (axonitem)
      {
         excite = D.inode[SYNAPSE_INODE].unode.synapse_node.s_eq_potential[idx] > 0;
         axonitem->setSynType(idx);
         axonitem->setColor(color);
         axonitem->setExcite(excite);
         setSynapseDirty(true);
      }
   }
   invalidate(sceneRect());
}

// Remove item(s).
// Deleted nodes must have their connectors removed.
// Deleted connectors must be removed from connected nodes.
void SimScene::deleteSelected()
{
   int d_idx;
   auto list = selectedItems();
   if (!list.size())
      return;
   auto all = items();

   Conn delList;
   std::list<QGraphicsItem *> ptrList; // can have dup ptrs, ensure no dups
   std::list<QGraphicsItem *>::iterator iter;
   FiberNode* fiber_item;
   CellNode* cell_item;
   Connector* synapse_item;
   setFileDirty(true);

   invalidate(sceneRect(),QGraphicsScene::AllLayers);
   par->ui->paramTabs->clear();  // nothing will be selected after we're done
   for (auto item = list.cbegin(); item != list.cend(); ++item)
   {
      BaseNode *base_item = dynamic_cast <BaseNode *>(*item);
      synapse_item = dynamic_cast <Connector *>(*item);
      if (base_item)
      {
         d_idx = base_item->getDidx();
         for (connIter iter = base_item->fromLines.begin(); iter != base_item->fromLines.end(); ++iter)
            delList.push_back(*iter);
         for (connIter iter = base_item->toLines.begin(); iter != base_item->toLines.end(); ++iter)
            delList.push_back(*iter);
         free_node(d_idx);
         ptrList.push_front(*item);
      }
      else if (synapse_item)
      {
         synapse_item->prepareGeometryChange();
         deleteSynapse(*synapse_item);
         ptrList.push_front(*item);
      }
      else
         cout << "Unhandled kind of selected item" << endl;
   }
        // remove all refs to any connecting lines, selected or not
   if (delList.size())
   {
      auto all = items();
      {
         for (auto all_iter = all.begin(); all_iter != all.end(); ++all_iter)
         {
            fiber_item = dynamic_cast <FiberNode *>(*all_iter);
            cell_item = dynamic_cast <CellNode *>(*all_iter);
            if (fiber_item)
               fiber_item->removeLines(delList);
            else if (cell_item)
               cell_item->removeLines(delList);
         }
         for (auto lines = delList.begin(); lines != delList.end(); ++lines)
         {
            ptrList.push_front(*lines);
         }
      }
   }
   ptrList.sort();     // there may be dups in the list, reduce to just once
   ptrList.unique();
   for (iter = ptrList.begin(); iter != ptrList.end(); ++iter)
   {
      removeItem(*iter);
      delete *iter;
    }
   get_maxes(&D);
}


void SimScene::mouseMoveEvent(QGraphicsSceneMouseEvent *me)
{
   moving = true;
   QPointF pt = me->scenePos();
   QString coords;
   QTextStream xy(&coords);
   xy << "X: " << pt.x() << endl << "Y: " << pt.y();
   par->ui->currXY->setPlainText(coords);
   if (waitForImportClick)
   {
      QPoint pos = me->screenPos();
      pos.rx() += 20;
      pos.ry() -= 20;
      importLabel->move(pos);
   }
   QGraphicsScene::mouseMoveEvent(me);
}

// Mouse is up. We may have selected/deselected objects.
void SimScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *me)
{
   if (!par->addFiberToggle && !par->addCellToggle && par->connectorMode == connMode::OFF)
      par->ui->simView->setDragMode(QGraphicsView::RubberBandDrag);
   moving = false;
   QGraphicsScene::mouseReleaseEvent(me);
   importGroupChk(me);
   selectGroupChk();
   axonSynChoices(me);
   mngTabs();
}

// Item(s) perhaps selected, do we need to show a param tab?
// If there is more than one current object or if the current object
// is not the one whose params are displayed on the current visible tab page,
// prompt to save before showing new object' params or none if more than one.
void SimScene::mngTabs()
{
   auto list = selectedItems();
   int curr_tab_obj, curr_sel_obj;

   if (list.size() == 1)
   {
      auto item = list.cbegin();
      FiberNode *fiber_item = dynamic_cast <FiberNode *>(*item);
      CellNode* cell_item = dynamic_cast <CellNode *>(*item);
      Connector* synapse_item = dynamic_cast <Connector *>(*item);
      if (fiber_item)
      {
         curr_sel_obj = fiber_item->getDidx();
         curr_tab_obj = par->ui->fiberObjId->text().toInt();
         cellChkDirty();
         synapseChkDirty();
         buildChkDirty();
         globalsChkDirty();
         if (curr_sel_obj != curr_tab_obj)
         {
            fiberChkDirty();
            fiberRecToUi(fiber_item->getDidx());
         }
         par->ui->paramTabs->clear();
         par->ui->paramTabs->addTab(par->ui->fiberPage,"Fiber Parameters");
         par->ui->fiberTypes->clear();
         if (par->ui->fiberNormal->isChecked())
            par->ui->fiberTypes->addTab(par->ui->fiberNormTab,"Normal Fiber");
         else if (par->ui->fiberElectric->isChecked())
            par->ui->fiberTypes->addTab(par->ui->fiberElecTab,"Electric Stimulation");
         else if (par->ui->fiberAfferent->isChecked())
            par->ui->fiberTypes->addTab(par->ui->fiberAfferentTab,"Afferent Source");
         else
            cout << "bug in mngtabs, unknown checkbox type" << endl;
      }
      else if (cell_item)
      {
         curr_sel_obj = cell_item->getDidx();
         curr_tab_obj = par->ui->cellObjId->text().toInt();
         fiberChkDirty();
         synapseChkDirty();
         buildChkDirty();
         globalsChkDirty();
         par->ui->paramTabs->clear();
         par->ui->paramTabs->addTab(par->ui->cellPage,"Cell Parameters");
         if (curr_sel_obj != curr_tab_obj)
         {
            cellChkDirty();
            cellRecToUi(cell_item->getDidx());
            par->ui->cellTypes->clear();
            if (par->ui->bursterCell->isChecked())
               par->ui->cellTypes->addTab(par->ui->bursterTab,"Burster");
            else if (par->ui->psrCell->isChecked())
               par->ui->cellTypes->addTab(par->ui->psrTab,"PSR");
            else
               par->ui->cellTypes->addTab(par->ui->cellTab,"Normal Cell");
         }
      }
      else if (synapse_item)
      {
         curr_sel_obj = synapse_item->getId();
         curr_tab_obj = par->ui->synapseObjId->text().toInt();
         cellChkDirty();
         fiberChkDirty();
         buildChkDirty();
         globalsChkDirty();
         if (synapseDirty && curr_sel_obj != curr_tab_obj)
            synapseChkDirty();
         par->ui->paramTabs->clear();
         par->ui->paramTabs->addTab(par->ui->synapsePage,"Synapse Parameters");
         par->ui->synapseList->expandAll();
          // selection of connector handled elsewhere
      }
   }
   else if (list.size() == 0 || list.size() > 1) // desel all, or group sel
   {
      // is current page dirty? Find out and prompt for save if so
      cellChkDirty();
      fiberChkDirty();
      synapseChkDirty();
      buildChkDirty();
      globalsChkDirty();
      selectedConn.lineitem = nullptr;
      par->ui->paramTabs->clear();
      par->ui->fiberObjId->setText("-1");  // no current objects
      par->ui->cellObjId->setText("-1");
      par->ui->synapseObjId->setText("-1");
   }
}

// Accept or reject current changes.
void SimScene::cellChkDirty()
{
   if (!cellDirty || par->warningsOff)
      return;
   QMessageBox msgBox;
   msgBox.setIcon(QMessageBox::Question);
   msgBox.setWindowTitle("Unsaved Changes");
   msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
   msgBox.setText("Save Changes on Cell Page?");
   if (msgBox.exec() == QMessageBox::Yes)
      cellApply();
   else
      cellUndo();
}

void SimScene::fiberChkDirty()
{
   if (!fiberDirty || par->warningsOff)
      return;
   QMessageBox msgBox;
   msgBox.setIcon(QMessageBox::Question);
   msgBox.setWindowTitle("Unsaved Changes");
   msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
   msgBox.setText("Save Changes on Fiber Page?");
   if (msgBox.exec() == QMessageBox::Yes)
      fiberApply();
   else
      fiberUndo();
}

void SimScene::synapseChkDirty()
{
   if (!synapseDirty || par->warningsOff)
      return;
   QMessageBox msgBox;
   msgBox.setIcon(QMessageBox::Question);
   msgBox.setWindowTitle("Unsaved Changes");
   msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
   msgBox.setText("Save Changes on Synapse Page?");
   if (msgBox.exec() == QMessageBox::Yes)
      synapseApply();
   else
      synapseUndo();
}

void SimScene::buildChkDirty()
{
   if (!buildDirty || par->warningsOff)
      return;
   QMessageBox msgBox;
   msgBox.setIcon(QMessageBox::Question);
   msgBox.setWindowTitle("Unsaved Changes");
   msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
   msgBox.setText("Save Changes on Build Synapse Page?");
   if (msgBox.exec() == QMessageBox::Yes)
      buildApply();
   else
      buildUndo();
}

void SimScene::globalsChkDirty()
{
   if (!globalsDirty || par->warningsOff)
      return;
   QMessageBox msgBox;
   msgBox.setIcon(QMessageBox::Question);
   msgBox.setWindowTitle("Unsaved Changes");
   msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
   msgBox.setText("Save Changes on Globals Page?");
   if (msgBox.exec() == QMessageBox::Yes)
      globalsApply();
   else
      globalsUndo();
}

// The controls may not be dirty, but there may be
// changes in the in-memory file. This tracks that.
bool SimScene::fileChkDirty()
{
   if (!fileDirty || par->warningsOff)
      return false;
   QMessageBox msgBox;
   msgBox.setIcon(QMessageBox::Question);
   msgBox.setWindowTitle("Save File");
   msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
   msgBox.setText("There have been changes to the simulation parameters, the model drawing, or launch parameters that have not been saved.\nDo you want to create/update a file?");
   if (msgBox.exec() == QMessageBox::Yes)
      return true;
   else
      return false;
}


// There are default values for most of the controls.
// During startup load them.
void SimScene::initControls()
{
   globalsRecToUi();   // load them
   updateInfoText();   // show them in graphic
   buildRecToUi();
}

void SimScene::fiberUndo()
{
   int d_idx = par->ui->fiberObjId->text().toInt();
   fiberRecToUi(d_idx);
   invalidate(sceneRect());
}

void SimScene::fiberApply()
{
   fiberUiToRec();
   get_maxes(&D);
   setFileDirty(true);
   par->buildFindList();
   invalidate(sceneRect());
}

void SimScene::cellUndo()
{
   int d_idx = par->ui->cellObjId->text().toInt();
   cellRecToUi(d_idx);
}

void SimScene::cellApply()
{
   cellUiToRec();
   get_maxes(&D);
   setFileDirty(true);
   par->buildFindList();
}

void SimScene::synapseUndo()
{
   if (selectedConn.lineitem)
      synapseRecToUi(selectedConn);
   setSynapseDirty(false);
}

void SimScene::synapseApply()
{
   if (selectedConn.lineitem)
   {
      synapseUiToRec();
      get_maxes(&D);
      setFileDirty(true);
   }
   setSynapseDirty(false);
}

void SimScene::buildUndo()
{
   synBuildModel->undo();   // restore original recs
   buildRecToUi();
   setBuildDirty(false);
}

void SimScene::buildApply()
{
   buildUiToRec();
   updateExcite();
   get_maxes(&D);
   setFileDirty(true);
}

void SimScene::globalsUndo()
{
   globalsRecToUi();
}

void SimScene::globalsApply()
{
   globalsUiToRec();
   setFileDirty(true);
}

void SimScene::resetModels()
{
   synBuildModel->reset();
}

// copy vars to controls
// assumes the syn_type field has been updated if we're using a pre-V6 file
void SimScene::buildRecToUi()
{
   synRec rec;
   S_NODE *s = &D.inode[SYNAPSE_INODE].unode.synapse_node;
   int node;

   synBuildModel->prepareUndo(); // make a copy of original recs

   // have to walk through table twice.  Create norm/parent rows
   // then create pre/post/children, since parent has to exist before child
   // can be added, and due to editing, children can occur in list before parent
   synBuildModel->reset();
   QModelIndex idx;
   for (node = 1; node < TABLE_LEN; ++node) // index 0 unused for synapse node
   {
      if (s->syn_type[node] == SYN_NORM || s->syn_type[node] == SYN_LEARN)
      {
         rec.rec[SYN_NUM] = node;
         rec.rec[SYN_NAME]  = s->synapse_name[node];
         rec.rec[SYN_TYPE]  = s->syn_type[node];
         rec.rec[SYN_EQPOT] = s->s_eq_potential[node];
         rec.rec[SYN_TC]    = s->s_time_constant[node];
         rec.rec[SYN_PARENT]   = 0;    // no parent
         if (s->syn_type[node] == SYN_NORM)
         {
            rec.rec[SYN_LRN_WINDOW] = QString();
            rec.rec[SYN_LRN_MAX] = QString();
            rec.rec[SYN_LRN_DELTA] = QString();
         }
         else
         {
            rec.rec[SYN_LRN_WINDOW] = s->lrn_window[node];
            rec.rec[SYN_LRN_MAX] = s->lrn_maxstr[node];
            rec.rec[SYN_LRN_DELTA] = s->lrn_delta[node];
         }
         synBuildModel->addRow(rec,idx);

      }
   }
   for (node = 1; node < TABLE_LEN; ++node)
   {
      if (s->syn_type[node] == SYN_PRE || s->syn_type[node] == SYN_POST)
      {
         rec.rec[SYN_NUM] = node;
         rec.rec[SYN_NAME]  = s->synapse_name[node];
         rec.rec[SYN_TYPE]  = s->syn_type[node];
         rec.rec[SYN_EQPOT] = s->s_eq_potential[node];
         rec.rec[SYN_TC]    = s->s_time_constant[node];
         rec.rec[SYN_PARENT]   = s->parent[node];
         rec.rec[SYN_LRN_WINDOW] = QString();
         rec.rec[SYN_LRN_MAX] = QString();
         rec.rec[SYN_LRN_DELTA] = QString();
         synBuildModel->addRow(rec,idx);
      }
   }
   par->ui->buildSynapsesView->expandAll();
   setBuildDirty(false);
}


void SimScene::globalsRecToUi()
{
   QString fmt;
   float step_time, secs;
   GLOBAL_NETWORK *g = &D.inode[GLOBAL_INODE].unode.global_node;

   par->ui->globComment->setText(g->global_comment);
   par->ui->globPhrenicRecruit->setText(g->phrenic_equation);
   par->ui->globLumbarRecruit->setText(g->lumbar_equation);
   secs = g->sim_length_seconds;
   fmt = QString("%1").arg(secs,0,'f',2);
   par->ui->globLength->setText(fmt);
   step_time = g->step_size;
   fmt = QString("%1").arg(step_time,0,'f');
   par->ui->globStepTime->setText(fmt);
   fmt = QString("%1").arg(g->k_equilibrium,0,'f');
   par->ui->globKEqPot->setText(fmt);
   if ( fabs(step_time - 0.5) < 0.000001)
   {
      par->ui->globalBdt->blockSignals(true);
      par->ui->globalBdt->setChecked(true);
      par->launch->bdtSel();
      par->ui->globalBdt->blockSignals(false);
   }
   else if ( fabs(step_time - 0.1) < 0.000001)
   {
      par->ui->globalEdt->setChecked(true);
      par->launch->edtSel();
   }
   else
   {
      par->ui->globalOther->setChecked(true);
      par->launch->customSel();
   }
   if (g->ilm_elm_fr == 0)    // old file
      g->ilm_elm_fr = 40.0;   // use default value until user changes it
   fmt = QString("%1").arg(g->ilm_elm_fr,0,'f');
   par->ui->iLmElmFiringRate->setText(fmt);
   setGlobalsDirty(false);
}

void SimScene::globalsUiToRec()
{
   QString fmt;
   float step_time, len;
   unsigned int sim_steps;
   GLOBAL_NETWORK *g = &D.inode[GLOBAL_INODE].unode.global_node;

   memset(g->global_comment,0,sizeof(g->global_comment));
   memset(g->phrenic_equation,0,sizeof(g->phrenic_equation));
   memset(g->lumbar_equation,0,sizeof(g->lumbar_equation));
   strncpy(g->global_comment,par->ui->globComment->text().toLatin1().data(),sizeof(g->global_comment)-1);
   strncpy(g->phrenic_equation,par->ui->globPhrenicRecruit->text().toLatin1().data(),sizeof(g->phrenic_equation)-1);
   strncpy(g->lumbar_equation,par->ui->globLumbarRecruit->text().toLatin1().data(),sizeof(g->lumbar_equation)-1);
   len = par->ui->globLength->text().toFloat();  // this is in seconds
   g->sim_length_seconds = len;
   step_time = par->ui->globStepTime->text().toFloat();
   sim_steps = ceil((len*1000.0) / step_time);
   g->sim_length = sim_steps;
   g->step_size = step_time;
   g->k_equilibrium = par->ui->globKEqPot->text().toFloat();
   g->ilm_elm_fr = par->ui->iLmElmFiringRate->text().toFloat();
   g->sim_length_seconds = par->ui->globLength->text().toFloat();
   updateInfoText();
   setGlobalsDirty(false);
}



void SimScene::updateInfoText()
{
   // Update text in  scene ul corner
   float secs;
   QString txt;
   QTextStream strm(&txt);
   GLOBAL_NETWORK *g = &D.inode[GLOBAL_INODE].unode.global_node;

   secs = g->sim_length_seconds;
   strm  <<  g->global_comment << endl;
   strm  <<  "Simulation Length: " << secs << " seconds" << endl;
   strm  <<  "K Eq Potential   : " <<  g->k_equilibrium << endl;
   strm  <<  "Step Size        : " <<  g->step_size << endl;
   strm  <<  "Nodes Free: " <<  D.num_free_nodes << " Nodes in use: " << MAX_INODES-D.num_free_nodes << endl;
   strm  <<  "Synapse Types: " <<  synTypesInUse() << endl;

   globalText->setText(QString());
   QBrush brush(sceneFG,Qt::NoBrush);
   QPen pen(brush,NODE_WIDTH);
   pen.setColor(sceneFG);
   brush.setColor(sceneFG);
   brush.setStyle(Qt::SolidPattern);
   globalText->setBrush(brush);
   globalText->setFont(itemFont);
   globalText->setPos(QPoint(1,3));
   globalText->setText(txt);
}

// Copy control values to vars
// we don't keep track of which rows may be changed, so
// update from all of them.
void SimScene::buildUiToRec()
{
   synRec rec;
   S_NODE *s = &D.inode[SYNAPSE_INODE].unode.synapse_node;
   QString name;
   int idx, type, node, window;
   int par_idx = 0;
   float eq, tc, str_delta, max_str;
   QModelIndex norm;
   QModelIndex child;
   QModelIndex root;
   int norm_recs = synBuildModel->rowCount(root);

   auto upd_D =[&]  {
        idx = rec.rec[SYN_NUM].toInt();
        name = rec.rec[SYN_NAME].toString();
        type = rec.rec[SYN_TYPE].toInt();
        eq = rec.rec[SYN_EQPOT].toFloat();
        tc = rec.rec[SYN_TC].toFloat();
        const char *str = name.toLocal8Bit().constData();
        memset(s->synapse_name,0,SNMLEN);
        strncpy(s->synapse_name[idx],str,SNMLEN-1);
        if (type == SYN_LEARN)
        {
           window = rec.rec[SYN_LRN_WINDOW].toInt();
           str_delta = rec.rec[SYN_LRN_DELTA].toFloat();
           max_str = rec.rec[SYN_LRN_MAX].toFloat();
        }
        else
        {
           window = 0;
           str_delta = 0;
           max_str = 0;
        }
        s->s_eq_potential[idx] = eq;
        s->s_time_constant[idx] = tc;
        s->lrn_window[idx] = window;
        s->lrn_delta[idx] = str_delta;
        s->lrn_maxstr[idx] = max_str;
        s->syn_type[idx] = static_cast<char>(type);
    };

   for (node = 0; node < norm_recs; ++node)
   {
      norm = synBuildModel->index(node,0,root);
      if (norm.isValid())
      {
         synBuildModel->readRec(rec,norm);
         par_idx = idx = rec.rec[SYN_NUM].toInt();
         upd_D();
      }
      child = synBuildModel->index(0,0,norm); // up to 2 children
      if (child.isValid())
      {
         synBuildModel->readRec(rec,child);
         upd_D();
         s->parent[idx] = par_idx;
      }
      child = synBuildModel->index(1,0,norm); // up to 2 children
      if (child.isValid())
      {
         synBuildModel->readRec(rec,child);
         upd_D();
         s->parent[idx] = par_idx;
      }
   }
   setBuildDirty(false);
}

// Copy D array values to controls
void SimScene::fiberRecToUi(int d_idx, int sel)
{
   F_NODE* fiber = &D.inode[d_idx].unode.fiber_node;
   int save_type = fiber->pop_subtype;
   if (sel > 0)
      fiber->pop_subtype = sel;
   par->ui->paramTabs->addTab(par->ui->fiberPage,"Fiber Parameters");
   par->ui->fiberTypes->clear();
   if (fiber->pop_subtype == ELECTRIC_STIM)
   {
      par->ui->fiberElectricSeed->setValue(fiber->f_seed);
              // int milliseconds to float seconds
      par->ui->fiberElectricStartTime->setValue(fiber->f_begin/1000.0);
      fiber->f_end < 0 ?  par->ui->fiberElectricStopTime->setValue(fiber->f_end) :
                          par->ui->fiberElectricStopTime->setValue(fiber->f_end/1000.0);
      par->ui->fiberElectricFreq->setValue(fiber->frequency);
      par->ui->fiberElectricFuzzyRange->setValue(fiber->fuzzy_range);
      par->ui->fiberTypes->addTab(par->ui->fiberElecTab,"Electric Stimulation");
      if (fiber->freq_type == STIM_FIXED)
         par->ui->fiberElectricDeterministic->setChecked(true);

      else
         par->ui->fiberElectricFuzzy->setChecked(true);
      par->ui->fiberElectric->setChecked(true);
      par->ui->fiberElectricFreq->setValue(fiber->frequency);
      par->ui->fiberElectricFuzzyRange->setValue(fiber->fuzzy_range);
   }
   else if (fiber->pop_subtype == AFFERENT)
   {
      par->ui->fiberTypes->addTab(par->ui->fiberAfferentTab,"Afferent Source");
      par->ui->fiberAfferentOffset->setValue(fiber->offset);
      par->ui->fiberAffSeed->setValue(fiber->f_seed);
      par->ui->fiberAfferent->setChecked(true);
      par->ui->afferentFileName->setText(fiber->afferent_file);
      par->ui->fiberAfferentStartFire->setValue(fiber->f_begin/1000.0);
      par->ui->fiberAfferentNumPop->setValue(fiber->f_pop);
      par->ui->fiberAfferentScale->setValue(fiber->slope_scale);
      fiber->f_end < 0 ?  par->ui->fiberAfferentStopFire->setValue(fiber->f_end) :
                          par->ui->fiberAfferentStopFire->setValue(fiber->f_end/1000.0);
      par->affRecToModel(fiber);
   }
   else
   {
      par->ui->fiberTypes->addTab(par->ui->fiberNormTab,"Normal Fiber");
      par->ui->fibProbFire->setValue(fiber->f_prob);
      par->ui->fibStartFire->setValue(fiber->f_begin/1000.0);
      fiber->f_end < 0 ?  par->ui->fibEndFire->setValue(fiber->f_end) :
                          par->ui->fibEndFire->setValue(fiber->f_end/1000.0);
      par->ui->fibRndSeed->setValue(fiber->f_seed);
      par->ui->fibNumPop->setValue(fiber->f_pop);
      par->ui->fiberNormal->setChecked(true);
   }
   par->ui->fiberGroup->setText(D.inode[d_idx].unode.fiber_node.group);
   par->ui->fiberPopNum->setText(QString::number(D.inode[d_idx].node_number));
   par->ui->fiberObjId->setText(QString::number(d_idx));
   par->ui->fibComment->setText(D.inode[d_idx].comment1);
   fiber->pop_subtype = save_type;
   setFiberDirty(false);
}

void SimScene::fiberUiToRec()
{
   int d_idx = par->ui->fiberObjId->text().toInt();
   F_NODE* fiber = &D.inode[d_idx].unode.fiber_node;
   QString txt1, txt2;
   QTextStream strm1(&txt1);
   QTextStream strm2(&txt2);
   int node_num = D.inode[d_idx].node_number;

   if (par->ui->fiberNormal->isChecked())
   {
      fiber->f_prob = par->ui->fibProbFire->value();
      fiber->f_begin = par->ui->fibStartFire->value()*1000.0;
      par->ui->fibEndFire->value() < 0 ? fiber->f_end = par->ui->fibEndFire->value():
                                         fiber->f_end = round(par->ui->fibEndFire->value()*1000.0);
      fiber->f_seed = par->ui->fibRndSeed->value();
      fiber->f_pop = par->ui->fibNumPop->value();
      fiber->pop_subtype = FIBER;
      strm1 << "Pop" << node_num
            << endl << fiber->f_pop << endl
            << fiber->f_prob;
   }
   else if (par->ui->fiberElectric->isChecked())
   {
      fiber->f_prob = 1;
      fiber->f_seed = par->ui->fiberElectricSeed->value();
      fiber->f_pop = 1; // always just one fiber in the population
      fiber->f_begin = par->ui->fiberElectricStartTime->value() * 1000.0;
      par->ui->fiberElectricStopTime->value() < 0 ?
             fiber->f_end = par->ui->fiberElectricStopTime->value() : 
             fiber->f_end = round(par->ui->fiberElectricStopTime->value()*1000.0); 
      fiber->pop_subtype = ELECTRIC_STIM;
      if (par->ui->fiberElectricDeterministic->isChecked())
         fiber->freq_type = STIM_FIXED;
      else
         fiber->freq_type = STIM_FUZZY;
      fiber->frequency = par->ui->fiberElectricFreq->value();
      fiber->fuzzy_range = par->ui->fiberElectricFuzzyRange->value();
      strm1 << "Pop" << node_num
            << endl << fiber->f_pop  << endl
            << "E Stim";
   }
   else if (par->ui->fiberAfferent->isChecked())
   {
      fiber->pop_subtype = AFFERENT;
      fiber->f_seed = par->ui->fiberAffSeed->value();
      fiber->f_begin = par->ui->fiberAfferentStartFire->value() * 1000.0;
      fiber->f_pop = par->ui->fiberAfferentNumPop->value();
      if (fiber->f_pop ==1) // see note above
         fiber->f_pop = 100;
      fiber->offset = par->ui->fiberAfferentOffset->value();
      par->ui->fiberAfferentStopFire->value() < 0 ?
             fiber->f_end = par->ui->fiberAfferentStopFire->value() : 
             fiber->f_end = round(par->ui->fiberAfferentStopFire->value()*1000.0); 
      strm1 << "Pop" << node_num
            << endl << fiber->f_pop  << endl
            << " AFF ";
      memset(fiber->afferent_file, 0, sizeof(fiber->afferent_file));
      memset(fiber->afferent_prog, 0, sizeof(fiber->afferent_prog));
      strncpy(fiber->afferent_file,par->ui->afferentFileName->text().toLatin1().data(),sizeof(fiber->afferent_file)-1);
      fiber->slope_scale = par->ui->fiberAfferentScale->value();
      par->affModelToRec(fiber);
   }
   else
      cout << "There is an unknown type of fiber pop in simscene" << endl;
   par->launch->adjustLaunchFibers();
   def_fiber.unode.fiber_node  = *fiber;

   memset(D.inode[d_idx].comment1, 0, sizeof(D.inode[d_idx].comment1));
   strncpy(D.inode[d_idx].comment1, par->ui->fibComment->text().toLatin1().data(),sizeof(D.inode[d_idx].comment1)-1);
   strm2 << D.inode[d_idx].comment1;
   FiberNode* fiberitem = dynamic_cast <FiberNode *>(getObjNode(d_idx));
   if (fiberitem)
      fiberitem->updateText(txt1,txt2);
   setFiberDirty(false);
}


// Fetch values from struct and show in controls.
// If this is first time, use default values
// We need to know what parts of the rec to use (the price for overloading vars)
// but was also need to leave the pop_subtype alone incase the user wants to 
// undo later. Most callers don't care, but checkbox handlers do.
void SimScene::cellRecToUi(int d_idx, int selector)
{
   C_NODE* cell = &D.inode[d_idx].unode.cell_node;
   int save_sub;

       // Note: some of the vars in the structs are overloaded,
       // so a var may hold a value that means very different things
       // depending on the cell type
   par->ui->cellTypes->clear();
   save_sub = cell->pop_subtype;
   if (selector > 0)
      cell->pop_subtype = selector;
   if (cell->pop_subtype == BURSTER_POP)
   {
      par->ui->cellTypes->addTab(par->ui->bursterTab,"Burster");
      par->ui->bursterCell->blockSignals(true);  // we don't want a click event
      par->ui->bursterCell->setChecked(true);
      par->ui->bursterCell->blockSignals(false);
      par->ui->bursterTimeConst->setValue(cell->c_accomodation);
      par->ui->bursterSlopeH->setValue(cell->c_thresh_remove_ika);
      par->ui->bursterHalfVAct->setValue(cell->c_thresh_active_ika);
      par->ui->bursterSlopeAct->setValue(cell->c_max_conductance_ika);
      par->ui->bursterResetV->setValue(cell->c_pro_deactivate_ika);
      par->ui->bursterCellPopSize->setValue(cell->c_pop);
      par->ui->bursterThreshV->setValue(cell->c_pro_activate_ika);
      par->ui->bursterHalfVH->setValue(cell->c_rebound_time_k);
      par->ui->bursterKCond->setValue(cell->c_ap_k_delta);
      par->ui->bursterDeltaH->setValue(cell->c_constant_ika);
      par->ui->bursterAppliedCurr->setValue(cell->c_dc_injected);
      par->ui->bursterNoiseAmp->setValue(cell->c_rebound_param);
      par->ui->cellComment->setInputMask("");
   }
   else if (cell->pop_subtype == PSR_POP)
   {
      par->ui->cellTypes->addTab(par->ui->psrTab,"PSR");
      par->ui->psrCell->blockSignals(true);
      par->ui->psrCell->setChecked(true);
      par->ui->psrCell->blockSignals(false);
      par->ui->psrRiseT->setValue(cell->c_accomodation);
      par->ui->psrFallT->setValue(cell->c_k_conductance);
      par->ui->psrOutThresh->setValue(cell->c_resting_thresh);
      par->ui->psrOutThreshSd->setValue(cell->c_resting_thresh_sd);
      par->ui->psrCellPopSize->setValue(cell->c_pop);
      par->ui->cellComment->setInputMask("");
   }
   else // standard cell or a cell subtype
   {
      par->ui->cellTypes->addTab(par->ui->cellTab,"Normal Cell");
      par->ui->cellAccom->setValue(cell->c_accomodation);
      par->ui->cellMembTC->setValue(cell->c_mem_potential);
      par->ui->cellRestThresh->setValue(cell->c_resting_thresh);
      par->ui->cellRestThreshSD->setValue(cell->c_resting_thresh_sd);
      par->ui->cellKCond->setValue(cell->c_k_conductance);
      par->ui->cellKCondChg->setValue(cell->c_ap_k_delta);
      par->ui->cellAccomParam->setValue(cell->c_accom_param);
      par->ui->cellPopSize->setValue(cell->c_pop);
      par->ui->cellDCInj->setValue(cell->c_dc_injected);
      par->ui->injectedExpression->setText(cell->c_injected_expression);
      par->ui->noiseAmp->setValue(cell->c_rebound_param);
       // Some subtypes require specific text as part of the comment.
       // The mask more or less enforces this. Some types need sequence
       // number if more than using more than one population. This is not
       // enforced.
      if (cell->pop_subtype == PHRENIC_POP)
      {
         par->ui->cellTypes->addTab(par->ui->cellTab,"Lung Phrenic Cell");
         par->ui->lungPhrenicCell->blockSignals(true);
         par->ui->lungPhrenicCell->setChecked(true);
         par->ui->lungPhrenicCell->blockSignals(false);
         par->ui->cellComment->setInputMask("\\P\\H\\R\\E\\N\\I\\CXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
         par->ui->cellComment->setText("PHRENIC");
      }
      else if (cell->pop_subtype == LUMBAR_POP)
      {
         par->ui->cellTypes->addTab(par->ui->cellTab,"Lung Lumbar Cell");
         par->ui->lungLumbarCell->blockSignals(true);
         par->ui->lungLumbarCell->setChecked(true);
         par->ui->lungLumbarCell->blockSignals(false);
         par->ui->cellComment->setInputMask("\\L\\U\\M\\B\\A\\R\\CXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
         par->ui->cellComment->setText("LUMBAR");
      }
      else if (cell->pop_subtype == INSP_LAR_POP)
      {
         par->ui->cellTypes->addTab(par->ui->cellTab,"Inspiratory Laryngeal Cell");
         par->ui->lungInspiLarCell->blockSignals(true);
         par->ui->lungInspiLarCell->setChecked(true);
         par->ui->lungInspiLarCell->blockSignals(false);
         par->ui->cellComment->setInputMask("\\I\\L\\MXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
         par->ui->cellComment->setText("ILM");
      }
      else if (cell->pop_subtype == EXP_LAR_POP)
      {
         par->ui->cellTypes->addTab(par->ui->cellTab,"Expiratory Laryngeal Cell");
         par->ui->lungExpirLarCell->blockSignals(true);
         par->ui->lungExpirLarCell->setChecked(true);
         par->ui->lungExpirLarCell->blockSignals(false);
         par->ui->cellComment->setInputMask("\\E\\L\\MXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
         par->ui->cellComment->setText("ELM");
      }
      else
      {
         par->ui->cellTypes->addTab(par->ui->cellTab,"Normal Cell");
         par->ui->standardCell->blockSignals(true); // if all else fails, a standard cell
         par->ui->standardCell->setChecked(true);
         par->ui->standardCell->blockSignals(false);
         par->ui->cellComment->setInputMask("");
      }
   }
   par->ui->cellComment->setText(D.inode[d_idx].comment1);
   par->ui->cellPopNum->setText(QString::number(D.inode[d_idx].node_number));
   par->ui->cellGroup->setText(D.inode[d_idx].unode.cell_node.group);
   par->ui->cellObjId->setText(QString::number(d_idx));
   cell->pop_subtype = save_sub;
   setCellDirty(false);
}

// Copy controls to D.
// Update the def_cell values to current.
void SimScene::cellUiToRec()
{
   int d_idx = par->ui->cellObjId->text().toInt();
   C_NODE* cell = &D.inode[d_idx].unode.cell_node;

       // Note: some of the vars in the structs are overloaded,
       // so a var may hold a value that means very different things
       // depending on the node type, the name of the var sometimes has
       // nothing to do with the value it holds.
   if (par->ui->bursterCell->isChecked())
   {
      cell->c_k_conductance = 0;  // flag for burster
      if (cell->c_mem_potential == 0)
         cout << "unexpected psr state" << endl;
      cell->c_rebound_time_k = par->ui->bursterHalfVH->value();
      cell->c_thresh_remove_ika = par->ui->bursterSlopeH->value();
      cell->c_thresh_active_ika = par->ui->bursterHalfVAct->value();
      cell->c_max_conductance_ika = par->ui->bursterSlopeAct->value();
      cell->c_pro_deactivate_ika = par->ui->bursterResetV->value();
      cell->c_pro_activate_ika = par->ui->bursterThreshV->value();
      cell->c_constant_ika = par->ui->bursterDeltaH->value();
      cell->c_accomodation = par->ui->bursterTimeConst->value();
      cell->c_ap_k_delta = par->ui->bursterKCond->value();
      cell->c_pop = par->ui->bursterCellPopSize->value();
      cell->c_dc_injected = par->ui->bursterAppliedCurr->value();
      cell->c_rebound_param = par->ui->bursterNoiseAmp->value();
      cell->pop_subtype = BURSTER_POP;
      par->ui->cellComment->setInputMask("");
      def_cell.unode.cell_node = *cell;
   }
   else if (par->ui->psrCell->isChecked())
   {
      if (cell->c_k_conductance == 0)
         cout << "unexpected burster state" << endl;
      cell->c_mem_potential = 0; // flag for PSR, not used but kept for legacy reasons
      cell->c_accomodation = par->ui->psrRiseT->value();
      cell->c_k_conductance = par->ui->psrFallT->value();
      cell->c_resting_thresh = par->ui->psrOutThresh->value();
      cell->c_resting_thresh_sd = par->ui->psrOutThreshSd->value();
      cell->c_pop = par->ui->psrCellPopSize->value();
      cell->pop_subtype = PSR_POP;
   }
   else if (par->ui->standardCell->isChecked() ||
            par->ui->lungPhrenicCell->isChecked() ||
            par->ui->lungLumbarCell->isChecked() ||
            par->ui->lungInspiLarCell->isChecked() ||
            par->ui->lungExpirLarCell->isChecked() )
   {
      cell->c_accomodation = par->ui->cellAccom->value();
      cell->c_mem_potential = par->ui->cellMembTC->value();
      cell->c_resting_thresh = par->ui->cellRestThresh->value();
      cell->c_resting_thresh_sd = par->ui->cellRestThreshSD->value();
      cell->c_k_conductance = par->ui->cellKCond->value();
      cell->c_ap_k_delta = par->ui->cellKCondChg->value();
      cell->c_accom_param = par->ui->cellAccomParam->value();
      cell->c_pop = par->ui->cellPopSize->value();
      cell->c_rebound_param = par->ui->noiseAmp->value();
      cell->c_dc_injected = par->ui->cellDCInj->value();
      if (par->ui->injectedExpression->text().length())
         cell->c_injected_expression = strdup(par->ui->injectedExpression->text().toLatin1().data());
      else
         cell->c_injected_expression = nullptr;
      if (par->ui->standardCell->isChecked())
         cell->pop_subtype = CELL;
      else if (par->ui->lungPhrenicCell->isChecked())
         cell->pop_subtype = PHRENIC_POP;
      else if (par->ui->lungLumbarCell->isChecked())
         cell->pop_subtype = LUMBAR_POP;
      else if (par->ui->lungInspiLarCell->isChecked())
         cell->pop_subtype = INSP_LAR_POP;
      else if (par->ui->lungExpirLarCell->isChecked())
         cell->pop_subtype = EXP_LAR_POP;
   }
   else
      cout << "No cell radio button selected" << endl;  // "could never happen"
   def_cell.unode.cell_node = *cell;

   memset(D.inode[d_idx].comment1, 0,sizeof(D.inode[d_idx].comment1));
   strncpy(D.inode[d_idx].comment1, par->ui->cellComment->text().toLatin1().data(),sizeof(D.inode[d_idx].comment1)-1);

   QString txt1, txt2;
   QTextStream strm1(&txt1);
   QTextStream strm2(&txt2);
   int node_num = D.inode[d_idx].node_number;
   strm1 << "Pop" << node_num << endl
         << D.inode[d_idx].unode.cell_node.c_pop;
   strm2 << D.inode[d_idx].comment1;
   CellNode* cellitem = dynamic_cast <CellNode *>(getObjNode(d_idx));
   if (cellitem)
      cellitem->updateText(txt1,txt2);
   setCellDirty(false);
}

// Add a new synapse, find a free slot in the source record
// and set initial values.
void SimScene::addSynapse(Connector& rec)
{
   int source_num, target_num, mode;
   bool found_slot = false;
   int xloop;
   int source_entry = 0;

   FiberNode* fiber = dynamic_cast <FiberNode *>(rec.start);
   CellNode* cell = dynamic_cast <CellNode *>(rec.start);
   CellNode* end = dynamic_cast <CellNode *>(rec.end);

   if (fiber)
   {
      source_num = fiber->getDidx();
      mode = FIBER;
   }
   else if (cell)
   {
      source_num = cell->getDidx();
      mode = CELL;
   }
   else
   {
      cout << "not enough info to add synapse" << endl;
      return;
   }

   if (end)
      target_num = D.inode[end->getDidx()].node_number;
   else
   {
      cout << "not enough info to add synapse" << endl;
      return;
   }
   par->ui->synapseObjId->setText(QString::number(source_num) + " " + QString::number(target_num));

   if (mode == CELL)
   {
      for (xloop = FIRST_TARGET_IDX; xloop < SYNAPSE_INODE; xloop++)
      {    // find next unused slot
         if (D.inode[source_num].unode.cell_node.c_target_nums[xloop] == 0)
         {
            source_entry = xloop;
            found_slot = true;
            break;
         }
      }
      if (found_slot)
      {
         rec.setTargIdx(source_entry);
         if (source_entry >= D.inode[GLOBAL_INODE].unode.global_node.max_targets)
            D.inode[GLOBAL_INODE].unode.global_node.max_targets++;
         D.inode[source_num].unode.cell_node.c_targets++;
         D.inode[source_num].unode.cell_node.c_target_nums[source_entry]=target_num;
               // use current values from cell page as defaults
         D.inode[source_num].unode.cell_node.c_min_conduct_time[source_entry] =
                 par->ui->synapseMinCond->value();
         D.inode[source_num].unode.cell_node.c_conduction_time[source_entry] =
                 par->ui->synapseMaxCond->value();
         D.inode[source_num].unode.cell_node.c_terminals[source_entry] =
                 par->ui->synapseNumTerm->value();
         QModelIndex syn_row = par->ui->synapseList->currentIndex();
         int syn_type;
         if (syn_row.row()>=0)
            syn_type = syn_row.sibling(syn_row.row(),SYN_NUM).data().toInt();
         else
         {
            cout << "warning, synapse row not selected and it should be" << endl;
            syn_type = 1; // todo  write findFirstSyn lookup
         }
         D.inode[source_num].unode.cell_node.c_synapse_type[source_entry] = syn_type;
         D.inode[source_num].unode.cell_node.c_synapse_strength[source_entry] =
                 par->ui->synapseSynStren->value();
         D.inode[source_num].unode.cell_node.c_target_seed[source_entry] =
                 par->ui->synapseRndSeed->value();
      }
   }
   else if (mode == FIBER)
   {
      for (xloop = FIRST_TARGET_IDX; xloop < SYNAPSE_INODE; xloop++)
      {
         if (D.inode[source_num].unode.fiber_node.f_target_nums[xloop] == 0)
         {
            source_entry = xloop;
            found_slot = true;
            break;
         }
      }

      if (found_slot)
      {
         rec.setTargIdx(source_entry);
         if (source_entry >= D.inode[GLOBAL_INODE].unode.global_node.max_targets)
            D.inode[GLOBAL_INODE].unode.global_node.max_targets++;
         D.inode[source_num].unode.fiber_node.f_targets++;
         D.inode[source_num].unode.fiber_node.f_target_nums[source_entry]=target_num;
         D.inode[source_num].unode.fiber_node.f_min_conduct_time[source_entry] =
                 par->ui->synapseMinCond->value();
         D.inode[source_num].unode.fiber_node.f_conduction_time[source_entry] =
                 par->ui->synapseMaxCond->value();
         D.inode[source_num].unode.fiber_node.f_terminals[source_entry] =
                 par->ui->synapseNumTerm->value();
         QModelIndex syn_row = par->ui->synapseList->currentIndex();
         int syn_type;
         if (syn_row.row()>=0)
            syn_type = syn_row.sibling(syn_row.row(),SYN_NUM).data().toInt();
         else
         {
            cout << "warning, synapse row not selected and it should be" << endl;
            syn_type = 1;
         }
         D.inode[source_num].unode.fiber_node.f_synapse_type[source_entry] = syn_type;
         D.inode[source_num].unode.fiber_node.f_synapse_strength[source_entry] =
                 par->ui->synapseSynStren->value();
         D.inode[source_num].unode.fiber_node.f_target_seed[source_entry] =
                 par->ui->synapseRndSeed->value();
      }
   }
   else
      cout << "Unexpected object in addSynapose that is not cell or fiber" << endl;
}

// update database D rec for this synapse
void SimScene::deleteSynapse(Connector& rec)
{
   int source_num, mode;
   int source_entry, targ_num;
   Conn delList;
   delList.push_back(&rec);

   FiberNode* fiber = dynamic_cast <FiberNode *>(rec.start);
   CellNode* cell = dynamic_cast <CellNode *>(rec.start);
   CellNode* end = dynamic_cast <CellNode *>(rec.end); // end always a cell
   if (fiber)
   {
      source_num = fiber->getDidx();  // D array index
      mode = FIBER;
   }
   else if (cell)
   {
      source_num = cell->getDidx();
      mode = CELL;
   }
   else
      mode = UNUSED;
   source_entry = rec.getTargIdx();  // offset into target arrays

   if (mode == CELL)
   {
      targ_num = D.inode[source_num].unode.cell_node.c_target_nums[source_entry];
      if (targ_num)
      {
         targ_num = D.inode[source_num].unode.cell_node.c_target_nums[source_entry];
         D.inode[source_num].unode.cell_node.c_targets--;
         D.inode[source_num].unode.cell_node.c_target_nums[source_entry] = 0;
         D.inode[source_num].unode.cell_node.c_min_conduct_time[source_entry] = 0;
         D.inode[source_num].unode.cell_node.c_conduction_time[source_entry] = 0;
         D.inode[source_num].unode.cell_node.c_terminals[source_entry] = 0;
         D.inode[source_num].unode.cell_node.c_synapse_type[source_entry] = 0;
         D.inode[source_num].unode.cell_node.c_synapse_strength[source_entry] = 0;
         D.inode[source_num].unode.cell_node.c_target_seed[source_entry] = 0;
      }
      if (cell)
         cell->removeLines(delList);
      if (end)
         end->removeLines(delList);
   }
   else if (mode == FIBER)
   {
      targ_num = D.inode[source_num].unode.fiber_node.f_target_nums[source_entry];
      if (targ_num)
      {
         D.inode[source_num].unode.fiber_node.f_targets--;
         D.inode[source_num].unode.fiber_node.f_target_nums[source_entry] = 0;
         D.inode[source_num].unode.fiber_node.f_min_conduct_time[source_entry] = 0;
         D.inode[source_num].unode.fiber_node.f_conduction_time[source_entry] = 0;
         D.inode[source_num].unode.fiber_node.f_terminals[source_entry] = 0;
         D.inode[source_num].unode.fiber_node.f_synapse_type[source_entry] = 0;
         D.inode[source_num].unode.fiber_node.f_synapse_strength[source_entry] = 0;
         D.inode[source_num].unode.fiber_node.f_target_seed[source_entry] = 0;
      }
      if (fiber)
         fiber->removeLines(delList);
      if (end)
         end->removeLines(delList);
   }
}



// copy current values into gui controls
void SimScene::synapseRecToUi(connRec& rec)
{
   int min_cond, cond_time, syn_type, seed;
   short terminals;
   QString syn_name;
   float syn_strength;

   int source_num = rec.source_num.toInt();  // D idx
   int target_num = rec.target_num.toInt();  // pop num in c_target_nums array
   int mode = rec.mode.toInt();
   int source_entry;

   par->ui->synapseObjId->setText(QString::number(source_num) + " " + QString::number(target_num));
   source_entry = rec.lineitem->getTargIdx();
   if (mode == CELL)
   {
      min_cond = D.inode[source_num].unode.cell_node.c_min_conduct_time[source_entry];
      cond_time = D.inode[source_num].unode.cell_node.c_conduction_time[source_entry];
      terminals = D.inode[source_num].unode.cell_node.c_terminals[source_entry];
      syn_type = D.inode[source_num].unode.cell_node.c_synapse_type[source_entry];
      syn_strength = D.inode[source_num].unode.cell_node.c_synapse_strength[source_entry];
      seed = D.inode[source_num].unode.cell_node.c_target_seed[source_entry];
      syn_name = D.inode[SYNAPSE_INODE].unode.synapse_node.synapse_name[syn_type];
   }
   else if (mode == FIBER)
   {
      min_cond = D.inode[source_num].unode.fiber_node.f_min_conduct_time[source_entry];
      cond_time = D.inode[source_num].unode.fiber_node.f_conduction_time[source_entry];
      terminals = D.inode[source_num].unode.fiber_node.f_terminals[source_entry];
      syn_type = D.inode[source_num].unode.fiber_node.f_synapse_type[source_entry];
      syn_strength = D.inode[source_num].unode.fiber_node.f_synapse_strength[source_entry];
      seed = D.inode[source_num].unode.fiber_node.f_target_seed[source_entry];
      syn_name = D.inode[SYNAPSE_INODE].unode.synapse_node.synapse_name[syn_type];
   }
  else
  {
     cout << "Unexpected object in addSynapseRecToUi that is not cell or fiber" << endl;
     min_cond = cond_time = syn_type = seed = terminals = syn_strength = 0;
   }
   par->ui->synapseMinCond->setValue(min_cond);
   par->ui->synapseMaxCond->setValue(cond_time);
   par->ui->synapseNumTerm->setValue(terminals);
   par->ui->synapseSynStren->setValue(syn_strength);
   par->ui->synapseRndSeed->setValue(seed);
   par->ui->synFrom->setText(rec.axonPop);
   par->ui->synTo->setText(rec.synPop);

   QModelIndex idx = par->ui->synapseList->indexFromType(syn_type);
   par->ui->synapseList->expandAll();
   par->ui->synapseList->setCurrentIndex(idx);
   par->ui->synapseList->scrollTo(idx);
   if (rec.lineitem)
   {
      QColor color = synapseColors[syn_type-1];
      bool excite = D.inode[SYNAPSE_INODE].unode.synapse_node.s_eq_potential[syn_type]>0;
      rec.lineitem->setColor(color);
      rec.lineitem->setExcite(excite);
      rec.lineitem->setSynType(syn_type);
   }

   setSynapseDirty(false);
   invalidate(sceneRect());
}


// copy control values to vars
void SimScene::synapseUiToRec()
{
   if (!selectedConn.lineitem)
      return;

   int source_num, mode, source_entry, syn_type;

   source_num = selectedConn.source_num.toInt();  // D idx
   mode = selectedConn.mode.toInt();
   source_entry = selectedConn.lineitem->getTargIdx();
   QModelIndex syn_row = par->ui->synapseList->currentIndex();
   syn_type = syn_row.sibling(syn_row.row(),SYN_NUM).data().toInt();
   if (mode == CELL)
   {
      source_entry = selectedConn.lineitem->getTargIdx();
      D.inode[source_num].unode.cell_node.c_min_conduct_time[source_entry] =
              par->ui->synapseMinCond->value();
      D.inode[source_num].unode.cell_node.c_conduction_time[source_entry] =
              par->ui->synapseMaxCond->value();
      D.inode[source_num].unode.cell_node.c_terminals[source_entry] =
              par->ui->synapseNumTerm->value();
      D.inode[source_num].unode.cell_node.c_synapse_type[source_entry] = syn_type;
      D.inode[source_num].unode.cell_node.c_synapse_strength[source_entry] =
              par->ui->synapseSynStren->value();
      D.inode[source_num].unode.cell_node.c_target_seed[source_entry] =
              par->ui->synapseRndSeed->value();
   }
   else if (mode == FIBER)
   {
      source_entry = selectedConn.lineitem->getTargIdx();
      D.inode[source_num].unode.fiber_node.f_min_conduct_time[source_entry] =
              par->ui->synapseMinCond->value();
      D.inode[source_num].unode.fiber_node.f_conduction_time[source_entry] =
              par->ui->synapseMaxCond->value();
      D.inode[source_num].unode.fiber_node.f_terminals[source_entry] =
              par->ui->synapseNumTerm->value();
      D.inode[source_num].unode.fiber_node.f_synapse_type[source_entry] = syn_type;
      D.inode[source_num].unode.fiber_node.f_synapse_strength[source_entry] =
              par->ui->synapseSynStren->value();
      D.inode[source_num].unode.fiber_node.f_target_seed[source_entry] =
              par->ui->synapseRndSeed->value();
   }
   setSynapseDirty(false);
}


// Set this if there are any changes that should be
// save to a file. Some of the dirty flags just control
// coping values to the DB.
void SimScene::setFileDirty(bool flag)
{
   fileDirty = flag;
}

void SimScene::sceneMove()
{
   setFileDirty(true);
}


void SimScene::setSendControls()
{
   par->ui->cellSendLaunchBdt->setEnabled(!cellDirty);
   par->ui->cellSendLaunchPlot->setEnabled(!cellDirty);
   par->ui->fiberSendLaunchBdt->setEnabled(!fiberDirty);
   par->ui->fiberSendLaunchPlot->setEnabled(!fiberDirty);
}

void SimScene::setCellDirty(bool flag)
{
   cellDirty = flag;
   par->ui->cellUndo->setEnabled(flag);
   par->ui->cellApply->setEnabled(flag);
   setSendControls();
}

void SimScene::setFiberDirty(bool flag)
{
   fiberDirty = flag;
   par->ui->fiberUndo->setEnabled(flag);
   par->ui->fiberApply->setEnabled(flag);
   setSendControls();
}

void SimScene::setSynapseDirty(bool flag)
{
   synapseDirty = flag;
   par->ui->synapseUndo->setEnabled(flag);
   par->ui->synapseApply->setEnabled(flag);
}

void SimScene::setBuildDirty(bool flag)
{
   buildDirty = flag;
   par->ui->buildUndo->setEnabled(flag);
   par->ui->buildApply->setEnabled(flag);
}

void SimScene::setGlobalsDirty(bool flag)
{
   globalsDirty = flag;
   par->ui->globUndo->setEnabled(flag);
   par->ui->globApply->setEnabled(flag);
}

bool SimScene::allChkDirty()
{
   chkControlsDirty();
   return fileChkDirty();
}

// If saving a file, the file dirty check is not required and leads
// to a confusing dialog box.
void SimScene::chkControlsDirty()
{
   fiberChkDirty();   // make sure all controls are applied or undone
   cellChkDirty();
   synapseChkDirty();
   buildChkDirty();
}

void SimScene::clearAllDirty()
{
   setFiberDirty(false);
   setCellDirty(false);
   setSynapseDirty(false);
   setBuildDirty(false);
   setGlobalsDirty(false);
   setFileDirty(false);
}


// A synapse type has possibly changed.
// scan the conn recs and udate the excititory state
void SimScene::updateExcite()
{
   Connector* conn;
   int idx,excite;
   S_NODE *s = &D.inode[SYNAPSE_INODE].unode.synapse_node;

   auto list = items();
   foreach (QGraphicsItem *item, list)
   {
      conn = dynamic_cast <Connector *>(item);
      if (conn)
      {
         idx = conn->synapseType;
         excite = s->s_eq_potential[idx];
         if (excite >= 0)
            conn->setExcite(true);
         else
            conn->setExcite(false);
      }
   }
   invalidate();
}

// Part of the file saving flow, prepare various data structures prior to
// actually saving the info to file(s).
void SimScene::saveSnd()
{
   setPrePost();
   coordsToRec();
   get_maxes(&D);
}

void SimScene::setPrePost()
{
   S_NODE *s = &D.inode[SYNAPSE_INODE].unode.synapse_node;
   int node;
   bool havePrePost = false;
   for (node = 1; node < TABLE_LEN; ++node) // index 0 unused for synapse node
   {
      if (s->syn_type[node] == SYN_PRE || s->syn_type[node] == SYN_POST)
      {
         havePrePost = true;
         break;
      }
   }
   D.presynaptic_flag = havePrePost;
}

void SimScene::showCellRetro(bool onoff)
{
   setRAMode(onoff);
   auto list = selectedItems();
   if (list.size() == 1)
   {
      auto item = list.cbegin();
      CellNode* cell_item = dynamic_cast <CellNode *>(*item);

      if (cell_item)
      {
         connDir dir;
         if (onoff)
            dir = connDir::RETRO; // this will force the connectors to top
         else
         {
            dir = connDir::NONE;
            lineParent->setZValue(connOrder); // connector back to current order
         }
         connIter iter;
         for (iter = cell_item->toLines.begin(); iter != cell_item->toLines.end(); ++iter)
            (*iter)->setSrcTarg(dir);
         for (iter = cell_item->fromLines.begin(); iter != cell_item->fromLines.end(); ++iter)
            (*iter)->setSrcTarg(connDir::NONE);
      }
      invalidate(sceneRect());
   }
}


void SimScene::showCellAntero(bool onoff)
{
   setRAMode(onoff);
   auto list = selectedItems();
   if (list.size() == 1)
   {
      auto item = list.cbegin();
      CellNode* cell_item = dynamic_cast <CellNode *>(*item);

      if (cell_item)
      {
         connDir dir;
         if (onoff)
            dir = connDir::ANTERO;
         else
         {
            dir = connDir::NONE;
            lineParent->setZValue(connOrder); // connector back to current order
         }
         connIter iter;
         for (iter = cell_item->fromLines.begin(); iter != cell_item->fromLines.end(); ++iter)
            (*iter)->setSrcTarg(dir);
         for (iter = cell_item->toLines.begin(); iter != cell_item->toLines.end(); ++iter)
            (*iter)->setSrcTarg(connDir::NONE);
      }
      invalidate(sceneRect());
   }
}

void SimScene::showFiberTarg(bool onoff)
{
   setRAMode(onoff);

   auto list = selectedItems();
   if (list.size() == 1)
   {
      auto item = list.cbegin();
      FiberNode* fiber_item = dynamic_cast <FiberNode *>(*item);

      if (fiber_item)
      {
         connDir dir;
         if (onoff)
            dir = connDir::RETRO;
         else
            dir = connDir::NONE;
         for (connIter iter = fiber_item->fromLines.begin(); iter != fiber_item->fromLines.end(); ++iter)
         (*iter)->setSrcTarg(dir);
      }
      invalidate(sceneRect());
   }
}

void SimScene::toggleLines(bool ontop)
{
   if (ontop)
      connOrder = onTopZ;
   else
      connOrder = connZ;
   lineParent->setZValue(connOrder);
}

void SimScene::setRAMode(bool mode)
{
   auto allobjs = items();
   foreach (QGraphicsItem *item, allobjs)
   {
      Connector *conn = dynamic_cast <Connector *>(item);
      if (conn)
         conn->setRAMode(mode);
   }
}

void SimScene::selAll()
{
   QList<QGraphicsItem*> allobjs = items();
      foreach(QGraphicsItem *item, allobjs)
         item->setSelected(true);
}

void SimScene::deSelAll()
{
   QList<QGraphicsItem*> allobjs = items();
      foreach(QGraphicsItem *item, allobjs)
         item->setSelected(false);
}

BaseNode* SimScene::getObjNode(int d_idx)
{
   BaseNode* found = nullptr;
   QList<QGraphicsItem*> allobjs = items();
      foreach(QGraphicsItem *item, allobjs)
      {
         found = dynamic_cast <BaseNode*>(item);
         if (found && found->getDidx() == d_idx)
            break;
      }
   return found;
}


void SimScene::outOfNodes()
{
   QMessageBox msgBox;
   msgBox.setIcon(QMessageBox::Critical);
   msgBox.setWindowTitle("Out Of Nodes");
   msgBox.setStandardButtons(QMessageBox::Ok);
   msgBox.setText("There are no more free nodes, node not added.");
   msgBox.exec();
}


int SimScene::chkSynInUse(int syn_num)
{
   int src;
   int inuse = 0;

   for (src = FIRST_INODE; src <= LAST_INODE; ++src)
   {
      if (D.inode[src].node_type == FIBER)
      {
         F_NODE *f = &D.inode[src].unode.fiber_node;
         int f_t = f->f_targets;
         for (int i = 0; i < f_t ; ++i)
            if (f->f_synapse_type[i] == syn_num)
               ++inuse;
      }
      else if (D.inode[src].node_type == CELL)
      {
         C_NODE *c = &D.inode[src].unode.cell_node;
         int c_t = c->c_targets;
         for (int i = 0; i < c_t ; ++i)
            if (c->c_synapse_type[i] == syn_num)
               ++inuse;
      }
   }
   return inuse;
}


void SimScene::deleteSynType(int syn_num)
{
   auto all = items();
   Connector* synapse_item;
   F_NODE* f;
   C_NODE* c;
   int src, f_t, c_t;
   Conn delList;

   deSelAll();
   for (auto item = all.begin(); item != all.end(); ++item)
   {
       // get a list of all graphical object synapes of this type
      synapse_item = dynamic_cast <Connector *>(*item);
      if (synapse_item && synapse_item->synapseType == syn_num)
            delList.push_back(synapse_item);
   }
      // remove them from any basenode that uses the connector
   for (auto item = all.begin(); item != all.end(); ++item)
   {
      BaseNode* base_item = dynamic_cast <BaseNode *>(*item);
      if (base_item)
         for (auto lines = delList.begin(); lines != delList.end(); ++lines)
            base_item->removeLines(delList);
    }
    for (connIter lines = delList.begin(); lines != delList.end(); ++lines)
    {
       (*lines)->prepareGeometryChange();
       deleteSynapse(**lines);
       delete *lines;
    }

     // remove all refs
   for (src = FIRST_INODE; src <= LAST_INODE; ++src)
   {
      if (D.inode[src].node_type == FIBER)
      {
         f = &D.inode[src].unode.fiber_node;
         f_t = f->f_targets;
         for (int i = 0; i < f_t ; ++i)
            if (f->f_synapse_type[i] == syn_num)
               f->f_synapse_type[i] = 0;

      }
      else if (D.inode[src].node_type == CELL)
      {
         c = &D.inode[src].unode.cell_node;
         c_t = c->c_targets;
         for (int i = 0; i < c_t ; ++i)
            if (c->c_synapse_type[i] == syn_num)
            c->c_synapse_type[i] = 0;
      }
   }
   setBuildDirty(false);
   invalidate(sceneRect());
}

int SimScene::countSynapses()
{
   int node;
   int count = 0;
   S_NODE *s = &D.inode[SYNAPSE_INODE].unode.synapse_node;

   for (node = 1; node < TABLE_LEN; ++node) // index 0 unused for synapse node
      if (s->syn_type[node] != SYN_NOT_USED)
         ++count;
   return count;
}

void SimScene::fiberTypeFixed()
{
   setFiberDirty(true);
}

void SimScene::fiberTypeFuzzy()
{
   setFiberDirty(true);
}

// Find the item with this D structure index and scroll display so
// it is centered on it, or, at least, in view. Can't center stuff near
// an edge.
void SimScene::centerOn(int d_idx)
{
   BaseNode* node = getObjNode(d_idx);
   if (node && views().first())
   {
      deSelAll();
      views().first()->centerOn(node);
      node->setSelected(true);
      mngTabs();
   }
}

bool SimScene::isMono()
{
   return par->showMono;
}

void SimScene::doMonochrome(bool on_off)
{
   if (on_off)
   {
      sceneFG = defSceneBG;
      sceneBG = defSceneFG;
   }
   else
   {
      sceneFG = defSceneFG;
      sceneBG = defSceneBG;
   }
   setBackgroundBrush(sceneBG);
   updateInfoText();
   invalidate(sceneRect());
}



