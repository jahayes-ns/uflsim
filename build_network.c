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


#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "simulator.h"
#include "inode.h"
#include "simrun_wrap.h"
#include <string.h>
          
#define BOUNDS(idx, count) ((idx) >= 0 && (idx) < (count)) || DIE
#if 0
// hard to breakpoing macros, use function for debugging
void BOUNDS(int idx, int count)
{
   if (idx >= 0 && idx < count)
   {
      fprintf(stdout,"Bounds check fail idx: %d count: %d\n",idx,count);
      DIE;
   }
}
#endif

extern int Debug;
extern int haveLearn;
extern int learnCPop[MAX_INODES];
extern int numCPop;
extern int learnFPop[MAX_INODES];
extern int numFPop;

typedef struct
{
  Syn *ptr;
  int maxqidx;
} SynInfo;

static SynInfo ***synapse; // see lengthy comment at end of file with some
                           // clues about how this is used.

double
ran (int *i)
{
  double x;
  *i = 69069 * *i + 1;
  x = *i;
  if (x < 0) 
     x = x + 4294967296.0;
  return x / 4294967296.0; // 0.0 - 1.0
}

/* func is function to call. Choices are:
   count_to_synapses(...)
   attach_to_synapses(...) 
*/
static inline void
for_terminals (int iseed, Target *target, int target_count, TargetPop *tp, int tidx0,
          void (*func)(TargetPop*, CellPop*, Target*, int, int, int))
{
  int tidx;
  for (tidx = 0; tidx < tp->NT; tidx++) {
     if(Debug) printf ("for_terminals:  tidx0 %d, tp->NT %d, tidx %d, target_count %d cell pop count %d\n", tidx0, tp->NT, tidx, target_count,S.net.cellpop_count);

     if ((tp->IRCP) > 0) {
        int tcpidx = tp->IRCP - 1;
        CellPop *tcp = S.net.cellpop + tcpidx;
        // tcidx is rand #0-1 * number of cells
        int tcidx = (int)(ran(&iseed) * tcp->cell_count);
   
        // quidx is min cond time + random #0-1 * (cond time - min cond time)
        //int qidx = tp->MCT + (int)(ran(&iseed) * (tp->NCT - tp->MCT));
        double r0 = ran(&iseed);
        // this is how to original code did this. Why calculate qidx
        // then do it again with a different random number?
        int qidx = tp->MCT + (int)(r0 * (tp->NCT - tp->MCT));
        // JAH:   tp->MCT - target population minimum condition time
        //        tp->NCT - target population condition time
        //        tp->NT  - target population number of terminals
        if(Debug) {
          printf("seed:%d rand: %lf  NCT: %d MCT: %d\n",iseed,r0,tp->NCT, tp->MCT);
          printf("qidx#1: %d\n",qidx);
        }
        // if number of terminals == max cond time - min cond time
        if (tp->NT == tp->NCT - tp->MCT)
           qidx = tp->MCT + tidx;     // qidx is min time plus current terminal index
        else
           // else repeat the calculation above (huh?)
           qidx = tp->MCT + (int)(ran(&iseed) * (tp->NCT - tp->MCT));
   
        // qidx seems to be determined by how long the max-min conductance time is
        // larger diffs means more elements in the q array
        if(Debug) printf("qidx#2: %d\n",qidx);
        if(Debug) fflush(stdout);
   
        // check for errors and exit program if there are any
        if ( tcpidx < 0 || tcpidx >= S.net.cellpop_count) {   
           fprintf (stdout, "bounds1: tcpidx %d, tidx0 %d, tp->NT %d, tidx %d, target_count %d cellpopcount %d \n", tcpidx, tidx0, tp->NT, tidx, target_count,S.net.cellpop_count); fflush(stdout);
        }
        BOUNDS (tcpidx, S.net.cellpop_count);
        if ( (tidx0+tidx) < 0 || tidx0+tidx >= target_count ) {   
           fprintf (stdout, "bounds2: tidx0 %d, tp->NT %d, tidx %d, target_count %d cellpopcount %d \n", tidx0, tp->NT, tidx, target_count,S.net.cellpop_count); fflush(stdout);
        }
        BOUNDS (tidx0 + tidx, target_count);
        func (tp, tcp, target + tidx0 + tidx, tcpidx, tcidx, qidx);
     }
  }
}

// This sets up the glue between sender and receiver terminals.
// This shares a pointer and other values between each sender and receiver.
// The sender writes potentials to the q array with an optional delay factor
// The receiver reads behind the sender and uses the value to increase or
// decrease potentials (excit,inhib), perhaps to cause a firing event. 

