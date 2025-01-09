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


/* the hash generation breaks if you have a guard def, */
/* don't know why, seems like once INODE_H gets def'ed, the second */
/* stage sees no content on the second pass through the file. */
/* We don't def this in the Makefile, so the gen_hash.sh script works */
/* and the guard def works for compiles */

#ifdef GENHASHDEF
#undef SIMULATOR_H
#endif

#ifndef SIMULATOR_H
#define SIMULATOR_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include "common_def.h"
#include "util.h"


#define DECLARE_PTR_STR(v,t) t *v; char *v##_str = #t;

typedef struct {
  int stepnum;
  double ap;
  double rl;
  double dp;
  double d;
  unsigned int origin:1;
  unsigned int initialized:1;
} Slice;

#define LRN_SIZE 5
#define LRN_FREE -1 
#define LRN_GROWBY  5


// Info learning synapses need to manage the learn process
typedef struct 
{ 
   int recv_pop;  // receiving fiber/cell
   int send_term;  // receiving fiber/cell
   int recv_term; // receiving terminal
   int ariv_time; // arrival time (i.e., delay)
} LEARN;


typedef struct
{
  float G;//state
  float EQ;
  float DCS;
  int q_count;//state
  float *q;//state
  int cpidx; // JAH: target cell population index
  int cidx;  // JAH: target cell index
  int stidx; // JAH: SynType index? But this doesn't match syntype(!)
  int syntype;
  int synparent;
  float initial_strength;
  float lrn_strength;
  int   lrnWindow;
  float lrnStrMax;
  float lrnStrDelta;
  int lrn_size;
  LEARN *lrn;//indirect
} Syn;

typedef struct
{
  int delay;
  float strength;
  Syn *syn;//indirect
  unsigned int disabled:1;
} Target;

typedef struct
{
  float Vm_prev;//state
  float Vm;//state
  float Gk;//state
  float Thr;//state
  float gnoise_e;//state
  float gnoise_i;//state
  int spike;//state
  int syn_count;
  Syn *syn;//tag
  int target_count;
  Target *target;//skip
  float x;
  float y;
  float z;
  int sub_type;
} Cell;

typedef struct
{
  int state;
  double signal;
  int target_count;
  Target *target;
} Fiber;

typedef struct
{
  int MCT;   // min conductance time
  int NCT;   // conduction time
  int NT;    // # nterminals
  int IRCP;  // target nums
  int INSED; // target seed
  int TYPE;     // this is index into syn array, not really an explicit type
  float STR;  // synapse strength
} TargetPop;

typedef struct
{
  int start;
  int stop;
  int infsed;//istate
  float probability;
  int fiber_count;
  Fiber *fiber;//skip
  int targetpop_count;
  TargetPop *targetpop;//iskip
  int pop_subtype;
  int freq_type;   /* electric stim subtype params fixed or fuzzy*/
  float frequency;   /* how often to fire stim (Hz)*/
  float fuzzy_range; /* if fuzzy, width of range to randomly vary stim fire (ms) */
  int next_stim;//skip
  int next_fixed;//skip
  char *afferent_file_name;//string
  int num_aff;
  int offset;
  double aff_val[MAX_AFFERENT_PROB];
  double aff_prob[MAX_AFFERENT_PROB];
  double prev_signal;
  double slope_scale;
  void *affStruct;
  int haveLearn;
} FiberPop;

typedef struct
{
  double R0; 			/* step / membrane capacitance */
  float TMEM;
  float B;			/* K conductance change with AP */
  float DCG;			/* DCG   = exp (-step / TGK) */
  float GE0;			/* GE0   = IC + Gm0 * Vm0 */
  float MGC;			/* Accommodation Parameter */
  float DCTH;			/* exp(-step/TTH) */
  float Th0;			/* Resting threshold */
  float Th0_sd;			/* Resting threshold SD */

  float theta_h;
  float sigma_h;
  float theta_m;
  float sigma_m;
  float Vreset;
  float Vthresh;
  float delta_h;

  double taubar_h;
  double g_NaP_h;
  int orig_cell_count;
  int cell_count;
  Cell *cell;
  int targetpop_count;
  TargetPop *targetpop;//iskip
  float noise_amp;
  int noise_seed;//istate
  double fr;
  double TGK;
  char *name;
  char *ic_expression;//string
  void *ic_evaluator;
  int pop_subtype;
  int haveLearn;
} CellPop;

typedef struct
{
  float EQ;
  float DCS;
  int   lrnWindow;
  float lrnStrMax;
  float lrnStrDelta;
  int SYN_TYPE;
  int PARENT;
} SynType;

typedef struct
{
  int pop;
  int cell;
  int var;
  int type;
  char *lbl;//string
  float val;
  int spike;
} Plot;

typedef struct
{
  int fiberpop_count;
  FiberPop *fiberpop;
  int cellpop_count;
  CellPop *cellpop;
  int syntype_count;
  SynType *syntype;//iskip
} Network;

typedef struct
{
  int file_subversion;
  int slice_count;
  Slice *slice;  

  Network net;
  int step_count;
  float step;
  float Gm0;
  float Vm0;
  float Ek;

  char *snd_file_name;//string
  char *input_filename;//string
  int update_interval;
  char outsned;
  char save_spike_times;
  char save_smr;
  char save_smr_wave;
  char save_pop_total;
  int spawn_number;
  int plot_count;
  Plot *plot;//skip
  int fwrit_count;
  Plot *fwrit;//skip
  int cwrit_count;
  Plot *cwrit;//skip
  int nanlgid;
  int nanlgpop;
  int nanlgrate;
  double sclfct;
  double dcgint;
  FILE *ofile;
  FILE *ifile;
  int stepnum;//state
  int ispresynaptic;
  int nonoise;
  int seed;//state
  char *phrenic_equation;//skip
  void *pe_evaluator;
  char *lumbar_equation;//skip
  void *le_evaluator;
  double lmmfr;
  int    p1code;
  int    p1start;
  int    p1stop;
  int    p2code;
  int    p2start;
  int    p2stop;
  int ie_sample;
  int ie_smooth;
  float ie_plus;
  float ie_minus;
  int ie_freq;
} simulator_global;

extern simulator_global S;

double ran (int *i);
void build_network (void);
void simloop (void);
int read_sim ();
void update (void);
extern int sigterm;
void get_cell_coords (char *filename);
void list_slice_groups (void);
void cut_connections (Slice *pln);

#endif


