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



// Functions for managing groups and exporting/importing part of a model to/from files.

#include <stdlib.h>
#include <QMessageBox>
#include <QInputDialog>
#include <memory>
#include "simwin.h"
#include "simscene.h"
#include "sim2build.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "simulator.h"
#include "inode.h"
#include "fileio.h"
#include "inode_hash.h"
#ifdef __cplusplus
}
#endif

#include "c_globals.h"
#include "sim2build.h"

using namespace std;

// Create a group from the selected items. If we find one that is already in a
// group, put the name in the popup.
void SimScene::createGroup()
{
   FiberNode* fiber_item;
   CellNode* cell_item;
   connIter iter;
   int d_idx;
   bool ok, have_name = false;
   graphicsItemList list;
   QString gname;

   // Build list of just selected cells and fibers.
   auto sel_items = selectedItems();
   foreach (QGraphicsItem* item, sel_items)   // std::list is easier to deal with
   {                                          // than QList.
      fiber_item = dynamic_cast <FiberNode *>(item);
      cell_item = dynamic_cast <CellNode *>(item);
      if (fiber_item || cell_item)
         list.push_back(item);
   }
   if (!list.size())
   {
      QMessageBox msgBox;
      msgBox.setIcon(QMessageBox::Warning);
      msgBox.setWindowTitle("No Cell or Fiber Nodes Selected.");
      msgBox.setStandardButtons(QMessageBox::Ok);
      msgBox.setText("Select one or more Cell or Fiber nodes to create a group.");
      msgBox.exec();
      return;
   }
   foreach (QGraphicsItem *item, list)  // First item that already has a group name
   {                                    // provides default name.
      fiber_item = dynamic_cast <FiberNode *>(item);
      cell_item = dynamic_cast <CellNode *>(item);
      if (fiber_item)
      {
         d_idx = fiber_item->getDidx();
         if (strlen(D.inode[d_idx].unode.fiber_node.group))
            gname = D.inode[d_idx].unode.fiber_node.group;
         break;
      }
      else if (cell_item)
      {
         d_idx = cell_item->getDidx();
         if (strlen(D.inode[d_idx].unode.cell_node.group))
            gname = D.inode[d_idx].unode.cell_node.group;
         break;
      }
   }

   do
   {
      gname = QInputDialog::getText(nullptr, tr("Enter Group Name"),
                                    tr("Group Name:"), QLineEdit::Normal,
                                    gname, &ok);
      if (!ok) // cancel button
         return;
      if (!gname.size())
      {
         QMessageBox msgBox;
         msgBox.setIcon(QMessageBox::Warning);
         msgBox.setWindowTitle("Please enter a group name.");
         msgBox.setStandardButtons(QMessageBox::Ok);
         msgBox.setText("The selected items need a group name.");
         msgBox.exec();
      }
      else
         have_name = true;
   } while (!have_name);

   if (gname.size() > GRPLEN-1)
       gname = gname.chopped(GRPLEN-1);
   coordsToRec();  // make sure coords up to date
   foreach (QGraphicsItem *item, list)
   {
      fiber_item = dynamic_cast <FiberNode *>(item);
      cell_item = dynamic_cast <CellNode *>(item);
      if (fiber_item)
      {
         d_idx = fiber_item->getDidx();
         strncpy(D.inode[d_idx].unode.fiber_node.group,gname.toLatin1().data(),GRPLEN-1);
      }
      else if (cell_item)
      {
         d_idx = cell_item->getDidx();
         strncpy(D.inode[d_idx].unode.cell_node.group,gname.toLatin1().data(),GRPLEN-1);
      }
   }
   connInGroup(list);
   deSelAll();
   foreach (QGraphicsItem* item, list)
      item->setSelected(true);
   setFileDirty(true);
}


// Custom sorter for sorting items by inode numbers
bool comp_inode(QGraphicsItem* first, QGraphicsItem* second)
{
   CellNode* c_first = dynamic_cast <CellNode *>(first);
   CellNode* c_second = dynamic_cast <CellNode *>(second);
   FiberNode* f_first = dynamic_cast <FiberNode *>(first);
   FiberNode* f_second = dynamic_cast <FiberNode *>(second);

   if (c_first && c_second)
      return c_first->getDidx() < c_second->getDidx();
   else if (c_first && f_second)
      return c_first->getDidx() < f_second->getDidx();
   else if (f_first && c_second)
      return f_first->getDidx() < c_second->getDidx();
   else if (f_first && f_second)
      return f_first->getDidx() < f_second->getDidx();
   else
      return false; // not cell or fiber
}