//** This is where the shared learning info should be kept. This is the 
//** common area where senders store firing history and where receivers
//** decide if the sender strength should be modified.
static inline void
attach_to_synapses (TargetPop *tp, CellPop *tcp, Target* t, int tcpidx, int tcidx, int qidx)
{
  // JAH:
  // tp - target population
  // tcp - target cell population
  // t - target
  // tcpidx - target cell population index
  // tcidx - target cell index
  // qidx - queue (slot) index (aka delay)
  BOUNDS (tcpidx, S.net.cellpop_count);
  BOUNDS (tcidx, S.net.cellpop[tcpidx].cell_count);
  BOUNDS (tp->TYPE - 1, S.net.syntype_count);
  t->syn = synapse[tcpidx][tcidx][tp->TYPE-1].ptr;
  t->delay = qidx;
  t->strength = tp->STR;
  t->disabled = 0;
  t->syn->initial_strength = tp->STR;
  t->syn->lrn_strength = tp->STR;

//  if(Debug) printf("attach syn ptr %p to synapse[%d][%d][%d]  vals: delay (qidx):%d str:%f disabled:%d\n", t->syn, tcpidx,tcidx,tp->TYPE-1, t->delay, t->strength, t->disabled);
  if(Debug) printf("attach synapse[%d][%d][%d]  vals: delay (qidx):%d str:%f disabled:%d\n", tcpidx,tcidx,tp->TYPE-1, t->delay, t->strength, t->disabled);
}

// Update the synapse[pop][cell][synapse slot] 
// If we are using this type of synapse, flag the slot for later updates.
// Also update the maxqidx value.
// Note that this STRONGLY assumes that the cell/fiber does not 
// have more than one of the same type of synapse. If so, it will not be counted.
// Of course, why would you do this?
static inline void
count_to_synapses (TargetPop *tp, CellPop *tcp, Target *target, int tcpidx, int tcidx, int qidx)
{
  Cell *tc = tcp->cell + tcidx;

  if (Debug) printf ("tgt cellpop %d tgt cell %d syntype offset %d cell count: %d\n", tcpidx, tcidx, tp->TYPE, tcp->cell_count);
  fflush(stdout);

  BOUNDS (tcpidx, S.net.cellpop_count);
  BOUNDS (tcidx, tcp->cell_count);
  BOUNDS (tcidx, S.net.cellpop[tcpidx].cell_count);
  BOUNDS (tp->TYPE - 1, S.net.syntype_count);

  if (synapse[tcpidx][tcidx][tp->TYPE-1].ptr == 0) {
     synapse[tcpidx][tcidx][tp->TYPE-1].ptr++; // used as NZ flag, later replaced by real ptr
     tc->syn_count++;
  }
  if (qidx > synapse[tcpidx][tcidx][tp->TYPE-1].maxqidx)
     synapse[tcpidx][tcidx][tp->TYPE-1].maxqidx = qidx;
  if (Debug) printf("count_to_synapses: ptr, maxqidx for synapse[%d][%d][%d] is %p %d\n",tcpidx,tcidx,tp->TYPE-1, synapse[tcpidx][tcidx][tp->TYPE-1].ptr, synapse[tcpidx][tcidx][tp->TYPE-1].maxqidx);
}


/* func choices are:
   count_to_synapses(...)
   attach_to_synapses(...) 
*/
static inline void
for_cell_targets (void (*func)(TargetPop*, CellPop*, Target*, int, int, int))
{
  {if(Debug) printf(" for_cell_targets\n");}
  int cpidx;
  for (cpidx = 0; cpidx < S.net.cellpop_count; cpidx++)  // for each pop
  {
    CellPop *cp = S.net.cellpop + cpidx;
    int cidx;
    for (cidx = 0; cidx < cp->cell_count; cidx++)    // for each cell
    {
      Cell *c = cp->cell + cidx;
      int tpidx, tidx0 = 0;
      for (tpidx = 0; tpidx < cp->targetpop_count; tpidx++)  // for each target population
      {
        TargetPop *tp = cp->targetpop + tpidx;
        int iseed = (cidx + 1) * tp->INSED;
        if (c->target == 0) 
        {
          if(Debug) printf ("S.net.cellpop[1].cell[4].target: %lx\n", (long)S.net.cellpop[1].cell[4].target);
          if(Debug) printf ("cell %d of pop %d target is not set.  targetpop_count = %d\n",
             cidx, cpidx, cp->targetpop_count);
          DIE;
        }
        tp->NT >= 0 || DIE;
        if (tp->NT > 0) 
        {
          BOUNDS (tidx0 + tp->NT - 1, c->target_count);
          if(Debug) printf("for cell terminals\n");
          for_terminals (iseed, c->target, c->target_count, tp, tidx0, func);
          tidx0 += tp->NT;
        }
      }
    }
  }
}

