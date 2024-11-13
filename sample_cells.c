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

#include "simulator.h"
#include "sample_cells.h"
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "util.h"

extern char inPath[];
extern char outPath[];

typedef struct
{
  int cpidx;
  int cidx;
} PopCell;

typedef struct
{
  PopCell src;
  PopCell tar;
  int syntype;
} Connection;

static bool *cellpop_done;
static int sample_cell_count;
static int sample_cell_alloc;
static PopCell *sample_cell;
static int connection_count = 0;
static int connection_idx;
static Connection *connection;

static void
add_sample_cell (int pn, int cn)
{
  if (sample_cell_count == sample_cell_alloc)
    sample_cell = realloc (sample_cell, (++sample_cell_alloc) * sizeof *sample_cell);
  sample_cell[sample_cell_count].cpidx = pn;
  sample_cell[sample_cell_count].cidx = cn;
  sample_cell_count++;
}

static void
trace (int pn, int cn)
{
  CellPop *p = S.net.cellpop + pn;
  if (p->cell_count == 0)
    return;
  add_sample_cell (pn, cn);
  if (cellpop_done[pn])
    return;
  cellpop_done[pn] = true;

  Cell *c = p->cell + cn;
  int tn;

  bool targetpop_done[S.net.cellpop_count];
  bool targetpopsyn_done[S.net.cellpop_count][S.net.syntype_count];
  memset (targetpop_done, 0, sizeof targetpop_done);
  memset (targetpopsyn_done, 0, sizeof targetpopsyn_done);
  for (tn = 0; tn < c->target_count; tn++) {
    int tpn = c->target[tn].syn->cpidx;
    int stidx = c->target[tn].syn->stidx;
    if (targetpopsyn_done[tpn][stidx])
      continue;
    int tcn = c->target[tn].syn->cidx;
    connection_idx < connection_count || DIE;
    connection[connection_idx].src.cpidx = pn;
    connection[connection_idx].src.cidx = cn;
    connection[connection_idx].tar.cpidx = tpn;
    connection[connection_idx].tar.cidx = tcn;
    connection[connection_idx].syntype = c->target[tn].syn->stidx + 1;
    connection_idx++;
    trace (tpn, tcn);
    targetpop_done[tpn] = true;
    targetpopsyn_done[tpn][stidx] = true;
  }
}

static bool *poptrace_done;

static int
count_pops_done (void)
{
  int n, count = 0;
  for (n = 0; n < S.net.cellpop_count; n++)
    count += poptrace_done[n] == true;
  return count;
}

static void
pop_trace (int pn)
{
  CellPop *p = S.net.cellpop + pn;
  if (p->cell_count == 0)
    return;
  if (cellpop_done[pn] || poptrace_done[pn])
    return;
  poptrace_done[pn] = true;

  int tn;
  
  for (tn = 0; tn < p->targetpop_count; tn++) {
    int tpn = p->targetpop[tn].IRCP - 1;
    if (tpn >= 0)
      pop_trace (tpn);
  }
}

static int
get_best_pop (void)
{
  poptrace_done = malloc (S.net.cellpop_count * sizeof (bool));
  int pn;
  int best_pop = -1;
  int max_pops_done = 0;
  for (pn = 0; pn < S.net.cellpop_count; pn++) {
    if (cellpop_done[pn] || S.net.cellpop[pn].cell_count == 0)
      continue;
    memset (poptrace_done, 0, S.net.cellpop_count * sizeof (bool));
    pop_trace (pn);
    int pops_done = count_pops_done ();
    if (pops_done > max_pops_done) {
      max_pops_done = pops_done;
      best_pop = pn;
    }
  }
  free (poptrace_done);
  return best_pop;
}

static int
compare_popcell (const void *p1, const void *p2)
{
  PopCell *a = (PopCell *) p1;
  PopCell *b = (PopCell *) p2;
  if (a->cpidx == b->cpidx)
    return (a->cidx > b->cidx) - (a->cidx < b->cidx);
  return (a->cpidx > b->cpidx) - (a->cpidx < b->cpidx);
}

static int
compare_connection (const void *p1, const void *p2)
{
  Connection *a = (Connection *) p1;
  Connection *b = (Connection *) p2;
  if (a->src.cpidx != b->src.cpidx)
    return (a->src.cpidx > b->src.cpidx) - (a->src.cpidx < b->src.cpidx);
  if (a->tar.cpidx != b->tar.cpidx)
    return (a->tar.cpidx > b->tar.cpidx) - (a->tar.cpidx < b->tar.cpidx);
  return (a->syntype > b->syntype) - (a->syntype < b->syntype);
}