// If only one node selected, show the group, else user has to click on
// one member of group.
void SimScene::selGroup()
{
   auto sel = selectedItems();
   if (sel.size() == 1)
   {
      FiberNode *fiber_item = dynamic_cast <FiberNode *>(sel[0]);
      CellNode* cell_item = dynamic_cast <CellNode *>(sel[0]);
      if (fiber_item || cell_item)
         showGroup = true;
      selectGroupChk();
      mngTabs();
   }
   else if (showGroup)
   {
      par->printMsg("Out of Group Select Mode.");
      showGroup=false;
   }
   else
   {
      par->printMsg("In Group Select Mode, click on any group member to select the group.");
      showGroup=true;
   }
}

// Remove item(s) from group. If more than one group selected, everybody gets removed.
void SimScene::unGroup()
{
   string gname, match, msg;
   int didx;
   auto sel = selectedItems();
   set <string> matches;

   if (sel.size())
   {
      foreach (auto item, sel) // build list of group name(s)
      {
         FiberNode *fiber_item = dynamic_cast <FiberNode *>(item);
         CellNode* cell_item = dynamic_cast <CellNode *>(item);
         if (fiber_item) 
         {
            didx = fiber_item->getDidx();
            match = D.inode[didx].unode.fiber_node.group;
            matches.insert(match);
         }
         else if (cell_item) 
         {
            didx = cell_item->getDidx();
            match = D.inode[didx].unode.cell_node.group;
            matches.insert(match);
         }
      }
      if (!matches.size())
         return;
      msg = "Removing selected items from the ";
      foreach (string grp, matches)
         msg += grp + " ";
      msg += "group(s).";
      par->printMsg(QString::fromStdString(msg));
      foreach (auto item, sel)
      {
         FiberNode *fiber_item = dynamic_cast <FiberNode *>(item);
         CellNode* cell_item = dynamic_cast <CellNode *>(item);
         if (fiber_item) 
         {
            didx = fiber_item->getDidx();
            gname = D.inode[didx].unode.fiber_node.group;
            if (matches.find(gname) != matches.end())
               memset(D.inode[didx].unode.fiber_node.group,0,GRPLEN);
               
         }
         else if (cell_item) 
         {
            didx = cell_item->getDidx();
            gname = D.inode[didx].unode.cell_node.group;
            if (matches.find(gname) != matches.end())
               memset(D.inode[didx].unode.cell_node.group,0,GRPLEN);
         }
      }
      setFileDirty(true);
   }
}


// User has selected a node.  Get the clicked-on node, look up it's group name,
// then select everyone in that group. This means if a non-group item is
// selected, show all items not in a group.
void SimScene::selectGroupChk()
{
   QString gname, match;
   int didx;
   auto sel = selectedItems();
   graphicsItemList nodes;

   if (showGroup && sel.size() == 1)
   {
      FiberNode *fiber_item = dynamic_cast <FiberNode *>(sel[0]);
      CellNode* cell_item = dynamic_cast <CellNode *>(sel[0]);
      if (fiber_item) 
      {
         didx = fiber_item->getDidx();
         match = D.inode[didx].unode.fiber_node.group;
      }
      else if (cell_item) 
      {
         didx = cell_item->getDidx();
         match = D.inode[didx].unode.cell_node.group;
      }
      else
      {
         showGroup=false;
         return;
      }
      if (match.length())
         par->printMsg("Selecting the " + match + " group.");
      else
         par->printMsg("Selecting all nodes not in a group.");

      deSelAll();
      auto list = items();
      foreach (auto item, list)
      {
         FiberNode *fiber_item = dynamic_cast <FiberNode *>(item);
         CellNode* cell_item = dynamic_cast <CellNode *>(item);
         if (fiber_item) 
         {
            didx = fiber_item->getDidx();
            gname = D.inode[didx].unode.fiber_node.group;
            if (gname == match)
               nodes.push_back(item);
         }
         else if (cell_item) 
         {
            didx = cell_item->getDidx();
            gname = D.inode[didx].unode.cell_node.group;
            if (gname == match)
               nodes.push_back(item);
         }
      }
      connInGroup(nodes);
      deSelAll();
      foreach (auto item, nodes)
         item->setSelected(true);
   }
   showGroup=false;
}

// function to select nodes & conns in a group by name

