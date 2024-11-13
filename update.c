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
#include <string.h>
#include <stdbool.h>
#include "simulator.h"

#if defined Q_OS_WIN
#include "lin2ms.h"
#endif
extern bool get_essentials(bool,bool,bool,bool);

static void
copy_state (Network *old, Network *new)
{
if (old->cellpop_count == new->cellpop_count)
   printf("cell pops the same\n");
if (old->fiberpop_count == new->fiberpop_count)
   printf("fiber pops the same\n");
printf("%p %p\n",old->fiberpop, new->fiberpop);
printf("%p %p\n",old->cellpop,new->cellpop);
printf("%p %p\n",old->syntype,new->syntype);


  int cellpop_count = MIN (old->cellpop_count, new->cellpop_count);
  int cpidx;
  int fiberpop_count = MIN (old->fiberpop_count, new->fiberpop_count);
  int fpidx;

  for (cpidx = 0; cpidx < cellpop_count; cpidx++) {
    CellPop *pold = old->cellpop + cpidx;
    CellPop *pnew = new->cellpop + cpidx;
    int cidx;
    int cell_count = MIN (pold->cell_count, pnew->cell_count);
    pnew->noise_seed = pold->noise_seed;
    for (cidx = 0; cidx < cell_count; cidx++) {
      Cell *cold = pold->cell + cidx;
      Cell *cnew = pnew->cell + cidx;
      int oldsidx, newsidx;
      cnew->Vm_prev = cold->Vm_prev;
      cnew->Vm = cold->Vm;
      cnew->Gk = cold->Gk;
      cnew->Thr = cold->Thr;
      cnew->spike = cold->spike;
      cnew->gnoise_e = cold->gnoise_e;
      cnew->gnoise_i = cold->gnoise_i;
      cnew->x = cold->x;
      cnew->y = cold->y;
      cnew->z = cold->z;
      for (oldsidx = 0; oldsidx < cold->syn_count; oldsidx++) {
	Syn *sold = cold->syn + oldsidx;
	int stidx = sold->stidx;
	for (newsidx = 0; newsidx < cnew->syn_count; newsidx++) {
	  Syn *snew = cnew->syn + newsidx;
	  if (snew->stidx == stidx) {
	    int q_count = MIN (sold->q_count, snew->q_count);
	    int qidx;
            snew->G = sold->G;
	    for (qidx = 0; qidx < q_count; qidx++)
	      snew->q[(S.stepnum + qidx) % snew->q_count] = sold->q[(S.stepnum + qidx) % sold->q_count];
	    break;
	  }
	}
      }
    }
  }
  for (fpidx = 0; fpidx < fiberpop_count; fpidx++) {
    FiberPop *pold = old->fiberpop + fpidx;
    FiberPop *pnew = new->fiberpop + fpidx;
    pnew->infsed = pold->infsed;
  }
}

static void
free_net (Network *np)
{
  int cpidx, fpidx;
  for (cpidx = 0; cpidx < np->cellpop_count; cpidx++) {
    CellPop *p = np->cellpop + cpidx;
    int cidx;
    for (cidx = 0; cidx < p->cell_count; cidx++) {
      Cell *c = p->cell + cidx;
      int sidx;
      for (sidx = 0; sidx < c->syn_count; sidx++) {
	Syn *s = c->syn + sidx;
	free (s->q);
      }
      free (c->syn);
      free (c->target);
    }
    free (p->cell);
    free (p->targetpop);
  }
  free (np->cellpop);
  for (fpidx = 0; fpidx < np->fiberpop_count; fpidx++) {
    FiberPop *p = np->fiberpop + fpidx;
    int fidx;
    for (fidx = 0; fidx < p->fiber_count; fidx++) {
      Fiber *f = p->fiber + fidx;
      free (f->target);
    }
    free (p->fiber);
    free (p->targetpop);
  }
  free (np->fiberpop);
  free (np->syntype);
}

// Called in response to command from simbuild.
// A sim file with updated params is available. Use the
// new params.
void
update (void)
{
   printf("Loading mid-run update parameters. . .\n");
   fflush(stdout);
   simulator_global old_S = S, new_S;
   memset (&S.net, 0, sizeof S.net);
   get_essentials(false,false,true,false);
   read_sim();
   new_S = S;
   S = old_S;
    
   S.net            = new_S.net;
   S.step_count     = new_S.step_count;
   S.step           = new_S.step;
   S.Gm0            = new_S.Gm0;
   S.Vm0            = new_S.Vm0;
   S.Ek             = new_S.Ek;
   S.snd_file_name  = new_S.snd_file_name;
   copy_state (&old_S.net, &S.net);
   free_net (&old_S.net);

   printf("Mid-run update parameters have been loaded.\n");
   fflush(stdout);
}
