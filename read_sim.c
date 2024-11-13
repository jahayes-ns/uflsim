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
#if defined WIN32
#define _CRT_RAND_S
#endif

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>

#include "simulator.h"
#include "fileio.h"
#include  "inode.h"
#include "inode_hash.h"
#include "lung.h"
#include "c_globals.h"
#include "sample_cells.h"

#if defined WIN32
#include <libloaderapi.h>
#include <fcntl.h>
#include "lin2ms.h"
#endif

inode_global D;

extern double lmmfr;
extern char sim_fname[];
extern char *sim_ptr;
extern int sim_size;
extern char *snd_ptr;
extern int snd_size;
extern int condi_flag;
extern char inPath[];
extern char outPath[];

struct StructInfo *(*struct_info_fn) (const char *str, unsigned int len);
struct StructMembers *(*struct_members_fn) (const char *str, unsigned int len);

static char *
syn_txt (int stidx, int type)
{
  static char *s;
  char *fmt = "";
  free (s);
  
   switch (type) {
  case SYN_NORM: fmt = "%d"     ; break;
  case SYN_PRE: fmt  = "pre %d" ; break;
  case SYN_POST: fmt = "post %d"; break;
  case SYN_NOT_USED: fmt = "%d" ; fprintf(stdout,"Unexpected unused synapse in report\n");break;
  }
  if (asprintf (&s, fmt, stidx + 1) == -1) exit (1);
  return s;
}

static inline int
max_cells_per_pop (void)
{
  static int max;
  int pn;
  if (max == 0)
    for (pn = 0; pn < S.net.cellpop_count; pn++)
      if (S.net.cellpop[pn].cell_count > max)
        max = S.net.cellpop[pn].cell_count;
  return max;
}


#if HAVE_LIBJUDY
#include <Judy.h>

static void
alloc_array (Pvoid_t *ap)
{
  *ap = (Pvoid_t) NULL;
}

static inline PWord_t
get_ptr (Pvoid_t *ap, int pn, int tp, int st, int cn)
{
  PWord_t PValue;
  Word_t index = ((pn * S.net.cellpop_count + tp) * S.net.syntype_count + st) * max_cells_per_pop () + cn;
  JLI (PValue, *ap, index);
  PValue || DIE;
  return PValue;
}

static inline int
fetch (Pvoid_t *ap, int pn, int tp, int st, int cn)
{
  PWord_t PValue = get_ptr (ap, pn, tp, st, cn);
  return *PValue;
}

static inline void
increment (Pvoid_t *ap, int pn, int tp, int st, int cn)
{
  PWord_t PValue = get_ptr (ap, pn, tp, st, cn);
  (*PValue)++;
}

static void
free_array (Pvoid_t *ap)
{
  Word_t   Rc_word;
  JLFA (Rc_word, *ap);
}
static Pvoid_t cv, dv;

#else /* !HAVE_LIBJUDY */

static void
alloc_array (unsigned short *****ap)
{
  unsigned short ****s;
  int pn, tp, st;

  TMALLOC (s, S.net.cellpop_count);
  for (pn = 0; pn < S.net.cellpop_count; pn++) {
    TMALLOC (s[pn], S.net.cellpop_count);
    for (tp = 0; tp < S.net.cellpop_count; tp++) {
      TMALLOC (s[pn][tp], S.net.syntype_count);
      for (st = 0; st < S.net.syntype_count; st++)
        TCALLOC (s[pn][tp][st], max_cells_per_pop ());
    }
  }
  *ap = s;
}

static inline int
fetch (unsigned short *****ap, int pn, int tp, int st, int cn)
{
  return (*ap)[pn][tp][st][cn];
}

static inline void
increment (unsigned short *****ap, int pn, int tp, int st, int cn)
{
  (*ap)[pn][tp][st][cn]++;
}