// Given a list of graphics items in a group, add to list connectors
// where both endpoints (fibers or cells) are both in the list.
void SimScene::connInGroup(graphicsItemList& list)
{
  FiberNode *f_item;
  CellNode* c_item;
  connIter iter;
      // Include connectors that have both ends in the group.
   foreach (QGraphicsItem *item, list)
   {
      f_item = dynamic_cast <FiberNode *>(item);
      c_item = dynamic_cast <CellNode *>(item);
      if (f_item)
      {
         for (iter = f_item->fromLines.begin(); iter != f_item->fromLines.end(); ++iter)
            if (listContains(list,(*iter)->start) && listContains(list,(*iter)->end) &&
                !listContains(list,static_cast<QGraphicsItem*>(*iter))) // add once
                  list.push_back(*iter);

         for (iter = f_item->toLines.begin(); iter != f_item->toLines.end(); ++iter)
            if (listContains(list,(*iter)->start) && listContains(list,(*iter)->end) &&
                !listContains(list,static_cast<QGraphicsItem*>(*iter)))
                  list.push_back(*iter);
      }
      else if (c_item)
      {
         for (iter = c_item->fromLines.begin(); iter != c_item->fromLines.end(); ++iter)
            if (listContains(list,(*iter)->start) && listContains(list,(*iter)->end) &&
                   !listContains(list,static_cast<QGraphicsItem*>(*iter)))
                  list.push_back(*iter);

         for (iter = c_item->toLines.begin(); iter != c_item->toLines.end(); ++iter)
            if (listContains(list,(*iter)->start) && listContains(list,(*iter)->end) &&
                !listContains(list,static_cast<QGraphicsItem*>(*iter)))
                  list.push_back(*iter);
      }
   }
}


/*
   The idea here is to create an inode_global struct, like the global D struct,
   fill it up with the selected members, and export it to a subsystem file.
   Many things are not invariant, even absolute locations. I think it would be
   best to org the upper left most item at (0,0), adjust all (x,y) locations
   accordingly, adjust references to other pops starting at 1, same for
   axon/synapes. We save out enough info that the subsystem is runable as a
   .snd file. We do not export the bdt and plot selections. Have to create
   those from scratch if you want to run this stand-alone.
*/

// The first implementation of this was a many-pages long body of a single function.
// Broken up into pieces, but there is a lot of info required, so make them
// local statics to avoid passing 15 parameters.
static remapTarg remapFPop;
static remapTarg remapCPop;
static remapTarg remapSyn;
static exportObjs subTargets;
static int nextInode = 1;
static int expDx, expDy;
static int totalFibers;
static int totalCells;
static int totalSynTypes;


void SimScene::exportGroup()
{
   FiberNode* fiber_item;
   CellNode* cell_item;
   lineSegsIter iter;
   graphicsItemList group;

   auto sel = selectedItems();
   foreach (auto item, sel)
   {
      fiber_item = dynamic_cast <FiberNode *>(item);
      cell_item = dynamic_cast <CellNode *>(item);
      if (fiber_item || cell_item)
         group.push_back(item);
   }

   if (!group.size())
   {
      QMessageBox msgBox;
      msgBox.setIcon(QMessageBox::Warning);
      msgBox.setWindowTitle("No Items Selected to Export.");
      msgBox.setStandardButtons(QMessageBox::Ok);
      msgBox.setText("Select a group to export.");
      msgBox.exec();
      return;
   }
   coordsToRec();                    // make sure coords up to date
   unique_ptr<inode_global> subset;  // store everything in one of these
   subset = make_unique<inode_global>();
   for (int node=0; node<MAX_INODES; node++) // minimal init
   {
      subset->inode[node].node_type=UNUSED;
      strcpy(subset->inode[node].comment1,"This node is Unused");
   }
   remapFPop.clear();
   remapCPop.clear();
   remapSyn.clear();
   subTargets.clear();
   nextInode = 1;
   totalFibers = totalCells = totalSynTypes = 0;

   QRectF grpRect; // rectangle that includes all nodes
   foreach (QGraphicsItem *item, group)
      grpRect = grpRect.united(item->sceneBoundingRect());
   expDx = grpRect.x(); // UL corner
   expDy = grpRect.y();
   if (expDx < 0)     // old drawings sometimes have neg coordinates, make sure
      expDx = -expDx; // we start at (0,0)
   if (expDy < 0)
      expDy = -expDy;

   group.sort(comp_inode); // sort the list to be in inode order
   getExpTargs(group); // List of cell targets.
   remapObjs(group);   // Build lists of pops, targets, synapse types in group
                       // so we can remap various ID numbers
   foreach (QGraphicsItem *item, group)
   {
      fiber_item = dynamic_cast <FiberNode *>(item);
      cell_item = dynamic_cast <CellNode *>(item);
      if (fiber_item)
         exportFiber(fiber_item, *subset);
      else if (cell_item)
         exportCell(cell_item, *subset);
   }

   exportSynapses(*subset);
   exportGlobals(*subset);
   saveSubsystem(*subset);
}

// Create list of cell targets that are in export items
void SimScene::getExpTargs(graphicsItemList& group)
{
   CellNode* cell_item;
   int slot, pop;
   foreach (QGraphicsItem *item, group)
   {
      cell_item = dynamic_cast <CellNode *>(item);
      if (cell_item)
      {
         slot = cell_item->getDidx();
         pop = D.inode[slot].node_number;
         subTargets.insert(pop);
      }
   }
}


