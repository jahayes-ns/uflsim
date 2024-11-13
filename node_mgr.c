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

#include "inode.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#ifdef BSD
#include <sys/dir.h>
#else
#include <dirent.h>
#endif
#include "c_globals.h"

// Initial state of the global INODE array
void init_nodes()
{
//   printf("d: is %d  inode is: %d  %d  %d  %d  %d %d\n", sizeof(D), sizeof(D.inode[0]),
//      sizeof(GLOBAL_NETWORK), sizeof(C_NODE), sizeof(F_NODE), sizeof(S_NODE), sizeof(U_NODE));
   int xloop;
     // there may be strdup pointers, don't leak
   for(int node = FIRST_INODE; node <= LAST_INODE; ++node)
      if (D.inode[node].node_type == CELL && D.inode[node].unode.cell_node.c_injected_expression)
         free(D.inode[node].unode.cell_node.c_injected_expression);

   memset(&D,0,sizeof(D));
   memset(cellsInUse,false,sizeof(cellsInUse));
   memset(fibersInUse,false,sizeof(fibersInUse));
   D.num_free_nodes=MAX_INODES;
   maxCellPop = 0;
   maxFiberPop = 0;
   for (xloop=0; xloop<MAX_INODES; xloop++)
   {
      D.inode[xloop].node_type=UNUSED;
      strcpy(D.inode[xloop].comment1,"This node is Unused");
   }
}


// The max for both fibers and cell is a total of 98.
bool reachedMax()
{
   int pop;
   int fibers = 0;
   int cells = 0;

   for(pop = FIRST_INODE; pop <= LAST_INODE; pop++)
      if (fibersInUse[pop])
         ++fibers;
   for(pop = FIRST_INODE; pop <= LAST_INODE; pop++)
      if (cellsInUse[pop])
         ++cells;
   if (fibers+cells == LAST_INODE)
      return true;
   else
      return false;
}

// Find next unused synapse number in the D structure.
// newsned never deleted synaposes, simbuild does.
int getSynapse()
{
   S_NODE *s = &D.inode[SYNAPSE_INODE].unode.synapse_node;
   int node;
   for (node = 1; node < TABLE_LEN; ++node) // index 0 unused for synapse node
   {
      if (s->syn_type[node] == SYN_NOT_USED)
      {
         s->syn_type[node] = SYN_NORM;
         break;
      }
   }
   if (node == TABLE_LEN) // out of nodes
      node = -1;
   if (node > D.inode[GLOBAL_INODE].unode.global_node.num_synapse_types)
      D.inode[GLOBAL_INODE].unode.global_node.num_synapse_types = node;
   return node; 
}

void freeSynapse(int node)
{
   S_NODE *s = &D.inode[SYNAPSE_INODE].unode.synapse_node;

   memset(s->synapse_name[node],0,SNMLEN); 
   s->syn_type[node] = SYN_NOT_USED;
   s->s_eq_potential[node] = 0.0;
   s->s_time_constant[node] = 0.0;
   s->parent[node] = 0;
   D.inode[GLOBAL_INODE].unode.global_node.num_synapse_types = lastSynType(&D);
}

int lastSynType(inode_global *model)
{
     // scan entire table to find last valid synaptic entry/index
   int last = 0;
   for (int syn=1;syn<TABLE_LEN-1;syn++)  // 0 entry not used
   {
      if (model->inode[SYNAPSE_INODE].unode.synapse_node.syn_type[syn] != SYN_NOT_USED)
         last = syn;
   }
   return last;
}

int synTypesInUse()
{
   int used = 0;
   for (int syn=1;syn<TABLE_LEN-1;syn++)  // 0 entry not used
   {
      if (D.inode[SYNAPSE_INODE].unode.synapse_node.syn_type[syn] != SYN_NOT_USED)
         ++used;
   }
   return used;
}



// Find next unused cell population number from in-use list or 
// indicate there are no more free nodes.
int nextCellPop()
{
   int pop;
   if (reachedMax())
      return -1;
   for (pop = FIRST_INODE; pop <= LAST_INODE; pop++)
   {
      if (cellsInUse[pop] == false)
      {
         cellsInUse[pop] = true;
         return pop;
      }
   }
   return -1; // should never get here, but. . .
}


// Find next unused fiber population number from in-use list or 
// indicate there are no more free nodes.
int nextFiberPop()
{
   int pop;
   if (reachedMax())
      return -1;
   for(pop = FIRST_INODE; pop <= LAST_INODE; pop++)
   {
      if (fibersInUse[pop] == false)
      {
         fibersInUse[pop] = true;
         return pop;
      }
   }
   return -1; 
}

// we sometimes want the max pop #, not how many
int usedMaxFiberPop()
{
   int pop;
   int max = 0;
   for (pop = FIRST_INODE; pop <= LAST_INODE; pop++)
      if ( (fibersInUse[pop] == true) && (pop > max))
         max = pop;
   return max;
}