static void
free_array (unsigned short *****ap)
{
  unsigned short ****s = *ap;
  int pn, tp, st;

  for (pn = 0; pn < S.net.cellpop_count; pn++) {
    for (tp = 0; tp < S.net.cellpop_count; tp++) {
      for (st = 0; st < S.net.syntype_count; st++)
        free (s[pn][tp][st]);
      free (s[pn][tp]);
    }
    free (s[pn]);
  }
  free (s);
  *ap = s;
}
static unsigned short ****cv, ****dv;
#endif /* !HAVE_LIBJUDY */

static void
condi_mean_sdev (void)
{
  char *cfile_name;
  FILE *f;

  if (asprintf(&cfile_name, "%scondi_mean_sdev_%02d.csv",outPath,S.spawn_number) == -1) exit (1);
  (f = fopen (cfile_name, "w")) || DIE;
  free(cfile_name);
  fprintf (f, "SP,TP,Syntype,MCT,NCT,NT,STR,SCNT,TCNT,DV_mean,DV_sdev,NT_mean,CV_mean,CV_sdev\n");
  int cpidx;
  for (cpidx = 0; cpidx < S.net.cellpop_count; cpidx++) {
    CellPop *cp = S.net.cellpop + cpidx;
    if (cp->cell_count == 0)
      continue;
    int tpidx;
    for (tpidx = 0; tpidx < cp->targetpop_count; tpidx++) {
      TargetPop *tp = cp->targetpop + tpidx;
      if (tp->IRCP == 0)
        continue;
      int tcpidx = tp->IRCP - 1;
      CellPop *tcp = S.net.cellpop + tcpidx;
      if (tcp->cell_count == 0)
        continue;
      int stidx = tp->TYPE - 1;
      int explicit_syn_type=S.net.syntype[stidx].SYN_TYPE;
      int cn, n = 0;
      unsigned long long sum = 0, sumsq = 0;
      for (cn = 0; cn < cp->cell_count; cn++) {
        n = fetch (&dv, cpidx, tcpidx, stidx, cn);
        sum += n;
        sumsq += n * n;
      }
      if (sum == 0)
        continue;
      n = cp->cell_count;
      double mean = (double)sum / n;
      double sdev = sqrt ((n * sumsq - sum * sum) / (n * (n - 1.)));
      fprintf (f, "%d,%d,%s,%d,%d,%d,%g,%d,%d,%g,%g,%g,", cpidx + 1, tcpidx + 1, syn_txt (stidx,explicit_syn_type),
               tp->MCT,tp->NCT,tp->NT,tp->STR, cp->cell_count, tcp->cell_count, mean, sdev, tp->NT / mean);
      sum = 0, sumsq = 0;
      for (cn = 0; cn < tcp->cell_count; cn++) {
        n = fetch (&cv, cpidx, tcpidx, stidx, cn);
        sum += n;
        sumsq += n * n;
      }
      n = tcp->cell_count;
      mean = (double)sum / n;
      sdev = sqrt ((n * sumsq - sum * sum) / (n * (n - 1.)));
      fprintf (f, "%g,%g\n", mean, sdev);
    }
  }
  fclose (f);
}

typedef struct
{
  int cpidx;
  int cidx;
  int stidx;
} Term;

typedef struct
{
  Term term;
  int count;
} TermTally;

static int termtally_count;
static int termtally_alloc;
static TermTally *termtally;