// Build lists of pops, targets, synapse types in group
// so we can remap various ID numbers.
void SimScene::remapObjs(graphicsItemList& group)
{
   FiberNode* fiber_item;
   CellNode* cell_item;
   int cpop, fpop;
   pair <remapIter,bool> added;
   int total_syns = 1; // index, not a count
   int slot, pop, par, tar, synapse_type, target_num;
   F_NODE *fiber;
   C_NODE *cell;
   S_NODE *syn_from = &D.inode[SYNAPSE_INODE].unode.synapse_node;

   cpop = fpop = 1;
   foreach (QGraphicsItem *item, group)
   {
      fiber_item = dynamic_cast <FiberNode *>(item);
      cell_item = dynamic_cast <CellNode *>(item);
      if (fiber_item)
      {
         ++totalFibers;
         slot = fiber_item->getDidx();
         pop = D.inode[slot].node_number;
         added = remapFPop.insert({pop,fpop});  // fpop will be new pop #
         if (added.second)
            ++fpop;
         fiber = &D.inode[slot].unode.fiber_node;
         for (tar = 0; tar <= LAST_INODE; tar++)
         {
            target_num = fiber->f_target_nums[tar];
            if (target_num == 0 || subTargets.find(target_num) == subTargets.end())
               continue;
            synapse_type = fiber->f_synapse_type[tar];
            added = remapSyn.insert({synapse_type,total_syns});
            if (added.second) 
            {
               ++total_syns; 
               ++totalSynTypes;
            }
            if (syn_from->syn_type[synapse_type] != SYN_NORM) // no naked pre/post, 
            {                                                 // always pick up parent
               par = syn_from->parent[synapse_type];
               added = remapSyn.insert({par,total_syns});
               if (added.second)
               {
                  ++total_syns;
                  ++totalSynTypes;
               }
            }
         }
      }
      else if (cell_item)
      {
         ++totalCells;
         slot = cell_item->getDidx();
         pop = D.inode[slot].node_number;
         added = remapCPop.insert({pop,cpop});
         if (added.second)
            ++cpop;
         cell = &D.inode[slot].unode.cell_node;
         for (tar = 0; tar <= LAST_INODE; tar++)
         {
            target_num = cell->c_target_nums[tar];
            if (target_num == 0 || subTargets.find(target_num) == subTargets.end())
               continue;
            synapse_type = cell->c_synapse_type[tar];
            added = remapSyn.insert({synapse_type,total_syns});
            if (added.second)
            {
               ++total_syns; 
               ++totalSynTypes;
            }
            if (syn_from->syn_type[synapse_type] != SYN_NORM)
            {
               par = syn_from->parent[synapse_type];
               added = remapSyn.insert({par,total_syns});
               if (added.second)
               {
                  ++total_syns;
                  ++totalSynTypes;
               }
            }
         }
      }
   }
}

void SimScene::exportFiber(FiberNode* fiber_item,inode_global& subset)
{
   F_NODE *fiber, *src_fiber;
   AXON *axon;
   int slot, newnum, synapse_type, target_num, old_node;

   slot = fiber_item->getDidx();
   subset.inode[nextInode] = D.inode[slot];  // copy rec
   fiber = &subset.inode[nextInode].unode.fiber_node;
   old_node = subset.inode[nextInode].node_number;
   subset.inode[nextInode].node_number = remapFPop[old_node];
   src_fiber = &D.inode[slot].unode.fiber_node;
   fiber->fiber_x = src_fiber->fiber_x - expDx; // move it
   fiber->fiber_y = src_fiber->fiber_y - expDy;
   // anything synapse-related may be moved, zero current vals
   memset(fiber->fiber_axon,0,sizeof(fiber->fiber_axon));
   memset(fiber->f_target_nums,0,sizeof(fiber->f_target_nums));
   memset(fiber->f_min_conduct_time,0,sizeof(fiber->f_min_conduct_time));
   memset(fiber->f_conduction_time,0,sizeof(fiber->f_conduction_time));
   memset(fiber->f_terminals,0,sizeof(fiber->f_terminals));
   memset(fiber->f_synapse_type,0,sizeof(fiber->f_synapse_type));
   memset(fiber->f_synapse_strength,0,sizeof(fiber->f_synapse_strength));
   memset(fiber->f_target_seed,0,sizeof(fiber->f_target_seed));
   fiber->f_targets = 0;

   for (int tar = 0, next_slot = 0; tar <= LAST_INODE; tar++)
   {
      target_num = src_fiber->f_target_nums[tar];
      if (target_num && subTargets.find(target_num) != subTargets.end())
      {
         ++fiber->f_targets;
         // adjust targ & syn numbers from lists, other params the same
         newnum = remapCPop[target_num];
         fiber->f_target_nums[next_slot] = newnum;
         synapse_type = src_fiber->f_synapse_type[tar];
         fiber->f_synapse_type[next_slot] = remapSyn[synapse_type];
         fiber->f_min_conduct_time[next_slot] = src_fiber->f_min_conduct_time[tar];
         fiber->f_conduction_time[next_slot] = src_fiber->f_conduction_time[tar];
         fiber->f_terminals[next_slot] = src_fiber->f_terminals[tar];
         fiber->f_synapse_strength[next_slot] = src_fiber->f_synapse_strength[tar];
         fiber->f_target_seed[next_slot] = src_fiber->f_target_seed[tar];
         axon = &src_fiber->fiber_axon[tar];
         for (int pts = 0; pts < axon->num_coords; ++pts)
         {
           ++fiber->fiber_axon[next_slot].num_coords;
            fiber->fiber_axon[next_slot].x_coord[pts] = axon->x_coord[pts] - expDx;
            fiber->fiber_axon[next_slot].y_coord[pts] = axon->y_coord[pts] - expDy;
         }
         ++next_slot;
      }
   }
   ++nextInode;
}