/* func choices are:
   count_to_synapses(...)
   attach_to_synapses(...) 
*/
static inline void
for_fiber_targets (void (*func)(TargetPop*, CellPop*, Target*, int, int, int))
{
if(Debug) printf("   for_fiber_targets\n");
  int fpidx;
  for (fpidx = 0; fpidx < S.net.fiberpop_count; fpidx++)  // for each pop
  {
    FiberPop *fp = S.net.fiberpop + fpidx;
    int fidx;
    for (fidx = 0; fidx < fp->fiber_count; fidx++)   // for each fiber
    {
      Fiber *f = fp->fiber + fidx;
      int tpidx, tidx0 = 0;
      for (tpidx = 0; tpidx < fp->targetpop_count; tpidx++)  // for each terminal
      {
        TargetPop *tp = fp->targetpop + tpidx;
        int iseed = (fidx + 1) * tp->INSED;
        tp->NT >= 0 || DIE;
        if (tp->NT > 0) 
        {
          BOUNDS (tidx0 + tp->NT - 1, f->target_count);
          if(Debug) printf("for fiber terminals\n");
          for_terminals (iseed, f->target, f->target_count, tp, tidx0, func);
          tidx0 += tp->NT;
        }
      }
    }
  }
}

int row_order_offset(int i, int j, int numberOfColumns) {
   int offset = i * numberOfColumns + j;
   return offset;
}

int st (float eq, float dcs)
{
  if (eq == 115 && fabs (dcs - 0.71653131057378925043) < .000001) return 1;
  if (eq == -25 && fabs (dcs - 0.71653131057378925043) < .000001) return 2;
  if (eq == 115 && fabs (dcs - 0.77880078307140486825) < .000001) return 3;
  if (eq == -25 && fabs (dcs - 0.77880078307140486825) < .000001) return 4;
  return 99999999;
}