static int
lookup (int pop, int cell)
{
  int n;
  for (n = 0; n < sample_cell_count; n++)
    if (sample_cell[n].cpidx == pop && sample_cell[n].cidx == cell)
      return 101 + n;
  return 0;
}

static void
rosetta (void)
{
  FILE *f;
  int n;
  char *sfile_name;
  if (asprintf(&sfile_name, "%ssample_cells_rosetta_%02d.txt",outPath,S.spawn_number) == -1) exit (1);
  f = fopen (sfile_name, "w");
  free(sfile_name);
  if (f == NULL)
    return;
  fprintf (f, "unit pop cell\n");
  for (n = 0; n < sample_cell_count; n++)
    fprintf (f, "%4d %3d %3d\n", 101 + n, sample_cell[n].cpidx + 1, sample_cell[n].cidx + 1);
  fprintf (f, "\n");

  qsort (connection, connection_count, sizeof *connection, compare_connection);

  fprintf (f, "connections\n");
  fprintf (f, "src pop/cell  tar pop/cell  syntype  src unit  tar unit\n");
  for (n = 0; n < connection_count; n++)
    fprintf (f, "%7d/%03d  %7d/%03d  %7d  %7d  %7d\n",
             connection[n].src.cpidx + 1,
             connection[n].src.cidx + 1,
             connection[n].tar.cpidx + 1,
             connection[n].tar.cidx + 1,
             connection[n].syntype,
             lookup (connection[n].src.cpidx, connection[n].src.cidx),
             lookup (connection[n].tar.cpidx, connection[n].tar.cidx));
  fclose (f);
}

static void
unique (void)
{
  int n, to;
  if (sample_cell_count)
  {
    for (to = 0, n = 1; n < sample_cell_count; n++)
      if (memcmp (&sample_cell[n], &sample_cell[to], sizeof sample_cell[n]) != 0)
        sample_cell[++to] = sample_cell[n];
    sample_cell_count = to + 1;
  }
}

void
find_sample_cells (void)
{
  unsigned int seed;
  FILE *f;

  {
    int pn;
    connection_count = 0;
    for (pn = 0; pn < S.net.cellpop_count; pn++)
      if (S.net.cellpop[pn].cell_count > 0) {
        int tn;
        for (tn = 0; tn < S.net.cellpop[pn].targetpop_count; tn++)
          if ( S.net.cellpop[pn].targetpop[tn].NT > 0)
            connection_count++;
      }
    if (connection_count) {  
    connection = malloc (connection_count * sizeof *connection);
    connection_idx = 0;
    }
    else
      connection = 0;
    printf ("%d connections\n", connection_count);
  }

#define SEED_FILE "sample_cell_seed"
  char *seed_name;
  asprintf(&seed_name, "%s%s",outPath,SEED_FILE);
   // For max confusion, this is both an input and an output. We store in the output dir
  if ((f = fopen (seed_name, "r")) != NULL) {
    if (fscanf (f, "%u", &seed));
    fclose (f);
  }
  else {
    seed = time (0);
    if ((f = fopen (seed_name, "w")) != NULL) {
      fprintf (f, "%u\n", seed);
      fclose (f);
    }
  }
  free(seed_name);
  
  srand (seed);
  printf ("sample cell seed: %u\n", seed);
  cellpop_done = calloc (S.net.cellpop_count, sizeof (bool));
  sample_cell_count = 0;
  int pn;
  while ((pn = get_best_pop ()) >= 0)
    if (!cellpop_done[pn]) {
      int cn = floor (rand () / (RAND_MAX + 1.) * S.net.cellpop[pn].cell_count);
      trace (pn, cn);
    }
  qsort (sample_cell, sample_cell_count, sizeof *sample_cell, compare_popcell);
  unique ();
  rosetta ();
  int n;
  char *sfile_name;
  if (asprintf(&sfile_name, "%ssample_cells_%02d.ols",outPath,S.spawn_number) == -1) exit (1);
  f = fopen (sfile_name, "w");
  free(sfile_name);
  if (f == NULL)
    return;
  fprintf (f, "scr1\n");
  for (n = 0; n < sample_cell_count; n++)
    fprintf (f, "%d 2 %d %d\n", n, sample_cell[n].cpidx + 1, sample_cell[n].cidx + 1);
  fclose (f);
  free (cellpop_done);
  free (connection);
}