void SimScene::exportCell(CellNode* cell_item, inode_global& subset)
{
   C_NODE *cell, *src_cell;
   AXON *axon;
   int slot, newnum, synapse_type, target_num, old_node;

   slot = cell_item->getDidx();
   subset.inode[nextInode] = D.inode[slot];  // copy rec
   old_node = subset.inode[nextInode].node_number;
   subset.inode[nextInode].node_number = remapCPop[old_node];
   cell = &subset.inode[nextInode].unode.cell_node;
   src_cell = &D.inode[slot].unode.cell_node;
   cell->cell_x = src_cell->cell_x - expDx; // move it
   cell->cell_y = src_cell->cell_y - expDy;
    // anything synapse-related may be moved, zero current vals
   memset(cell->cell_axon,0,sizeof(cell->cell_axon));
   memset(cell->c_target_nums,0,sizeof(cell->c_target_nums));
   memset(cell->c_min_conduct_time,0,sizeof(cell->c_min_conduct_time));
   memset(cell->c_conduction_time,0,sizeof(cell->c_conduction_time));
   memset(cell->c_terminals,0,sizeof(cell->c_terminals));
   memset(cell->c_synapse_type,0,sizeof(cell->c_synapse_type));
   memset(cell->c_synapse_strength,0,sizeof(cell->c_synapse_strength));
   memset(cell->c_target_seed,0,sizeof(cell->c_target_seed));
   cell->c_targets = 0;

   for (int tar = 0, next_slot = 0; tar <= LAST_INODE; tar++)
   {
      target_num = src_cell->c_target_nums[tar];
      if (target_num && subTargets.find(target_num) != subTargets.end())
      {
         ++cell->c_targets;
         newnum = remapCPop[target_num];
         cell->c_target_nums[next_slot] = newnum;
         synapse_type = src_cell->c_synapse_type[tar];
         cell->c_synapse_type[next_slot] = remapSyn[synapse_type];
         cell->c_min_conduct_time[next_slot] = src_cell->c_min_conduct_time[tar];
         cell->c_conduction_time[next_slot] = src_cell->c_conduction_time[tar];
         cell->c_terminals[next_slot] = src_cell->c_terminals[tar];
         cell->c_synapse_strength[next_slot] = src_cell->c_synapse_strength[tar];
         cell->c_target_seed[next_slot] = src_cell->c_target_seed[tar];
         axon = &src_cell->cell_axon[tar];
         for (int pts = 0; pts < axon->num_coords; ++pts)
         {
            ++cell->cell_axon[next_slot].num_coords;
            cell->cell_axon[next_slot].x_coord[pts] = axon->x_coord[pts] - expDx;
            cell->cell_axon[next_slot].y_coord[pts] = axon->y_coord[pts] - expDy;
         }
         ++next_slot;
      }
   }
   ++nextInode;
}

void SimScene::exportSynapses(inode_global& subset)
{
   // create synapse types 
   int fidx,tidx;
   S_NODE *syn_to, *syn_from;

   syn_from = &D.inode[SYNAPSE_INODE].unode.synapse_node;
   syn_to = &subset.inode[SYNAPSE_INODE].unode.synapse_node;
   subset.inode[SYNAPSE_INODE].node_type = SYNAPSE;

   for (remapIter iter = remapSyn.begin(); iter != remapSyn.end(); ++iter)
   {
      fidx = (*iter).first;
      tidx = (*iter).second;
      strcpy(syn_to->synapse_name[tidx], syn_from->synapse_name[fidx]); 
      syn_to->s_eq_potential[tidx] = syn_from->s_eq_potential[fidx]; 
      syn_to->s_time_constant[tidx] = syn_from->s_time_constant[fidx]; 
      syn_to->syn_type[tidx] = syn_from->syn_type[fidx]; 
      if (syn_from->parent[fidx]) // re-parent?
      {
         int par = syn_from->parent[fidx];
         syn_to->parent[tidx] = remapSyn[par];
      }
   }
}