int usedMaxCellPop()
{
   int pop;
   int max = 0;
   for (pop = FIRST_INODE; pop <= LAST_INODE; pop++)
      if ( (cellsInUse[pop] == true) && (pop > max))
         max = pop;
   return max;
}

/* This routine gets a free node and inits 
   it for the type requested->returns index into the D array 
   First find a free node  
   reference is by base 1 , not base 0 arrays
*/
int get_node(unsigned char local_type)
{
   int   xloop;
   int   local_free;
   int   new_map;
   int   idx = 0;

   new_map=false;

   for(xloop=FIRST_INODE; xloop <= LAST_INODE; xloop++)
   {
      if (D.inode[xloop].node_type==UNUSED)
      {
         if (local_type==CELL)
            local_free = nextCellPop();
         else if (local_type==FIBER)
            local_free = nextFiberPop();
         else
         {
            local_free = -1;
            printf("Unknown new node type\n");
         }

         if (local_free == -1)
            return local_free;   // up to caller to handle this

         new_map=true;
         D.inode[xloop].node_type=local_type;
         strcpy(D.inode[xloop].comment1,"This node is in use");
         --D.num_free_nodes;
         idx = xloop;

         D.num_used_nodes[local_type]++; 
         D.inode[xloop].node_number=local_free; /* population # */
         break;
      }
   }

   if (new_map==true)
   {
      D.inode[GLOBAL_INODE].unode.global_node.total_fibers=usedMaxFiberPop();
      D.inode[GLOBAL_INODE].unode.global_node.total_cells=usedMaxCellPop();
      D.inode[GLOBAL_INODE].unode.global_node.total_populations = 
        D.inode[GLOBAL_INODE].unode.global_node.total_fibers +
        D.inode[GLOBAL_INODE].unode.global_node.total_cells;
   }
   return idx;
}


// If a node has been freed (i.e., deleted), search the other fibers and
// cells to see if it was a target, and remove the target info and synapse
// coordinates for that object.
void adjustTargets(int node_id)
{
   int item, targ, num_targs;
   C_NODE *cell;
   F_NODE *fiber;

   for (item=FIRST_INODE; item <= LAST_INODE; item++)
   {
      if (item == node_id)  // self already dealt with
         continue;
      switch (D.inode[item].node_type)
      {
         case CELL:
            cell = &D.inode[item].unode.cell_node;
            num_targs = cell->c_targets;
            for (targ = 0; targ < num_targs; ++targ)
            {
               if (cell->c_target_nums[targ] == node_id)
               {
                  cell->c_target_nums[targ] = 0;
                  memset(&cell->cell_axon[targ],0,sizeof(AXON));
                  --cell->c_targets;
               }
            }
            break;

         case FIBER:
            fiber = &D.inode[item].unode.fiber_node;
            num_targs = fiber->f_targets;
            for (targ = 0; targ < num_targs; ++targ)
            {
               if (fiber->f_target_nums[targ] == node_id)
               {
                  fiber->f_target_nums[targ] = 0;
                  memset(&fiber->fiber_axon[targ],0,sizeof(AXON));
                  --fiber->f_targets;
               }
            }
            break;

         default:
            continue;
            break;
      }
   }
}


/* Free/delete an existing node */
void  free_node(int node_id)
{
   if (D.inode[node_id].node_type == CELL)
   {
      --D.inode[GLOBAL_INODE].unode.global_node.total_cells;
        // this really is not total cells, is is the max pop #
      --D.num_used_nodes[CELL];
      cellsInUse[D.inode[node_id].node_number] = false;
   }
   else
   {
      --D.inode[GLOBAL_INODE].unode.global_node.total_fibers;
      --D.num_used_nodes[FIBER];
      fibersInUse[D.inode[node_id].node_number] = false;
   }

   memset(&D.inode[node_id].unode,0,sizeof(D.inode[0].unode));
   D.inode[node_id].node_type = UNUSED;
   D.inode[node_id].node_number = UNUSED;
   strcpy(D.inode[node_id].comment1,"This node is unused");
   ++D.num_free_nodes;

   adjustTargets(node_id);

   /* set the globals to reflect the new pop quantities                       */
   get_maxes(&D);
}