#include  <time.h>
void
condi (void)
{
  time_t start = time (0);
  FILE *f;
  int pn;
  char *cfile_name;

  alloc_array (&cv);
  alloc_array (&dv);
  if (asprintf (&cfile_name, "%scondi_%02d.csv",outPath,S.spawn_number) == -1) exit (1);
  (f = fopen (cfile_name, "w")) || DIE;
  free(cfile_name);
  fprintf (f, "SP,SC,TP,TC,Terms.,Syntype\n");
  for (pn = 0; pn < S.net.cellpop_count; pn++) {
    CellPop *p = S.net.cellpop + pn;
    int cn;
    for (cn = 0; cn < p->cell_count; cn++) {
      Cell *c = p->cell + cn;
      int tidx;
      int n;
      termtally_count = 0;
      for (tidx = 0; tidx < c->target_count; tidx++) {
         Target *target = c->target + tidx;
         Syn *syn = target->syn;
         Term newterm = {syn->cpidx, syn->cidx, syn->stidx};
         for (n = 0; n < termtally_count; n++)
         if (memcmp (&termtally[n].term, &newterm, sizeof (Term)) == 0) {
            termtally[n].count++;
            break;
          }
          if (n == termtally_count) {
             if (termtally_count == termtally_alloc)
                TREALLOC (termtally, termtally_alloc += 256);
             termtally[n].term = newterm;
             termtally[n].count = 1;
             termtally_count++;
          }
      }
      for (n = 0; n < termtally_count; n++) {
         Term *t = &termtally[n].term;
        //        printf ("src %3d%03d tar %3d st %2d size %d\n", pn, cn, t->cpidx, t->stidx, p->cell_count);
        increment (&dv, pn, t->cpidx, t->stidx, cn);
        increment (&cv, pn, t->cpidx, t->stidx, t->cidx);
        fprintf (f, "%d,%d,%d,%d,%d,", pn + 1, cn + 1, t->cpidx + 1, t->cidx + 1, termtally[n].count);

         int explicit_syn_type = S.net.syntype[t->stidx].SYN_TYPE;
         if (explicit_syn_type == SYN_NORM)
            fprintf (f, "%d\n", t->stidx + 1);
         else if (explicit_syn_type == SYN_PRE)
            fprintf (f, "pre %d\n", t->stidx + 1);
         else if (explicit_syn_type == SYN_POST)
            fprintf (f, "post %d\n", t->stidx + 1);
         else if (explicit_syn_type == SYN_NOT_USED)
           fprintf(f,"not used");  // this is probably a bug
      }
    }
  }
  condi_mean_sdev ();
  fclose (f);
  free_array (&cv);
  free_array (&dv);
  fprintf(stdout,"condi took %ld seconds\n", time (0) - start);
}

static void
quiet_model ()
{
  int cpidx, fpidx;

  for (cpidx = 0; cpidx < S.net.cellpop_count; cpidx++)
    S.net.cellpop[cpidx].noise_amp = 0;
  for (fpidx = 0; fpidx < S.net.fiberpop_count; fpidx++) {
    FiberPop *fp = S.net.fiberpop + fpidx;
    int tpidx;
    double probability = fp->probability;
    fp->probability = 1;
    for (tpidx = 0; tpidx < fp->targetpop_count; tpidx++) {
      TargetPop *tp = fp->targetpop + tpidx;
   int explicit_syn_type=S.net.syntype[tp->TYPE-1].SYN_TYPE;
     if (explicit_syn_type  == SYN_NOT_USED) // probably a bug
        fprintf(stdout,"Unexpected unused synapse in quiet model\n");
      if (!S.ispresynaptic || explicit_syn_type == SYN_NORM) 
         tp->STR *= probability;
      else if (tp->STR < 1)
         tp->STR = pow (tp->STR, probability);
      else
         tp->STR = (tp->STR - 1) * probability + 1;
    }
  }
}