// Create the global inode. This is done last once we know max # of fibers, etc.
void SimScene::exportGlobals(inode_global& subset)
{
   subset.inode[GLOBAL_INODE].node_type = SUBSYSTEM;
   subset.presynaptic_flag = D.presynaptic_flag;
   subset.file_subversion = FILEIO_SUBVERSION_CURRENT;
   strcpy(subset.inode[GLOBAL_INODE].comment1,"This is a simulator model library component.");
}


void SimScene::saveSubsystem (inode_global& sub)
{
   QFileInfo info;
   QMessageBox msgBox;
   QString subsys;

   if (par->chkForDrawing())
      return;
#if 0
   if (par->connectorMode != connMode::OFF)
   {
      msgBox.setIcon(QMessageBox::Warning);
      msgBox.setWindowTitle("Finish Drawing The Model");
      msgBox.setStandardButtons(QMessageBox::Ok);
      msgBox.setText("There is an incomplete connector in the model drawing.\nFinish the drawing or cancel the connector with Space or Esc key\nbefore saving the model.");
      msgBox.exec();
      return;
   }
#endif
   if (par->currSndFile.length())
   {
      info.setFile(par->currSndFile);
      subsys = info.path() + "/" + info.completeBaseName() + ".sndlib";
   }
   else
      subsys = QDir::currentPath() + "/" + "sim_subsystem" + ".sndlib";
   QFileDialog openDlg(par);
   openDlg.setWindowTitle(tr("Export Group To File"));
   openDlg.setAcceptMode(QFileDialog::AcceptMode::AcceptSave);
   openDlg.setNameFilter(tr("export files (*.sndlib )"));
   openDlg.selectFile(subsys);
   openDlg.setLabelText(QFileDialog::DialogLabel::FileType,tr("Save file name"));
   openDlg.resize(QSize(900,500));
   if (!openDlg.exec())
      return;
   QStringList list = openDlg.selectedFiles();
   QString fName = list[0];
   if (fName.length())
   {
      Save_subsys(fName.toLatin1().data(),&sub);
      par->printMsg("Saved subsystem " + fName);
   }
}


// Set up for import group from file. Select file,
// prompt user to click on where to drop it.
void SimScene::importGroup()
{
   if (par->chkForDrawing())
      return;
   FILE *fd;
   QFileDialog openDlg(par);
   openDlg.setWindowTitle(tr("Import Group From File"));
   openDlg.setFileMode(QFileDialog::ExistingFile);
   openDlg.setNameFilter(tr(".sndlib (*.sndlib )"));
   openDlg.setDirectory(QDir::current());
   openDlg.setLabelText(QFileDialog::DialogLabel::FileType,tr("Select import file to load"));
   openDlg.resize(QSize(900,500));
   if (!openDlg.exec())
      return;
   QStringList list = openDlg.selectedFiles();
   QString fName = list[0];
   if (!fName.length())
      return;

   ::struct_info_fn = inode_struct_info;
   ::struct_members_fn = inode_struct_members;
   if (importModel)
      delete importModel;
   importModel = new inode_global();

   if ((fd = load_struct_open_simbuild (fName.toLatin1().data())) != 0)
   {
      load_struct (fd, (char*)"inode_global", importModel, 1);
      fclose (fd);
      if (importModel->inode[GLOBAL_INODE].node_type != SUBSYSTEM)
         par->printMsg("Warning: This may not be an import model file");
   }
   par->ui->simView->resetTransform();
   QRectF rect(0,0,1,1);
   par->ui->simView->ensureVisible(rect,0,0);
   par->ui->simView->scale(0.40,0.40);
   waitForImportClick = true;
   par->ui->simView->setCursor(Qt::CrossCursor);
   par->ui->simView->setFocus();
   importLabel->show();
}

// Called on mouse button release. Check to see if we are waiting for a click
// to import a group, and do so if we are.
void SimScene::importGroupChk(QGraphicsSceneMouseEvent *me)
{
   if (!waitForImportClick)
      return;
   waitForImportClick = false;
   par->ui->simView->unsetCursor();
   importLabel->hide();
   moveImport(me);
   drawImport();
   par->ui->simView->scaleToFit();
   delete importModel;
   importModel = nullptr;
}