// JAH: Writes out the adjacency matrices for the instantiated network in 
//      separate files based on excitatory vs. inhibitory connections
void writeAdjacencyMatrices (void)
{
  //fprintf(ofile, "cell pop count1: %d\n", sizeof(S.net.cellpop));
  //fprintf(ofile, "cell pop count1: %d\n", sizeof(S.net.cellpop[0]));
  /*fprintf(ofile, "cell pop count2: %d\n", S.net.cellpop_count);
  fprintf(ofile, "cell syn count2: %d\n", S.net.syntype_count);
  fprintf(ofile, "cell pop #1, target pop count: %d\n", S.net.cellpop[0].targetpop_count);
  fprintf(ofile, "cell pop #1, target pop: %d\n", S.net.cellpop[0].targetpop[0]);
  fprintf(ofile, "cell pop #1, cell #1, syn count: %d\n", S.net.cellpop[0].cell[0].syn_count);
  */
  
  // iterate over cell populations to get total number of cells in network
  fprintf(stderr, "start writeAdjacencyMatrices\n");
  
  int totalCellsInNetwork = 0;
  int cellPopulationSizes[S.net.cellpop_count];
  
  for (int cpidx = 0; cpidx < S.net.cellpop_count; cpidx++) {
     CellPop *cp = S.net.cellpop + cpidx;
     cellPopulationSizes[cpidx] = cp->cell_count;
     totalCellsInNetwork += cp->cell_count;
  }
  fprintf(stderr, "totalCellsInNetwork: %d\n", totalCellsInNetwork);
  
  // initialize adjacency matrices full of zeros the size of the network
  fprintf(stderr, "sizeof(int): %zu\n", sizeof(short));
//  short excitatoryAdjacencyMatrix[totalCellsInNetwork][totalCellsInNetwork];
  int *excitatoryAdjacencyMatrix = (int *)malloc(totalCellsInNetwork * totalCellsInNetwork * sizeof(int));
  
  fprintf(stderr, "middle writeAdjacencyMatrices\n");
  memset(excitatoryAdjacencyMatrix, 0, totalCellsInNetwork*totalCellsInNetwork*sizeof(int));

  fprintf(stderr, "middle2 writeAdjacencyMatrices\n");
  //short inhibitoryAdjacencyMatrix[totalCellsInNetwork][totalCellsInNetwork];
  int *inhibitoryAdjacencyMatrix = (int *)malloc(totalCellsInNetwork * totalCellsInNetwork * sizeof(int));
  memset(inhibitoryAdjacencyMatrix, 0, totalCellsInNetwork*totalCellsInNetwork*sizeof(int));

  int previousCellPopCounts = 0;
  for (int cpidx = 0; cpidx < S.net.cellpop_count; cpidx++) {
     CellPop *cp = S.net.cellpop + cpidx;
     //fprintf(ofile, "cell pop #1, cell count: %d\n", cp->cell_count);
     //fprintf(ofile, "cell pop #1, cell #1, target count: %d\n", cp->cell[0].target_count);
     Syn *syn = NULL;
     
     // int i = 0, source cells, also rows
     for (int i = 0; i < cp->cell_count; i++) {
     	Cell *c = cp->cell + i;
      
        //syn->cidx // target cells, also columns
        for (int j = 0; j < c->target_count; j++) {
           syn = c->target[j].syn;
           
           // This block to calculate targetPopulationOffset is to correct for 
           //    the preceding columns in the adjacency matrix from other cell populations
           int targetPopulationOffset = 0;
           for (int k = 0; k < syn->cpidx; k++) {
              targetPopulationOffset += cellPopulationSizes[k];
           }
           // excitatory connections
           if (syn->stidx == 0) {
           	  excitatoryAdjacencyMatrix[row_order_offset(previousCellPopCounts + i, targetPopulationOffset + syn->cidx, totalCellsInNetwork)] += 1;
//              excitatoryAdjacencyMatrix[previousCellPopCounts + i][targetPopulationOffset + syn->cidx] += 1;
           }
           // inhibitory connections
           else if (syn->stidx == 1) {
           	  inhibitoryAdjacencyMatrix[row_order_offset(previousCellPopCounts + i, targetPopulationOffset + syn->cidx, totalCellsInNetwork)] += 1;
//              inhibitoryAdjacencyMatrix[previousCellPopCounts + i][targetPopulationOffset + syn->cidx] += 1;
           }
           //fprintf(ofile, "cell pop #1, cell #%d, target #%d, syn#1, cidx: %d\n", i, j, syn->cidx);
           //fprintf(ofile, "cell pop #1, cell #%d, target #%d, syn#1, stidx: %d\n", i, j, syn->stidx);
         //  fprintf(ofile, "cell pop #1, cell #1, target #1, syn#%d, cpidx: %d\n", i, syn->cpidx);
           //fprintf(ofile, "cell pop #1, cell #1, target #1, syn#%d, syntype: %d\n", i, syn->syntype);
        }
     }
     previousCellPopCounts += cp->cell_count;
  }
  
  // Get filename basename
//  char *tmp = basename(S.snd_file_name);
 // fprintf(stderr, tmp);
 // char basefilename[sizeof(tmp)+22];
 // strcpy(basefilename, tmp);
  
  // write out excitatory adjacency matrix
  FILE *ofile;
  ofile = fopen("Excitatory.adjMatrix", "w");
  
  for (int i = 0; i < totalCellsInNetwork; i++) {
     for (int j = 0; j < totalCellsInNetwork; j++) {
        fprintf(ofile, "%d ", excitatoryAdjacencyMatrix[row_order_offset(i, j, totalCellsInNetwork)]);
     }
     fprintf(ofile, "\n");
  }
  fclose(ofile);
  free(excitatoryAdjacencyMatrix);
  
  // write out inhibitory adjacency matrix
  ofile = fopen("Inhibitory.adjMatrix", "w");
  
  for (int i = 0; i < totalCellsInNetwork; i++) {
     for (int j = 0; j < totalCellsInNetwork; j++) {
     //   fprintf(ofile, "%d ", inhibitoryAdjacencyMatrix[i][j]);
        fprintf(ofile, "%d ", inhibitoryAdjacencyMatrix[row_order_offset(i, j, totalCellsInNetwork)]);
     }
     fprintf(ofile, "\n");
  }
  fclose(ofile);
  free(inhibitoryAdjacencyMatrix);
  
 // fprintf(ofile, "cell pop #1: %d\n", S.net.cellpop[0]);
 // fprintf(ofile, "cell pop #2: %d\n", S.net.cellpop[1]);
/*  fprintf(ofile, "cell pop count1: %d\n", array_length(S.net.cellpop, sizeof(int)));
  fprintf(ofile, "pop #1 number of cells1: %d\n", S.net.cellpop[0].cell_count);
  fprintf(ofile, "pop #1 number of cells2: %d\n", array_length(S.net.cellpop[0].cell, sizeof(int)));
  fprintf(ofile, "\n");
  fprintf(ofile, "%d\n", array_length(synapse[0], sizeof(int)));
*/                                      
//  fprintf(ofile, "%zu\n", array_length(synapse[0][0], sizeof(int)));
//  fprintf(ofile, "\n");
/*  int rowIndex_fromNeuron = 0;
  int columnIndex_toNeuron = 0;
  for (cpidx = 0; cpidx < S.net.cellpop_count; cpidx++) {
    CellPop *cp = S.net.cellpop + cpidx;
    int totalSynapses = 0;
    int totalTargets = 0;
    fprintf(ofile, "cell pop counter #%d\n", (cpidx+1));
    for (int cidx = 0; cidx < cp->cell_count; cidx++) {
      Cell *c = cp->cell + cidx;
      fprintf(ofile, "\tcell counter #%d\n", (cidx+1));
      for (int sn = 0; sn < c->syn_count; sn++) {
        fprintf(ofile, "\t\tsynapse counter #%d\n", (sn+1));
        totalSynapses += c->syn_count;
        for (int qn = 0; qn < c->syn[sn].q_count; qn++) { 
           fprintf(ofile, "\t\tq counter #%d\n", (qn+1));
        /*
          if(Debug) printf("cell->syn[%d].q[%d] = %lf\n", sn,qn,c->syn[sn].q[qn]);
          if (!(c->syn[sn].q[qn] == 0 || (c->syn[sn].q[qn] == 1 && S.ispresynaptic))) 
          {
            printf ("transmit: cell %d pop %d syn %d qval %d of %d is %a\n",
               cidx, cpidx, sn, qn, c->syn->q_count, c->syn[sn].q[qn]);
            fflush(stdout);
            DIE; // todo this should not be a fatal error
          }*/
   //     }
      //    fprintf(ofile, "1 ");
    /*      columnIndex_toNeuron++;
        
      }
      for (int tn = 0; tn < c->target_count; tn++) {
        fprintf(ofile, "\t\ttarget counter #%d\n", (tn+1));
        totalTargets += c->target_count;
        
      }
      if (c->syn_count == 0) {
      //  fprintf(ofile, "0 ");
      }
      //fprintf(ofile, "\n");
      rowIndex_fromNeuron++;
    }
    fprintf(ofile, "Total synapse count for cell population #%d: %d\n", (cpidx+1), totalSynapses);
    fprintf(ofile, "Total target count for cell population #%d: %d\n", (cpidx+1), totalTargets);
  }*/
}