void read_snd()
{
  FILE *f = NULL;
  memset (D.inode, 0, sizeof D.inode);
  struct_info_fn = inode_struct_info;
  struct_members_fn = inode_struct_members;
  struct stat filestat;

  if (!snd_ptr)
  {
    if (strlen(inPath))  // Don't use path in snd file, use input destination arg.
    {                    // It better be there.
       char *tmp = basename(S.snd_file_name);
       char *fname;
       asprintf(&fname,"%s%s",inPath,tmp);
       stat(fname,&filestat);
       if (S_ISDIR(filestat.st_mode))
       {
          printf("The filename %s is a directory, exiting program. . .\n",fname);
          free(fname);
          exit(1);
       }
       f = fopen(fname,"r");
       if (!f)
       {
          printf("Cannot open model paramter file %s, exiting program. . .\n", fname);
          free(fname);
          exit(1);
       }
       else
          free(fname);
    }
    else
    {
       f = fopen(S.snd_file_name,"r");
       if (!f)
       {
          printf("Cannot open model paramter file %s, exiting program. . .\n", S.snd_file_name);
          exit(1);
       }
    }
  }
  else
  {
#if defined __linux__
    f = fmemopen(snd_ptr,snd_size,"r");
#else
    char namebuff[MAX_PATH];
    unsigned int randno;
    HANDLE hnd1, hnd2;

    rand_s(&randno);
    snprintf(&namebuff,sizeof(namebuff),"sndrun%x.tmp",randno);
    hnd1 = CreateFile(namebuff, GENERIC_READ | GENERIC_WRITE, 0, NULL, 
      CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    hnd2 = _open_osfhandle((intptr_t)hnd1,_O_RDONLY);
    f = _fdopen(hnd2,"r+");
    fwrite(snd_ptr,1, snd_size, f);
    rewind(f);
#endif
  }
  if (load_struct_read_version(f)) 
  {
    load_struct (f, "inode_global", &D, 1);
    fclose (f);
  }
  else // too old
  {
     return;
  }

  int n, pn;
  baby_lung_flag = D.baby_lung_flag;
  for (n = 0; n < MAX_INODES; n++) 
  {
    if (D.inode[n].node_type == CELL) 
    {
      pn = D.inode[n].node_number - 1;
      S.net.cellpop[pn].name = strdup (D.inode[n].comment1);
    }
    else if (D.inode[n].node_type == GLOBAL)
    {
      S.phrenic_equation = strdup (D.inode[n].unode.global_node.phrenic_equation);
      S.lumbar_equation = strdup (D.inode[n].unode.global_node.lumbar_equation);
    }
  }
}

// If we are using the direct connect, the sim file has already been
// obtained from simbuild. Otherwise, read from file.
int read_sim()
{
  if (sim_ptr == 0)
  {
    S.ifile = fopen(sim_fname,"r");
    if (!S.ifile)
    {
       printf("Cannot open simulator parameter file %s, exiting program. . .\n",sim_fname);
       exit(1);
    }
  }
  else
  {
#if defined __linux__
    fprintf(stdout,"Using direct connect image\n");
    fflush(stdout);
    S.ifile = fmemopen(sim_ptr,sim_size,"r");
#else
    char namebuff[MAX_PATH];
    unsigned int randno;
    HANDLE hnd1, hnd2;

    rand_s(&randno);
    snprintf(&namebuff,sizeof(namebuff),"simrun%x.tmp",randno);
    hnd1 = CreateFile(namebuff, GENERIC_READ | GENERIC_WRITE, 0, NULL, 
      CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    hnd2 = _open_osfhandle((intptr_t)hnd1,_O_RDONLY);
    S.ifile = _fdopen(hnd2,"r+");
    fwrite(sim_ptr,1, sim_size, S.ifile);
    rewind(S.ifile);
#endif
  }

  struct_info_fn = simulator_struct_info;
  struct_members_fn = simulator_struct_members;
  if (load_struct_read_version (S.ifile)) {
    load_struct (S.ifile, "simulator_global", &S, 1);
    lmmfr = S.lmmfr;
  }
  else {
    exit(1);    // not much to do, just leave
  }
  read_snd();

  if (0)
  {
    char *file_name;
    FILE *f;
    save_all = 1, save_state = 0, save_init = 0;
    if (asprintf (&file_name, "read_sim_out") == -1) exit (1);
    struct_info_fn = simulator_struct_info;
    struct_members_fn = simulator_struct_members;
    (f = save_struct_open (file_name)) || DIE;
    save_struct (f, "simulator_global", &S);
    fclose (f);
    free (file_name);
    exit (0);
  }

  fprintf(stdout,"SIMRUN: Building network. . .\n");
  if (S.nonoise)
    quiet_model ();
  build_network ();
  find_sample_cells ();
  if (condi_flag) 
    condi();
  fprintf(stdout,"network built, running sim\n");
  if (sim_ptr)
  {
     free(sim_ptr);
     sim_ptr = 0;
     sim_size = 0;
  }

  return 0;
}