// adjust all coords to move to new location
void SimScene::moveImport(QGraphicsSceneMouseEvent *me)
{
   int xoff = me->scenePos().rx();
   int yoff = me->scenePos().ry();
   int src, tar, target_num, coord;
   AXON *axon;

   for (src = FIRST_INODE; src <= LAST_INODE; ++src)   // 1 - 98 are possible fibers/cells
   {
      if (importModel->inode[src].node_type == FIBER)
      {
         F_NODE *f = &importModel->inode[src].unode.fiber_node;
         f->fiber_x += xoff;
         f->fiber_y += yoff;
         for (tar = 0; tar <= LAST_INODE; tar++)
         {
            target_num = f->f_target_nums[tar];
            if (target_num == 0)
               continue;
            axon = &f->fiber_axon[tar];
            if (axon->num_coords)
            {
               for (coord = 0 ; coord < axon->num_coords; ++coord)
               {
                  axon->x_coord[coord] += xoff;
                  axon->y_coord[coord] += yoff;
               }
            }
         }
      }
      else if (importModel->inode[src].node_type == CELL)
      {
         C_NODE *c = &importModel->inode[src].unode.cell_node;
         c->cell_x += xoff;
         c->cell_y += yoff;
         for (tar = 0; tar <= LAST_INODE; tar++)
         {
            target_num = c->c_target_nums[tar];
            if (target_num == 0)
               continue;
            axon = &c->cell_axon[tar];
            if (axon->num_coords)
            {
               for (coord = 0 ; coord < axon->num_coords; ++coord)
               {
                  axon->x_coord[coord] += xoff;
                  axon->y_coord[coord] += yoff;
               }
            }
         }
      }
   }
}