// This checks the allocated q arrays to be 0 if normal
// or 1 if presynaptic modifiers are being used.
// Exits on error. This, of course, should "never" happen
void check_synapses (void)
{
  int cpidx;
  for (cpidx = 0; cpidx < S.net.cellpop_count; cpidx++) 
  {
    CellPop *cp = S.net.cellpop + cpidx;
    int cidx;
    for (cidx = 0; cidx < cp->cell_count; cidx++) 
    {
      Cell *c = cp->cell + cidx;
      int sn;
      for (sn = 0; sn < c->syn_count; sn++) 
      {
        int qn;
        for (qn = 0; qn < c->syn[sn].q_count; qn++)
        {
          if(Debug) printf("cell->syn[%d].q[%d] = %lf\n", sn,qn,c->syn[sn].q[qn]);
          if (!(c->syn[sn].q[qn] == 0 || (c->syn[sn].q[qn] == 1 && S.ispresynaptic))) 
          {
            printf ("transmit: cell %d pop %d syn %d qval %d of %d is %a\n",
               cidx, cpidx, sn, qn, c->syn->q_count, c->syn[sn].q[qn]);
            fflush(stdout);
            DIE; // todo this should not be a fatal error
          }
        }
      }
    }
  }
}

/* Polar (Box-Mueller) method; See Knuth v2, 3rd ed, p122 */
static double ran_gaussian (int seed)
{
  double x, y, r2;
  static int irand = 38986022; //randomly chosen seed

  if (seed) {
    irand = seed; 
    return 0;
  }
  
  do
    {
      /* choose x,y in uniform square (-1,-1) to (+1,+1) */

      x = -1 + 2 * ran (&irand);
      y = -1 + 2 * ran (&irand);

      /* see if it is in the unit circle */
      r2 = x * x + y * y;
    }
  while (r2 > 1.0 || r2 == 0);

  /* Box-Muller transform */
  return y * sqrt (-2.0 * log (r2) / r2);
}