/* This routine figures out all maximum globals.  The code tries to keep
   these values up to date, but often fails to do so.  I notice that most of
   this does not look at the last item in the tables, which are all defined to
   be [100]
   Passed: pointer to model struct. Usually the global D array, but could
           be a group being exported.
*/
void get_maxes(inode_global *model)
{
   int xloop,yloop;
   int total_cells = 0;
   int total_fibers = 0;

   model->inode[GLOBAL_INODE].unode.global_node.max_conduction_time=0;
   model->inode[GLOBAL_INODE].unode.global_node.maximum_cells_per_pop=0;
   model->inode[GLOBAL_INODE].unode.global_node.num_synapse_types=0;
   model->inode[GLOBAL_INODE].unode.global_node.max_targets=0;

         /*  Find maximum conduction time   */
   for (xloop=FIRST_INODE; xloop<SYNAPSE_INODE; xloop++)
   {
      if (model->inode[xloop].node_type==CELL)
      {
         ++total_cells;
         for (yloop=0;yloop<SYNAPSE_INODE;yloop++)
         {
            if (model->inode[xloop].unode.cell_node.c_min_conduct_time[yloop] >
               model->inode[GLOBAL_INODE].unode.global_node.max_conduction_time)
               model->inode[GLOBAL_INODE].unode.global_node.max_conduction_time=
                  model->inode[xloop].unode.cell_node.c_min_conduct_time[yloop];
            if (model->inode[xloop].unode.cell_node.c_conduction_time[yloop] >
               model->inode[GLOBAL_INODE].unode.global_node.max_conduction_time)
               model->inode[GLOBAL_INODE].unode.global_node.max_conduction_time=
                  model->inode[xloop].unode.cell_node.c_conduction_time[yloop];
         }
      }
      if (model->inode[xloop].node_type==FIBER)
      {
         ++total_fibers;
         for (yloop=0;yloop<SYNAPSE_INODE;yloop++)
         {
            if (model->inode[xloop].unode.fiber_node.f_min_conduct_time[yloop] >
               model->inode[GLOBAL_INODE].unode.global_node.max_conduction_time)
               model->inode[GLOBAL_INODE].unode.global_node.max_conduction_time=
                  model->inode[xloop].unode.fiber_node.f_min_conduct_time[yloop];
            if (model->inode[xloop].unode.fiber_node.f_conduction_time[yloop] >
               model->inode[GLOBAL_INODE].unode.global_node.max_conduction_time)
               model->inode[GLOBAL_INODE].unode.global_node.max_conduction_time=
                  model->inode[xloop].unode.fiber_node.f_conduction_time[yloop];
         }
      }
   }
           /* now add the plus one value  */
   model->inode[GLOBAL_INODE].unode.global_node.max_conduction_time++;

   model->num_used_nodes[CELL] = total_cells;
   model->num_used_nodes[FIBER] = total_fibers;
   model->num_free_nodes = MAX_INODES - (total_cells+total_fibers);

           /*  Find maximum cell/fiber population size   */
   for (xloop=FIRST_INODE; xloop<SYNAPSE_INODE; xloop++)
   {
      if (model->inode[xloop].node_type==CELL)
      {
         if (model->inode[xloop].unode.cell_node.c_pop >
            model->inode[GLOBAL_INODE].unode.global_node.maximum_cells_per_pop)
            model->inode[GLOBAL_INODE].unode.global_node.maximum_cells_per_pop=
               model->inode[xloop].unode.cell_node.c_pop;
      }
      if (model->inode[xloop].node_type==FIBER)
      {
         if (model->inode[xloop].unode.fiber_node.f_pop >
            model->inode[GLOBAL_INODE].unode.global_node.maximum_cells_per_pop)
            model->inode[GLOBAL_INODE].unode.global_node.maximum_cells_per_pop=
               model->inode[xloop].unode.fiber_node.f_pop;
      }
   }
   model->inode[GLOBAL_INODE].unode.global_node.num_synapse_types = lastSynType(model);

         /* find maximum number of Targets  */
   for (xloop=FIRST_INODE; xloop<SYNAPSE_INODE; xloop++)
   {
      if (model->inode[xloop].node_type==CELL)
      {
         if (model->inode[xloop].unode.cell_node.c_targets >
            model->inode[GLOBAL_INODE].unode.global_node.max_targets)
            model->inode[GLOBAL_INODE].unode.global_node.max_targets=
               model->inode[xloop].unode.cell_node.c_targets;
      }
      if (model->inode[xloop].node_type==FIBER)
      {
         if (model->inode[xloop].unode.fiber_node.f_targets >
            model->inode[GLOBAL_INODE].unode.global_node.max_targets)
            model->inode[GLOBAL_INODE].unode.global_node.max_targets=
               model->inode[xloop].unode.fiber_node.f_targets;
      }
   }
        // These are not really # of nodes, they are max pop num for each type
        // and they can be different if read from old .snd files versus
        // creating a new model from scratch. What a pain.
        // TODO: this also fails to work if you read in an old snd file,
        // save out a new one, and then read in the new one and generate a
        // new .sim file. It will not be the same. The correct thing to do
        // is to abandon this hack of a hack and make the mods so the pop num
        // is not inferred by its location in an array by simrun.
   int currMaxCell = usedMaxCellPop();
   int currMaxFiber = usedMaxFiberPop();
   int maxCell;
   int maxFiber;
   maxCell = currMaxCell;
   maxFiber = currMaxFiber;

   model->inode[GLOBAL_INODE].unode.global_node.total_fibers=maxFiber;
   model->inode[GLOBAL_INODE].unode.global_node.total_cells=maxCell;
   model->inode[GLOBAL_INODE].unode.global_node.total_populations = 
     model->inode[GLOBAL_INODE].unode.global_node.total_fibers +
     model->inode[GLOBAL_INODE].unode.global_node.total_cells;
}