/* Add the imported nodes and connectors to the global D array.
*/
//target remap
using tRmap = struct {int node_num; int oldpop; int newpop;};
void SimScene::drawImport()
{
   int src, sib, tar;
   int target_num;
   int node_num, new_node;
   bool added = false;
   synRec rec;
   int coord;
   bool excite;
   int synapse_type;
   AXON *axon;
   QModelIndex idx;
   map <int, int> synMap;
   map <int, int> targMap;
   map <int, tRmap> popMap;

   S_NODE* syn = &importModel->inode[SYNAPSE_INODE].unode.synapse_node;
     // build lookup list import node# -> new node#
   for (src = 1; src < TABLE_LEN; ++src)
   {
      if (syn->syn_type[src] != SYN_NOT_USED)
      {
         node_num = getSynapse();
         if (node_num)
            synMap.insert({src,node_num});
         else
            outOfNodes();
      }
   }
     // cell target list, import pop # to new popnum
   for (src = FIRST_INODE; src <= LAST_INODE; ++src)
   {
      if (importModel->inode[src].node_type == CELL)
      {
         node_num = get_node(CELL);
         tRmap rmap = {node_num, importModel->inode[src].node_number,
                         D.inode[node_num].node_number};
         popMap.insert({src,rmap});
         targMap.insert({importModel->inode[src].node_number,
                        D.inode[node_num].node_number});
      }
   }

   // Create the new synapse types in the type table.
   // If there are pre/post children, create the parent, then the 
   // pre (if there is one) then the post
   for (auto iter = synMap.begin(); iter != synMap.end(); ++iter)
   {
      src = (*iter).first;
      if (syn->syn_type[src] != SYN_NORM) // skip unused, pre, post
         continue;
      src = (*iter).first;
      new_node = (*iter).second;
      QString sName(syn->synapse_name[src]);
      sName += "_imp" + QString::number(new_node);
      rec.rec[SYN_NUM]   = new_node;
      rec.rec[SYN_NAME]  = sName;
      rec.rec[SYN_TYPE]  = syn->syn_type[src];
      rec.rec[SYN_EQPOT] = syn->s_eq_potential[src];
      rec.rec[SYN_TC]    = syn->s_time_constant[src];
      rec.rec[SYN_PARENT] = 0;
      synBuildModel->addRow(rec,idx);
      added = true;
      for (sib = 1; sib < TABLE_LEN; ++sib)  // is this parent of pre synapse?
      {
         if (syn->parent[sib] == src && syn->syn_type[sib] == SYN_PRE)
         {
            auto child = synMap.find(sib);
            QString cName(syn->synapse_name[sib]);
            cName += "_imp" + QString::number(new_node);
            rec.rec[SYN_NUM]   = (*child).second;
            rec.rec[SYN_NAME]  = cName;
            rec.rec[SYN_TYPE]  = syn->syn_type[sib];
            rec.rec[SYN_EQPOT] = syn->s_eq_potential[sib];
            rec.rec[SYN_TC]    = syn->s_time_constant[sib];
            rec.rec[SYN_PARENT] = new_node;
            synBuildModel->addRow(rec,idx);
         }
      }
      for (sib = 1; sib < TABLE_LEN; ++sib)  // is this parent of post synapse?
      {
         if (syn->parent[sib] == src && syn->syn_type[sib] == SYN_POST)
         {
            auto child = synMap.find(sib);
            QString cName(syn->synapse_name[sib]);
            cName += "_imp" + QString::number(new_node);
            rec.rec[SYN_NUM]   = (*child).second;
            rec.rec[SYN_NAME]  = cName;
            rec.rec[SYN_TYPE]  = syn->syn_type[sib];
            rec.rec[SYN_EQPOT] = syn->s_eq_potential[sib];
            rec.rec[SYN_TC]    = syn->s_time_constant[sib];
            rec.rec[SYN_PARENT] = new_node;
            synBuildModel->addRow(rec,idx);
         }
      }
   }
   if (added)
   {
      buildApply();
      par->ui->buildSynapsesView->expandAll();
      updateInfoText();
   }

    // Draw the populations
   for (src = FIRST_INODE; src <= LAST_INODE; ++src)   // 1 - 98 are possible fibers/cells
   {
      if (importModel->inode[src].node_type == FIBER)
      {
         node_num = get_node(FIBER);
         importModel->inode[src].node_number = D.inode[node_num].node_number;
         D.inode[node_num] = importModel->inode[src];
         //F_NODE *f = &D.inode[node_num].unode.fiber_node;
         F_NODE *f = &importModel->inode[src].unode.fiber_node;
         for (tar = 0; tar <= LAST_INODE; tar++)
         {
            target_num = f->f_target_nums[tar];
            if (target_num == 0)
               continue;
            f->f_target_nums[tar] = targMap[target_num];
            f->f_synapse_type[tar] = synMap[f->f_synapse_type[tar]];
         }
         D.inode[node_num] = importModel->inode[src];
         addFiber(QPoint(f->fiber_x,f->fiber_y), node_num, true);
      }
      else if (importModel->inode[src].node_type == CELL)
      {
         auto iter = popMap.find(src);
         node_num = (*iter).second.node_num;
         //node_num = (*targMap.find(importModel->inode[src].node_number)).second.node_num;
         importModel->inode[src].node_number = (*iter).second.newpop;
         C_NODE *c = &importModel->inode[src].unode.cell_node;
         for (tar = 0; tar <= LAST_INODE; tar++)
         {
            target_num = c->c_target_nums[tar];
            if (target_num == 0)
               continue;
            c->c_target_nums[tar] = targMap[target_num];
            c->c_synapse_type[tar] = synMap[c->c_synapse_type[tar]];
        }
        D.inode[node_num] = importModel->inode[src];
        addCell(QPoint(c->cell_x,c->cell_y), node_num, true);
      }
   }

   // fake line clicks to add connectors
   for (src = FIRST_INODE; src <=LAST_INODE; ++src) // for each possible node
   {
      if (importModel->inode[src].node_type == FIBER)
      {
         F_NODE *f = &importModel->inode[src].unode.fiber_node;
         for (tar = 0; tar <= LAST_INODE; tar++)
         {
            target_num = f->f_target_nums[tar];
            if (target_num == 0)
               continue;
            F_NODE *f = &importModel->inode[src].unode.fiber_node;
            synapse_type = f->f_synapse_type[tar];
            excite = syn->s_eq_potential[synapse_type] > 0;
            axon = &f->fiber_axon[tar];
            if (axon->num_coords)
            {
               par->setConnMode(connMode::START);
               for (coord = 0 ; coord < axon->num_coords; ++coord)
               {
                  QPointF pt(axon->x_coord[coord],axon->y_coord[coord]);
                  addConnector(pt,synapse_type,excite,tar,true);
               }
               if (coord && par->getConnMode() == connMode::DRAWING)
               {
                   // This can happen in some hard-to-duplicate instances,
                  // usually a result of moving an endpoint jussssst outside
                  // of a nodes' shape. Can we figure out what the start/end
                  // node is and correct this? Also, what if it is the start point?
                  // 
                  cout << "Oops, did not find end point of connnector." << endl;
                  cout << "We should be connected to inode " << target_num 
                       << " " << importModel->inode[target_num].comment1 << endl;
                  par->setConnMode(connMode::OFF);
               }
            }
         }
      }
      else if (importModel->inode[src].node_type == CELL)
      {
         C_NODE *c = &importModel->inode[src].unode.cell_node;
         for (tar = 0; tar <= LAST_INODE; tar++)
         {
            target_num = c->c_target_nums[tar];
            if (target_num == 0)
               continue;
            synapse_type = c->c_synapse_type[tar];
            excite = syn->s_eq_potential[synapse_type] > 0;
            axon = &c->cell_axon[tar];
            if (axon->num_coords)
            {
               par->setConnMode(connMode::START);
               for (coord = 0 ; coord < axon->num_coords; ++coord)
               {
                  QPointF pt(axon->x_coord[coord],axon->y_coord[coord]);
                  addConnector(pt,synapse_type,excite,tar,true);
               }
               if (coord && par->getConnMode() == connMode::DRAWING)
               {
                  cout << "Oops, did not find end point of connnector." << endl;
                  cout << "We should be connected to inode " << target_num 
                       << " " << importModel->inode[target_num].comment1 << endl;
                  par->setConnMode(connMode::OFF);
               }
            }
         }
      }
      else
         continue;
   }
   par->buildFindList(); // new things to find
}