void build_network ()
{
  int cpidx; // JAH: cell population index
  int fpidx; // JAH: fiber population index 

  static int seed[] = {2,3,7,12,14,15,16,17,19,20,22,23,24,25,26,29,35,36,39,
             42,45,47,48,49,50,51,52,53,54,56,57,59,61,63,64,66,67,
             69,75,78,79,80,83,84,85,86,87,89,90,91,95,97,98};

  /* values to match old behavior on sparc-v1 */
  static int rgseed[43] = {38986022, 1391292912, -146887594, -1953281662, 329184802, 1192591352,
                           -1719851788, -1818740196, 1864492360, -900638728, -1188332476,
                           341092380, -930484606, 336680986, 336680986, 829766224, 755328026,
                           1360987778, -1078372960, -1685500086, 1152748448, -1089100128,
                           -648232574, -776959814, -1810241108, -1074979646, -1932475256,
                           -814349978, 1703279180, -715985718, 362197582, -2079242792, 909292898,
                           -1932739534, -1381483014, 996303780, 1049529372, -1878520724,
                           1116996432, -631444766, -356560908, 1525966756, -516425190};

  /* Allocate the cells */
  int seed_size = (sizeof seed / sizeof seed[0])-1;
  int last_seed = seed[seed_size];

  Debug = false;
 
  printf("blah build_network.c\n");
  if(Debug) printf("allocate array of ptrs for %d pops\n",S.net.cellpop_count);

  TMALLOC (synapse, S.net.cellpop_count);

  for (cpidx = 0; cpidx < S.net.cellpop_count; cpidx++) 
  {
    if(Debug) printf(" cell pop %d\n",cpidx);
    CellPop *cp = S.net.cellpop + cpidx;
    int target_count = 0, tpidx, cidx;
    cp->noise_seed = cpidx > seed_size ? last_seed + cpidx : seed[cpidx];
    if (cp->haveLearn)
    {
       haveLearn = 1;
       learnCPop[numCPop++] = cpidx;
    }

    if (Debug) printf("allocate array of ptrs in synapse[%d] for %d cells\n",cpidx,cp->cell_count);
    TMALLOC (synapse[cpidx], cp->cell_count);

    for (tpidx = 0; tpidx < cp->targetpop_count; tpidx++)
      target_count += cp->targetpop[tpidx].NT;

    if(Debug) printf("allocate array of ptrs in S.net.cellpop+%d for %d cells\n",cpidx,cp->cell_count);
    TCALLOC (cp->cell, cp->cell_count);
    if (!getenv ("SIM_OLDRAN")) // this is undocumented, just used for testing/debugging??
       ran_gaussian(cpidx < 43 ? rgseed[cpidx] : 100 + cpidx); // set seed for this cell pop

    for (cidx = 0; cidx < cp->cell_count; cidx++) 
    {
      Cell *c = cp->cell + cidx;
      if (Debug) printf("allocate array of %d ptrs in synapse[%d][%d] for synapse types\n",S.net.syntype_count, cpidx,cidx);
      TCALLOC (synapse[cpidx][cidx], S.net.syntype_count);
      c->Vm = S.Vm0;
      c->Thr = cp->Th0 + ran_gaussian (0) * cp->Th0_sd;
      c->target_count = target_count;
      if (Debug) printf("allocate array of ptrs in S.net.cellpop.target+%d for %d targets\n",cpidx,c->target_count);
        // this allocates an array of *Target ptrs for each terminal
      TMALLOC (c->target, c->target_count);
    }
  }

  /* Allocate the fibers */
  for (fpidx = 0; fpidx < S.net.fiberpop_count; fpidx++) 
  {
    FiberPop *fp = S.net.fiberpop + fpidx;
    int target_count = 0, tpidx, fidx;
    if (fp->haveLearn)
    {
       haveLearn = true;
       learnFPop[numFPop++] = fpidx;
    }

    // adjust start and start in ms to ticks
    if (Debug) printf("start,stop in ms: %d  %d\n",fp->start, fp->stop);
    fp->start = ceil(fp->start / S.step);
    fp->stop = ceil(fp->stop / S.step);
    if (Debug) printf("start,stop in ticks: %d  %d\n",fp->start, fp->stop);

    if (fp->pop_subtype == ELECTRIC_STIM) // this needs a couple of other inits
    {
       fp->next_stim = fp->start;
       fp->next_fixed = fp->start;
       {if(Debug)printf("step: %f first stim at %d  %d\n",
             S.step, 
             fp->start,
             fp->next_stim);}
    } 
    else if (fp->pop_subtype == AFFERENT) 
    {
       openExternalSource(fp);
    }
    for (tpidx = 0; tpidx < fp->targetpop_count; tpidx++)
      target_count += fp->targetpop[tpidx].NT;

    if (Debug)printf("\nallocate array of %d fibers for fiber pop %d in S.net.fiberpop + %d\n",fp->fiber_count,fpidx, fpidx);
    TCALLOC (fp->fiber, fp->fiber_count);

    for (fidx = 0; fidx < fp->fiber_count; fidx++) {
      Fiber *f = fp->fiber + fidx;
      f->target_count = target_count;
      if (Debug)printf("allocate %d ptrs in S.net.fiberpop[%d].target\n",f->target_count,fidx);
      TMALLOC (f->target, f->target_count);
    }
  }
      if (Debug)printf("call for_cell_targets, fnc is count_to_synapses\n");
  for_cell_targets (count_to_synapses);
      if (Debug)printf("call for_fiber_targets, fnc is count_to_synapses\n");
  for_fiber_targets (count_to_synapses);

  printf("JAH: cell point count %d\n", S.net.cellpop_count);
   /* Allocate synapses */
  for (cpidx = 0; cpidx < S.net.cellpop_count; cpidx++) 
  {
  	printf("JAH: allocating synapses\n");
    CellPop *cp = S.net.cellpop + cpidx;
    if (Debug)printf("cell count: %d\n",cp->cell_count);
    int cidx;
    {if (Debug) printf("cell pop %d\n",cpidx);}
    for (cidx = 0; cidx < cp->cell_count; cidx++) 
    {
      Cell *c = cp->cell + cidx;
      int stidx;
      Syn *syn;
      {if(Debug) printf("\nallocate %d slots in Syn array\n",c->syn_count);}
      // this allocates an array pointed to by synapse[pop][cell][c_syn_count].ptr
      // for the 
      TCALLOC (c->syn, c->syn_count);
      syn = c->syn;
      for (stidx = 0; stidx < S.net.syntype_count; stidx++) 
      {
        {if (Debug) printf("syn type array idx (stidx): %d\n",stidx);}
        SynType *stp = S.net.syntype + stidx;
        BOUNDS (cidx, S.net.cellpop[cpidx].cell_count);
        if (synapse[cpidx][cidx][stidx].ptr != 0) // is in use by the model 
        {
          {if(Debug) printf("Cell pop %d cell %d\n",cpidx,cidx);}

          int n;
          BOUNDS (syn - c->syn, c->syn_count);
          syn->EQ = stp->EQ;
          syn->DCS = stp->DCS;
          syn->lrnWindow = stp->lrnWindow;
          syn->lrnStrMax = stp->lrnStrMax;
          syn->lrnStrDelta = stp->lrnStrDelta;
          syn->q_count = synapse[cpidx][cidx][stidx].maxqidx + 1;
          syn->cpidx = cpidx;
          syn->cidx = cidx;
          syn->stidx = stidx;
          syn->syntype = stp->SYN_TYPE;
          syn->synparent = stp->PARENT;

          {if (Debug) printf("add to synapse list:\n    EQ: %f\n    DCS:%f\n    q_count: %d\n    refers to synapse[%d][%d][%d]  type: %d parent: %d\n",
          syn->EQ,
          syn->DCS,
          syn->q_count,
          syn->cpidx,
          syn->cidx,
          syn->stidx,
          syn->syntype,
          syn->synparent);}

          TCALLOC (syn->q, syn->q_count);
          if (stp->SYN_TYPE == SYN_NOT_USED) // a bug
            fprintf(stdout,"Unexpected unused synapse in list\n");
          if (S.ispresynaptic && (stp->SYN_TYPE == SYN_PRE || stp->SYN_TYPE == SYN_POST))
          {
            if (stp->SYN_TYPE == SYN_PRE)
            {
              if (Debug) printf("add PRE to syn->q [%d][%d][%d]\n",cpidx,cidx,stidx);
            }
            else if (stp->SYN_TYPE == SYN_POST)
            {
              if (Debug) printf("add POST to syn->q [%d][%d][%d]\n",cpidx,cidx,stidx);
            }
            for (n = 0; n < syn->q_count; n++)
            {
              syn->q[n] = 1;             // initial conductance value (I think)
              if (Debug) printf(" syn[%d] = 1\n",n);
            }
          }
          if (stp->SYN_TYPE == SYN_LEARN)
          {
             {if (Debug) printf("add ptr to learn array of %d slots\n",LRN_SIZE);}
             TCALLOC (syn->lrn, LRN_SIZE);
             LEARN *lrn = syn->lrn;
             syn->lrn_size = LRN_SIZE;
             for (int lrs = 0; lrs < LRN_SIZE; ++lrs,++lrn)
                lrn->recv_pop = LRN_FREE;  // mark as unused slot
          }

          synapse[cpidx][cidx][stidx].ptr = syn++;
        }
      }
      syn - c->syn == c->syn_count || DIE;
    }
  }

  if (Debug) printf("call for_cell_targets, fnc is attacth_to_synapses\n");
  for_cell_targets (attach_to_synapses);
  if (Debug) printf("call for_fiber_targets, JAH fnc is attach_to_synapses\n");
  for_fiber_targets (attach_to_synapses);
  check_synapses ();
  
  writeAdjacencyMatrices();
  printf("JAH: exiting build_network\n");
  fflush(stdout);
  //exit(0);
}


/* The synapse variable is a 3D dynamically allocated set of arrays 
   JAH edits (12/28/2024): Previously, the comment said the synapse matrix was as follows:
      "It is used used like so:
       synapse[cellpop][cell][SynInfo]."
   when in fact it seems to be like the following (and "synapse" is of struct SynInfo):
   It is used used like so:
   synapse[cellpop][cell][SynType].

   slots are like this:
   First index: synapse[pop], array size is # of cell pops, each slot holds a pointer to:
    Second Index: synapse[0][cells], cells is an array of pointers for each cell in pop:

   synapse[0]-> [ptr, ptr, ptr, . . .  ]

   synapse[1]-> [ptr, ptr, ptr, . . .  ]
     These point to:

     Third index, an array of pointers to a SynType struct, one
     for each cell synapse target type.

   So, something like this:

   synapse [0][0][0]   cell pop 0, cell 0, SynType 0
   synapse [0][0][1]                       SynType 1
   synapse [0][1][0]               cell 1, SynType 0
   synapse [0][1][1]                       SynType 1
   synapse [0][1][2]               cell 1, SynType 2

   synapse [1][0][0]   cell pop 1, cell 0, SynType 0
    . . .

   There is a parallel set of arrays in the global S array that was 
   read in from the .sim file.
   S.net.cellpop holds an array of pointers, one per cell pop.
   Each one of these points to a Cell struct
   Fibers and synapses are handled similarly.

   Q and Q_COUNT
   It took me a while to figure out how the q and q_count vars were used.
   In the original Fortran code, the G array was a 4D array, and the last item
   was a fixed size array that was the synapse delay array.  The port to C put
   the array inside the synapse struct in the q array.  The max conduction
   time determines the size of the array.  The minimum conduction time
   determines how much of the array will be used.
   So, if the min is 3 and the max is 8, we have something like this:
      q[0]  q[1]  q[2]  q[3]  q[4]  q[5]  q[6]  q[7]
      [ 0 ] [ 0 ] [ 0 ] [ 0 ] [ 0 ] [ 0 ] [ 0 ] [ 0 ]

   A variable, the delay, is set to a random number 0-7. This is in effect
   the "origin" of each q array. In effect, the starting point of the delay
   varies for each synapse terminal.
   The slots hold 0 by default.
   When a fiber or cell fires, an index (step number + delay) % size
   determines the index. So, for step 12 with a delay of 3 and synapse
   strength of 0.1, (12+3)%8 = 7, 0.1 is added to q[7].
      q[0]  q[1]  q[2]  q[3]  q[4]  q[5]  q[6]  q[7]
      [ 0 ] [ 0 ] [ 0 ] [ 0 ] [ 0 ] [ 0 ] [ 0 ] [0.1]
   If the next terminal has a delay of 5, (12+5)&8 = 1
   so 0.1 is added to q[1]

      [ 0 ] [0.1] [ 0 ] [ 0 ] [ 0 ] [ 0 ] [ 0 ] [ 0 ]

   When the target cell state vars are updated, the cell calculates the index
   as step num & size. In this case, 12 % 8 = 4, which is zero. This has the
   effect a delay. In the next steps, 13 % 8 = 5, which is zero, 14 % 8 = 6, 15
   % 8 = 7 which is 0.1. This is "used" in calculating the potential, then
   zeroed.  In effect, the firing is finally "seen" for that terminal after a
   delay of 3 steps.  Note the delay can be longer, delay will be between the
   min and max depending on the delay value.

   Here is a printout of a run using a fiber pop of 1 connected to a cell pop of 1
   using a synapse min = 3, max = 8, 4 terminals, strength = 0.1

   step: 51
   Fiber fire
     Fiber: delay: 3 index: 7 val: 0.10
     Fiber: delay: 7 index: 3 val: 0.10
     Fiber: delay: 5 index: 1 val: 0.10
     Fiber: delay: 6 index: 2 val: 0.10
   step: 52  Cell Delay slot 4
   step: 53  Cell Delay slot 5
   step: 54  Cell Delay slot 6
   step: 55 Cell Using slot 7 0.10
   step: 56  Cell Delay slot 0
   step: 57 Cell Using slot 1 0.10
   step: 58 Cell Using slot 2 0.10
   step: 59 Cell Using slot 3 0.10
   step: 60  Cell Delay slot 4
   step: 61  Cell Delay slot 5
   step: 62  Cell Delay slot 6
   Fiber fire
     Fiber: delay: 3 index: 2 val: 0.10
     Fiber: delay: 7 index: 6 val: 0.10
     Fiber: delay: 5 index: 4 val: 0.10
     Fiber: delay: 6 index: 5 val: 0.10
   step: 63  Cell Delay slot 7
   step: 64  Cell Delay slot 0
   step: 65  Cell Delay slot 1
   step: 66 Cell Using slot 2 0.10
   step: 67  Cell Delay slot 3
   step: 68 Cell Using slot 4 0.10
   step: 69 Cell Using slot 5 0.10
   step: 70 Cell Using slot 6 0.10
   step: 71  Cell Delay slot 7
   step: 72  Cell Delay slot 0
   step: 73  Cell Delay slot 1
   step: 74  Cell Delay slot 2
   step: 75  Cell Delay slot 3
   step: 76  Cell Delay slot 4
   step: 77  Cell Delay slot 5
*/
