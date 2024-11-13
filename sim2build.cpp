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

/* Some wrapper functions and some modified versions of existing
   simulator functions to unentangle the gui stuff from the workhorse stuff.
   Some of the C got got moved here into c++ land so I can have it do Qt UI stuff.
   Some older files truncated, some deleted completely.
*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <sys/types.h>
#include <iostream>
#include <QString>
#include <QStringList>
#include <QMessageBox>
#include <dirent.h>

#include "lin2ms.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "inode.h"
#include "fileio.h"
#include "simulator.h"
#include "simulator_hash.h"
#include "swap.h"
#include "fileio.h"
#include "inode_hash.h"
#include "inode_2.h"
#include "c_globals.h"
#ifdef __cplusplus
}
#endif

using namespace std;

void alert (char *msg)
{
   QMessageBox mbox;
   mbox.setText(msg);
   mbox.setIcon(QMessageBox::Warning);
   mbox.exec();
}

#define COPY(to,from,fld) if (sizeof to->fld != sizeof from->fld) DIE; \
                memcpy (from->fld, to->fld, sizeof from->fld)

#define EQUAL(from,to,fld) to->fld = from->fld;



static void
copy_old_inode (I_NODE *inode, OLD_I_NODE *old_inode)
{
  int n, node_type;
  memset (inode, 0, sizeof (I_NODE) * OLD_MAX_INODES);

  for (n = 0; n < OLD_MAX_INODES; n++) {
    unsigned int swaptest = 1;
    node_type = inode[n].node_type = old_inode[n].node_type;
    if (*(char *) &swaptest)
      SWAPLONG (node_type);
    inode[n].node_number = old_inode[n].node_number;
    memcpy (inode[n].comment1, old_inode[n].comment1, 100);
    memcpy (inode[n].comment2, old_inode[n].comment2, 100);
    if (n == 0)
      node_type = GLOBAL;
    if (n == 99)
      node_type = SYNAPSE;
    switch (node_type) {
    case UNUSED:
      continue;
    case fibeR:
       // simbuild does this differently, these are
       // really unused, we mange pop #s differenlty.
       inode[n].node_type = UNUSED;
       strcpy(inode[n].comment1,"This node is Unused");
       D.num_used_nodes[FIBER]--;
       break;
    case FIBER:
      {
 OLD_F_NODE *from = &old_inode[n].unode.fiber_node;
 F_NODE *to = &inode[n].unode.fiber_node;
 EQUAL (to,from,fiber_x);
 EQUAL (to,from,fiber_y);
 COPY (to,from,fiber_axon);
 EQUAL (to,from,f_prob);
 EQUAL (to,from,f_begin);
 EQUAL (to,from,f_end);
 EQUAL (to,from,f_seed);
 EQUAL (to,from,f_pop);
 EQUAL (to,from,f_targets);
 COPY (to,from,f_target_nums);
 memset (to->f_min_conduct_time, 0, sizeof to->f_min_conduct_time);
 COPY (to,from,f_conduction_time);
 COPY (to,from,f_terminals);
 COPY (to,from,f_synapse_type);
 COPY (to,from,f_synapse_strength);
 COPY (to,from,f_target_seed);
 if (maxFiberPop < inode[n].node_number)
   maxFiberPop = inode[n].node_number;
      }
      break;
    case celL:
       // simbuild does this differently, these are
       // really unused, we mng pop #s differenlty.
       inode[n].node_type = UNUSED;
       strcpy(inode[n].comment1,"This node is Unused");
       D.num_used_nodes[CELL]--;
       //if (maxCellPop < inode[n].node_number)
       //   maxCellPop = inode[n].node_number;
       break;

    case CELL:
       {
 OLD_C_NODE *from = &old_inode[n].unode.cell_node;
 C_NODE *to = &inode[n].unode.cell_node;

 EQUAL (to,from,cell_x);
 EQUAL (to,from,cell_y);
 COPY (to,from,cell_axon);
 EQUAL (to,from,c_accomodation);
 EQUAL (to,from,c_k_conductance);
 EQUAL (to,from,c_mem_potential);
 EQUAL (to,from,c_resting_thresh);
 EQUAL (to,from,c_ap_k_delta);
 EQUAL (to,from,c_accom_param);
 EQUAL (to,from,c_pop);
 EQUAL (to,from,c_targets);
 COPY (to,from,c_target_nums);
 memset (to->c_min_conduct_time, 0, sizeof to->c_min_conduct_time);
 COPY (to,from,c_conduction_time);
 COPY (to,from,c_terminals);
  COPY (to,from,c_synapse_type);
 COPY (to,from,c_synapse_strength);
 COPY (to,from,c_target_seed);
 EQUAL (to,from,c_rebound_param);
 EQUAL (to,from,c_rebound_time_k);
 EQUAL (to,from,c_thresh_remove_ika);
 EQUAL (to,from,c_thresh_active_ika);
 EQUAL (to,from,c_max_conductance_ika);
 EQUAL (to,from,c_pro_deactivate_ika);
 EQUAL (to,from,c_pro_activate_ika);
 EQUAL (to,from,c_constant_ika);
 EQUAL (to,from,c_dc_injected);
 if (maxCellPop < inode[n].node_number)
   maxCellPop = inode[n].node_number;
      }
      break;
    case SYNAPSE:
      {
 OLD_S_NODE *from = &old_inode[n].unode.synapse_node;
 S_NODE *to = &inode[n].unode.synapse_node;
 memcpy (to, from, sizeof *to);
 COPY (to,from,synapse_name);
 COPY (to,from,s_eq_potential);
 COPY (to,from,s_time_constant);
      }
      break;
    case GLOBAL:
      {
 OLD_GLOBAL_NETWORK *from = &old_inode[n].unode.global_node;
 GLOBAL_NETWORK *to = &inode[n].unode.global_node;
 EQUAL (to,from,total_populations);
 EQUAL (to,from,total_fibers);
 EQUAL (to,from,total_cells);
 EQUAL (to,from,sim_length);
 EQUAL (to,from,k_equilibrium);
 EQUAL (to,from,max_conduction_time);
 EQUAL (to,from,num_synapse_types);
 EQUAL (to,from,max_targets);
 EQUAL (to,from,step_size);
 EQUAL (to,from,maximum_cells_per_pop);
 COPY (to,from,global_comment);
      }
      break;
    default:
      DIE;
      break;
    }
  }
}


static void
copy_inode_2 (I_NODE *inode, I_NODE_2 *inode_2)
{
  int n, node_type;
  memset (inode, 0, sizeof (I_NODE) * OLD_MAX_INODES);

  for (n = 0; n < OLD_MAX_INODES; n++) {
    unsigned int swaptest = 1;
    node_type = inode[n].node_type = inode_2[n].node_type;
    if (*(char *) &swaptest)
      SWAPLONG (node_type);
    inode[n].node_number = inode_2[n].node_number;
    memcpy (inode[n].comment1, inode_2[n].comment1, 100);
    memcpy (inode[n].comment2, inode_2[n].comment2, 100);
    if (n == 0)
      node_type = GLOBAL;
    if (n == 99)
      node_type = SYNAPSE;
    switch (node_type) {
    case UNUSED:
      continue;
    case fibeR:
       inode[n].node_type = UNUSED; // simbuild does this differently
       strcpy(inode[n].comment1,"This node is Unused");
       D.num_used_nodes[FIBER]--;
       break;
    case FIBER:
      {
 F_NODE_2 *from = &inode_2[n].unode.fiber_node;
 F_NODE *to = &inode[n].unode.fiber_node;
 EQUAL (to,from,fiber_x);
 EQUAL (to,from,fiber_y);
 COPY (to,from,fiber_axon);
 EQUAL (to,from,f_prob);
 EQUAL (to,from,f_begin);
 EQUAL (to,from,f_end);
 EQUAL (to,from,f_seed);
 EQUAL (to,from,f_pop);
 EQUAL (to,from,f_targets);
 COPY (to,from,f_target_nums);
 COPY (to,from,f_min_conduct_time);
 COPY (to,from,f_conduction_time);
 COPY (to,from,f_terminals);
 COPY (to,from,f_synapse_type);
 COPY (to,from,f_synapse_strength);
 COPY (to,from,f_target_seed);
 if (maxFiberPop < inode[n].node_number)
   maxFiberPop = inode[n].node_number;
      }
      break;

    case celL:
       inode[n].node_type = UNUSED; // simbuild does this differently
       strcpy(inode[n].comment1,"This node is Unused");
       D.num_used_nodes[CELL]--;
       break;
    case CELL:
      {
 C_NODE_2 *from = &inode_2[n].unode.cell_node;
 C_NODE *to = &inode[n].unode.cell_node;

 EQUAL (to,from,cell_x);
 EQUAL (to,from,cell_y);
 COPY (to,from,cell_axon);
 EQUAL (to,from,c_accomodation);
 EQUAL (to,from,c_k_conductance);
 EQUAL (to,from,c_mem_potential);
 EQUAL (to,from,c_resting_thresh);
 EQUAL (to,from,c_ap_k_delta);
 EQUAL (to,from,c_accom_param);
 EQUAL (to,from,c_pop);
 EQUAL (to,from,c_targets);
 COPY (to,from,c_target_nums);
 COPY (to,from,c_min_conduct_time);
 COPY (to,from,c_conduction_time);
 COPY (to,from,c_terminals);
  COPY (to,from,c_synapse_type);
 COPY (to,from,c_synapse_strength);
 COPY (to,from,c_target_seed);
 EQUAL (to,from,c_rebound_param);
 EQUAL (to,from,c_rebound_time_k);
 EQUAL (to,from,c_thresh_remove_ika);
 EQUAL (to,from,c_thresh_active_ika);
 EQUAL (to,from,c_max_conductance_ika);
 EQUAL (to,from,c_pro_deactivate_ika);
 EQUAL (to,from,c_pro_activate_ika);
 EQUAL (to,from,c_constant_ika);
 EQUAL (to,from,c_dc_injected);
      }
      break;
    case SYNAPSE:
      {
 S_NODE_2 *from = &inode_2[n].unode.synapse_node;
 S_NODE *to = &inode[n].unode.synapse_node;
 memcpy (to, from, sizeof *to);
 COPY (to,from,synapse_name);
 COPY (to,from,s_eq_potential);
 COPY (to,from,s_time_constant);
      }
      break;
    case GLOBAL:
      {
 GLOBAL_NETWORK_2 *from = &inode_2[n].unode.global_node;
 GLOBAL_NETWORK *to = &inode[n].unode.global_node;
 EQUAL (to,from,total_populations);
 EQUAL (to,from,total_fibers);
 EQUAL (to,from,total_cells);
 EQUAL (to,from,sim_length);
 EQUAL (to,from,k_equilibrium);
 EQUAL (to,from,max_conduction_time);
 EQUAL (to,from,num_synapse_types);
 EQUAL (to,from,max_targets);
 EQUAL (to,from,step_size);
 EQUAL (to,from,maximum_cells_per_pop);
 COPY (to,from,global_comment);
      }
      break;
    default:
      DIE;
      break;
    }
  }
}

static OLD_I_NODE *dbg_inode;

static void
copy_new_inode (OLD_I_NODE *inode, I_NODE *old_inode)
{
  int n, node_type = 0;

  if (((int *)dbg_inode)[2] == 0) {printf ("%s line %d: n = %d, type %x\n", __FILE__, __LINE__, -1, node_type); exit (0);}
  memset (inode, 0, sizeof (OLD_I_NODE) * OLD_MAX_INODES);
  if (((int *)dbg_inode)[2] == 0) {printf ("%s line %d: n = %d, type %x\n", __FILE__, __LINE__, -1, node_type); exit (0);}

  for (n = 0; n < OLD_MAX_INODES; n++) {
    unsigned int swaptest = 1;
    node_type = inode[n].node_type = old_inode[n].node_type;

    if (*(char *) &swaptest)
      SWAPLONG (node_type);

    inode[n].node_number = old_inode[n].node_number;

    if (((int *)dbg_inode)[2] == 0) {printf ("%s line %d: n = %d, type %x\n", __FILE__, __LINE__, n, node_type); exit (0);}
    memcpy (inode[n].comment1, old_inode[n].comment1, 100);
    if (((int *)dbg_inode)[2] == 0) {printf ("%s line %d: n = %d, type %x\n", __FILE__, __LINE__, n, node_type); exit (0);}

    memcpy (inode[n].comment2, old_inode[n].comment2, 100);
    if (n == 0)
      node_type = GLOBAL;
    if (n == 99)
      node_type = SYNAPSE;
    switch (node_type) {
    case UNUSED:
      continue;
    case fibeR:
       inode[n].node_type = UNUSED; // simbuild does this differently
       strcpy(inode[n].comment1,"This node is Unused");
       D.num_used_nodes[FIBER]--;
       break;
    case FIBER:
      {
 F_NODE *from = &old_inode[n].unode.fiber_node;
 OLD_F_NODE *to = &inode[n].unode.fiber_node;
 EQUAL (to,from,fiber_x);
 EQUAL (to,from,fiber_y);
 COPY (to,from,fiber_axon);
 EQUAL (to,from,f_prob);
 EQUAL (to,from,f_begin);
 EQUAL (to,from,f_end);
 EQUAL (to,from,f_seed);
 EQUAL (to,from,f_pop);
 EQUAL (to,from,f_targets);
 COPY (to,from,f_target_nums);
 COPY (to,from,f_conduction_time);
 COPY (to,from,f_terminals);
 COPY (to,from,f_synapse_type);
 COPY (to,from,f_synapse_strength);
 COPY (to,from,f_target_seed);
      }
      break;
    case celL:
       inode[n].node_type = UNUSED; // simbuild does this differently
       strcpy(inode[n].comment1,"This node is Unused");
       D.num_used_nodes[CELL]--;
       break;
    case CELL:
      {
 C_NODE *from = &old_inode[n].unode.cell_node;
 OLD_C_NODE *to = &inode[n].unode.cell_node;

 EQUAL (to,from,cell_x);
 EQUAL (to,from,cell_y);
 COPY (to,from,cell_axon);
 EQUAL (to,from,c_accomodation);
 EQUAL (to,from,c_k_conductance);
 EQUAL (to,from,c_mem_potential);
 EQUAL (to,from,c_resting_thresh);
 EQUAL (to,from,c_ap_k_delta);
 EQUAL (to,from,c_accom_param);
 EQUAL (to,from,c_pop);
 EQUAL (to,from,c_targets);
 COPY (to,from,c_target_nums);
 COPY (to,from,c_conduction_time);
 COPY (to,from,c_terminals);
  COPY (to,from,c_synapse_type);
 COPY (to,from,c_synapse_strength);
 COPY (to,from,c_target_seed);
 EQUAL (to,from,c_rebound_param);
 EQUAL (to,from,c_rebound_time_k);
 EQUAL (to,from,c_thresh_remove_ika);
 EQUAL (to,from,c_thresh_active_ika);
 EQUAL (to,from,c_max_conductance_ika);
 EQUAL (to,from,c_pro_deactivate_ika);
 EQUAL (to,from,c_pro_activate_ika);
 EQUAL (to,from,c_constant_ika);
 EQUAL (to,from,c_dc_injected);
      }
      break;
    case SYNAPSE:
      {
 S_NODE *from = &old_inode[n].unode.synapse_node;
 OLD_S_NODE *to = &inode[n].unode.synapse_node;
 memcpy (to, from, sizeof *to);
 COPY (to,from,synapse_name);
 COPY (to,from,s_eq_potential);
 COPY (to,from,s_time_constant);
      }
      break;
    case GLOBAL:
      {
 GLOBAL_NETWORK *from = &old_inode[n].unode.global_node;
 OLD_GLOBAL_NETWORK *to = &inode[n].unode.global_node;
 EQUAL (to,from,total_populations);
 EQUAL (to,from,total_fibers);
 EQUAL (to,from,total_cells);
 EQUAL (to,from,sim_length);
 EQUAL (to,from,k_equilibrium);
 EQUAL (to,from,max_conduction_time);
 EQUAL (to,from,num_synapse_types);
 EQUAL (to,from,max_targets);
 EQUAL (to,from,step_size);
 EQUAL (to,from,maximum_cells_per_pop);
 COPY (to,from,global_comment);
      }
      break;
    default:
      printf ("unknown node type %d (%08x)\n", node_type, node_type);
      DIE;
      break;
    }
    if (((int *)dbg_inode)[2] == 0) {printf ("%s line %d: n = %d, type %x\n", __FILE__, __LINE__, n, node_type); exit (0);}
  }
    if (((int *)dbg_inode)[2] == 0) {printf ("%s line %d: n = %d, type %x\n", __FILE__, __LINE__, n, node_type); exit (0);}
}

static void
fix_burster_cells (int version)
{
  int n;
  C_NODE *c;

  for (n = 1; n < 99; n++)  
  {
    if (D.inode[n].node_type == CELL || D.inode[n].node_type == celL)
    {
      c = &D.inode[n].unode.cell_node;
      if (c->c_k_conductance == 0) 
      {
        if (version == 1) 
        {
          c->c_accomodation = 10000; /* Time Constant for h */
          c->c_ap_k_delta = 2.8; /* NaP Conductance */
         }
         /*
               Rebound Time  Theta_h
               Thresh to remove inac. Sigma_h
               Thresh to act ika         Theta_m
               Max ika Cond.  Sigma_m
               ika Propor. Deact. K Reset Voltage @ h=0
               ika Propor. Act. K         Threshold Voltage
               ika Time Constant         Delta_h @ h=0
             */
         c->c_rebound_time_k    = -48;
         c->c_thresh_remove_ika   =   6;
         c->c_thresh_active_ika   = -40;
         c->c_max_conductance_ika =  -6;
         c->c_pro_deactivate_ika  = -47.359;
         c->c_pro_activate_ika    = -40;
         c->c_constant_ika        = -0.00078;
      }
    }
  }
}

static void
zero_rebound_for_noise ()
{
  int n;

  for (n = 1; n < 99; n++)
    if (D.inode[n].node_type == CELL || D.inode[n].node_type == celL)
      D.inode[n].unode.cell_node.c_rebound_param = 0;
}

// entries in the bdt table
static void
read_scr1 (FILE *f,int launch)
{
  int n, cf, p, m;

  while (fscanf (f, "%d %d %d %d ", &n, &cf, &p, &m) == 4)
    if (n < MAX_ENTRIES) {
      sp_bcf[n][launch] = cf;
      sp_bpn[n][launch] = p;
      sp_bm[n][launch] = m;
    }
}

// entries in the simviwer plot table
static void
read_scr2 (FILE *f, int launch)
{
  int n, p, m, v;
  char *linebuf = NULL;
  size_t buflen = 0;

  getline (&linebuf, &buflen, f);
  sscanf (linebuf,"%d %d %d %d %s", &n, &p, &m, &v, sp_hostname[launch]);
  sp_bpn2[n][launch] = p;
  sp_bm2[n][launch] = m;
  sp_bv2[n][launch] = v;
  while (getline (&linebuf, &buflen, f) > 0)
  {
    if (sscanf(linebuf,"%d %d %d %d", &n, &p, &m, &v) != 4)
      break;
    if (n >= MAX_ENTRIES)
      break;
    sp_bpn2[n][launch] = p;
    sp_bm2[n][launch] = m;
    sp_bv2[n][launch] = v;
  }
}

// analog selections
static void
read_scr3 (FILE *f, int launch)
{
  int matched;
  int i, p, r;
  float t, s;
  int p1c,p1s,p1e,p2c,p2s,p2e;
  float plus, minus;
  int smooth, sample, freq;

  p1c = p1s = p1e = p2c = p2s = p2e = 0;
  plus = minus = smooth = sample = freq = 0;

   // Due to bugs in earlier software, some .ols files have invalid values
   // that cause the simulator to fail for reasons the user will not discover.
   // Default to acceptable values in such cases.
  matched = fscanf (f, "%d%d%d%f%f%d%d%d%d%d%d%d%d%f%f%d", 
                  &i, &p, &r, &t, &s, &p1c, &p1s, &p1e, &p2c, &p2s, &p2e,
                  &smooth,&sample,&plus,&minus,&freq);
  if (matched == 5)  // pre-marker code version 
  {
    if (i == 0)
      i = 1;
    if (p == 0)
      p = 1;
    if (r == 0)
      r = 1;
  }
  if (matched == 11)  // pre smoothing params
  {
    sp_aid   [launch] = i;
    sp_apop  [launch] = p;
    sp_arate [launch] = r;
    sp_atk   [launch] = t;
    sp_ascale[launch] = s;
    sp_p1code[launch] = p1c;
    sp_p1start[launch] =p1s;
    sp_p1stop[launch] = p1e;
    sp_p2code[launch] = p2c;
    sp_p2start[launch] = p2s;
    sp_p2stop[launch] = p2e;
  }
  else  if (matched == 16)
  {
    sp_smooth[launch] = smooth;
    sp_sample[launch] = sample;
    sp_plus[launch] = plus;
    sp_minus[launch] = minus;
    sp_freq[launch] = freq;
  }
}

// init the sim launch window values
void init_sp(void)
{
   int x,y;

   for (y=0; y<MAX_LAUNCH; y++)
   {
      sp_inter[y]=100;
      sp_aid[y]=1;
      sp_apop[y]=1;
      sp_arate[y]=1;
      sp_atk[y]=1.0;
      sp_ascale[y]=1.0;
      sp_p1code[y]=0;
      sp_p1start[y]=0;
      sp_p1stop[y]=0;
      sp_p2code[y]=0;
      sp_p2start[y]=0;
      sp_p2stop[y]=0;
      sp_smooth[y]=0;
      sp_sample[y]=0;
      sp_plus[y]=0.0;
      sp_minus[y]=0.0;
      sp_freq[y]=0;
      memset(sp_hostname[y],0,sizeof(sp_hostname[0]));
      for (x=0; x<MAX_ENTRIES; x++)  // in original code, this was 100, but the
      {                              // arrays are MAX_ENTRIES, which is 1000, typo?
         sp_bcf[x][y]=UNUSED;
         sp_bpn[x][y]=0;
         sp_bm[x][y]=0;
         sp_bpn2[x][y]=0;
         sp_bm2[x][y]=0;
         sp_bv2[x][y]=0;
      }
   }
}

// Read in the launch window values
// Old .ols files will have only the first page.
// Simbuild saves all of them.
static void
load_scr (char *sndfile)
{
  char *scrfile = strdup (sndfile); 
  char *filetype = strrchr (scrfile, '.');
  FILE  *f;
  static char *linebuf;
  static size_t buflen;
  char match1[80]; 
  char match2[80]; 
  char match3[80]; 
  int pos = 1;
  int idx = 0;

  if (strcmp (filetype, ".snd") != 0) {
     free(scrfile);
    return;
  }
   // zero out destination vars
  init_sp();
  strcpy (filetype, ".ols");
  if ((f = fopen (scrfile, "r")) == 0) {
     free(scrfile);
    return;
  }
  snprintf(match1,sizeof(match1),"scr%d\n",pos);
  snprintf(match2,sizeof(match1),"scr%d\n",pos+1);
  snprintf(match3,sizeof(match1),"scr%d\n",pos+2);

  while (getline (&linebuf, &buflen, f) > 0) {
    if (strcmp (linebuf, match1) == 0)
    {
      read_scr1 (f,idx);
      ++pos;
    }
    else if (strcmp (linebuf, match2) == 0)
    {
      read_scr2 (f,idx);
      ++pos;
    }
    else if (strcmp (linebuf, match3) == 0)
    {
      read_scr3(f,idx);
      ++pos;
      snprintf(match1,sizeof(match1),"scr%d\n",pos); // next launch block
      snprintf(match2,sizeof(match1),"scr%d\n",pos+1);
      snprintf(match3,sizeof(match1),"scr%d\n",pos+2);
      ++idx;
    }
  }
  fclose (f);
  free(scrfile);
}


// Read the current snd file version of .snd file into the D array.
// Assumes D array has been initialized for a new file read.
static void read_fileio_file (char *file_name)
{
   FILE *f;

   if ((f = load_struct_open_simbuild (file_name)) != 0)
   {
      ::struct_info_fn = inode_struct_info;
      ::struct_members_fn = inode_struct_members;
      load_struct (f, (char*)"inode_global", &D, 1);
      fclose (f);
      SubVersion = D.file_subversion;
   }

  int node_type;
  int cells=0;
  int fibers=0;
  for (int slot = FIRST_INODE ; slot <= LAST_INODE; ++slot)
  {
     node_type = D.inode[slot].node_type;
     if (node_type == CELL)
        ++cells;
     else if (node_type == FIBER)
        ++fibers;
     else if (node_type == celL) // simbuild does this differently
     {
        memset (&D.inode[slot],0,sizeof(D.inode[0]));
        D.inode[slot].node_type  = UNUSED;
        strcpy(D.inode[slot].comment1,"This node is Unused");
     }
     else if (node_type == fibeR)
     {
        memset (&D.inode[slot],0,sizeof(D.inode[0]));
        D.inode[slot].node_type  = UNUSED;
        strcpy(D.inode[slot].comment1,"This node is Unused");
     }
  }
      // now adjust some global counters to reflect this
   D.num_free_nodes = MAX_INODES - (cells + fibers);
   D.inode[GLOBAL_INODE].unode.global_node.total_populations = cells+fibers;
   D.inode[GLOBAL_INODE].unode.global_node.total_fibers = fibers;
   D.inode[GLOBAL_INODE].unode.global_node.total_cells = cells;
   D.num_used_nodes[FIBER] = fibers;
   D.num_used_nodes[CELL] = cells;
}

static void
read_snd_file (char *file_name)
{
  int l_fp;
  int test_count;
  unsigned int swaptest = 1;

  if ( (l_fp=open(file_name,0) ) == -1)   {
    printf("could not open %s!   \n",file_name);
    printf("ignored!   \n");
  }
  else  {
    int sndfile_format_version = 0;
    read(l_fp,&D.num_free_nodes,sizeof(D.num_free_nodes));
    if ((unsigned) D.num_free_nodes == 0xffffffff) {
      read (l_fp, &sndfile_format_version, sizeof sndfile_format_version);
      if (*(char *) &swaptest)
 SWAPLONG (sndfile_format_version);
      read (l_fp, &D.num_free_nodes, sizeof D.num_free_nodes);
    }
    read(l_fp,D.num_used_nodes,sizeof(D.num_used_nodes));
    if (sndfile_format_version == 0) {
      OLD_I_NODE *old_inode;
      OLD_I_NODE *old_inode2;
      (old_inode = (OLD_I_NODE*) malloc (sizeof (OLD_I_NODE) * OLD_MAX_INODES)) || DIE;
      dbg_inode = old_inode;
      read (l_fp, old_inode, sizeof (OLD_I_NODE) * OLD_MAX_INODES) == sizeof (OLD_I_NODE) * OLD_MAX_INODES || DIE;
      long int r = read (l_fp, old_inode, sizeof (OLD_I_NODE) * OLD_MAX_INODES);
      printf("%zu\n", sizeof (OLD_I_NODE) * OLD_MAX_INODES);
      if (r != sizeof (OLD_I_NODE) * OLD_MAX_INODES)
         printf("Wrong size\n");
      copy_old_inode (D.inode, old_inode);
      (old_inode2 = (OLD_I_NODE*) malloc (sizeof (OLD_I_NODE) * OLD_MAX_INODES)) || DIE;
      copy_new_inode (old_inode2, D.inode);
      if (memcmp (old_inode, old_inode2, sizeof (OLD_I_NODE) * OLD_MAX_INODES) != 0) {
 unsigned int n;
 for (n = 0; n < sizeof (OLD_I_NODE) * OLD_MAX_INODES / 4; n++) {
   printf ("%7d: old %08x new %08x", n, ((int *)old_inode)[n], ((int *)old_inode2)[n]);
   if (((int *)old_inode)[n] != ((int *)old_inode2)[n])
     printf (" ****");
   printf ("\n");
 }
 printf ("inodes are different\n");
 exit (1);
      }
      free (old_inode);
    }
    else if (sndfile_format_version >= 1 && sndfile_format_version <= 4) {
      I_NODE_2 *inode_2;
      (inode_2 = (I_NODE_2*) malloc (sizeof (I_NODE_2) * OLD_MAX_INODES)) || DIE;
      read (l_fp, inode_2, sizeof (I_NODE_2) * OLD_MAX_INODES) == sizeof (I_NODE_2) * OLD_MAX_INODES || DIE;
      copy_inode_2 (D.inode, inode_2);
    }
    else {
      close (l_fp);
      fprintf (stderr, "bad older .snd file format\n");
      return;
    }
    if (*(char *) &swaptest)
      swap_inode_bytes (1);
    if (sndfile_format_version <= 3)
      fix_burster_cells (sndfile_format_version);
    if (sndfile_format_version < 3)
      zero_rebound_for_noise ();

    //    printf ("total pops: %d\n", D.inode[GLOBAL_INODE].unode.global_node.total_populations); exit (0);
    test_count=read(l_fp, D.doc_file, 1024 );
    close(l_fp);
    if (test_count != 1024)
      D.doc_file[0]=0;   /* set doc string to null if old file type */
    /* this flag lurks at the end of the string, a hack from the old times */
    D.presynaptic_flag = D.inode[GLOBAL_INODE].unode.global_node.global_comment[99];
  }
}


// Some snd files have a c_injected_expression that the muParser lib
// evaulates. Some of the expressions contain if statements, such as:
// if(V<10,-1.75*(V-10),0)
// The current muParser (2.2.3) no longer recognizes this. Instead,
// the ternary op is used  (expr) ? (val if true) : (val if false)
// This fixes the text.
static void fix_muparse()
{
   int node;
   C_NODE *cn;
   QString str;

   for (node = FIRST_INODE; node <= LAST_INODE; node++)
   {
      if (D.inode[node].node_type == CELL)
      {
         cn = &D.inode[node].unode.cell_node;
         if (cn->c_injected_expression)
         {
            str = cn->c_injected_expression;
            if (str.indexOf("if",0,Qt::CaseInsensitive) != -1)
            {
               str = str.remove("if",Qt::CaseInsensitive);
               QStringList args = str.split(",");
               if (args.size() != 3)
               {
                  QString badnews="Can't parse injected expression, ";
                  badnews += cn->c_injected_expression;
                  badnews += "\nThe simulation result is likely to be incorrect.";
                  alert(badnews.toLatin1().data());
                     // best we can do is not crash later in xp_eval
                  free(cn->c_injected_expression);
                  cn->c_injected_expression = nullptr;
               }
               else
               {
                  QString new_exp = args[0] + " ? " + args[1] + " : " + args[2];
                  QByteArray array = new_exp.toLocal8Bit();
                  free(cn->c_injected_expression);
                  cn->c_injected_expression = strdup(array.data());
                       // where does this get freed?
               }
            }
         }
      }
   }

}

// simbuild tracks synapse types differently. If this is a V5 file, init
// our bookeeping info.
void V6SynapseAdjustments()
{
   int par_idx=0;
   if (Version >= FILEIO_FORMAT_VERSION6)
      return;

   S_NODE *s = &D.inode[SYNAPSE_INODE].unode.synapse_node;
   int node, val;
   for (node = 1; node < TABLE_LEN; ++node) // index 0 unused for synapse node
   {
      if (s->s_time_constant[node] != 0.0)
      {
         if (!D.presynaptic_flag)
            s->syn_type[node] = SYN_NORM;
         else
         {
            val = node % 3;  // this assumes rule-of-three, par/pre/post
            if (val == 1)
            {
              s->syn_type[node] = SYN_NORM;
              par_idx = node;
            }
            else if (val == 2)
            {
              s->syn_type[node] = SYN_PRE;
              s->parent[node] = par_idx;
            }
            else
            {
              s->syn_type[node] = SYN_POST;
              s->parent[node] = par_idx;
            }
         }
      }
      else
         break; // in V5, no missing nodes, so bail on last one
   }
}

// Testing revealed that the c_targets and f_targets values were often
// much larger than the actual number of targets.
// There were also several cases where the number of targets value was smaller than
// non-zero targets in the various target arrays.
// Adjust the target numbers to be accurate, and zero elements that should be
// zero. 
// This perhaps is due to bugs in older sw that were fixed later?
// Unfortunately, there are some v6 files in the wild that perpetrate this problem,
// so we have to check every file we load to see if it needs a fix.
// Later even: discovered that the ?_target_nums must be the index of the last 
// non-zero item in the array that is <= the original target_nums. That is:
// original target_num is 5 and we see this:  1 2 0 4 9
// then previous code here counted 4, but 5 is the correct answer. Fixed.
void fixTargetCounts()
{
   int claim;
   int actual;
   int maxidx;
   int entry, fix_col;
   C_NODE *cn;
   F_NODE *fn;
   AXON *axon;

   for (int node = FIRST_INODE; node <= LAST_INODE; ++node) 
   {
      if (D.inode[node].node_type == CELL)
      {
         cn = &D.inode[node].unode.cell_node;
         claim = cn->c_targets;
         actual = maxidx = 0;
         for (entry = 0; entry < TABLE_LEN; ++entry)
            if (cn->c_target_nums[entry] != 0)
            {
               ++actual;
               maxidx = entry+1;
            }
         if (claim != actual)
         {
            if (claim < actual)  // unused but non-zero entries, zero them
            {                    // assume the first N entries are valid
               for (fix_col = claim; fix_col < TABLE_LEN; ++fix_col)
               {
                  cn->c_target_nums[fix_col] = 0;
                  cn->c_min_conduct_time[fix_col] = 0;
                  cn->c_conduction_time[fix_col] = 0;
                  cn->c_terminals[fix_col] = 0;
                  cn->c_synapse_type[fix_col] = 0;
                  cn->c_synapse_strength[fix_col] = 0;
                  cn->c_target_seed[fix_col] = 0;
               }
            }
            else
            {
               cn->c_targets = maxidx;
            }
         }
            // some axons have cell coords even though num_coords is 0.
            // probably that was how they were 'deleted'. It is confusing
            // during debugging, so zero out the entries with no coordinates.
         axon = &cn->cell_axon[0];
         for (entry = 0; entry < TABLE_LEN; ++entry, ++axon)
            if (axon->num_coords == 0)
               memset(axon,0,sizeof(AXON));
      }
      else if (D.inode[node].node_type == FIBER)
      {
         fn = &D.inode[node].unode.fiber_node;
         claim = fn->f_targets;
         actual = maxidx = 0 ;
         for (entry = 0; entry < TABLE_LEN; ++entry)
            if (fn->f_target_nums[entry] != 0)
            {
               ++actual;
               maxidx = entry+1;
            }
         if (claim != actual)
         {
            if (claim < actual)  // unused but non-zero entries, zero them
            {                    // assume the first N entries are valid
               for (fix_col = claim; fix_col < TABLE_LEN; ++fix_col)
               {
                  fn->f_target_nums[fix_col] = 0;
                  fn->f_min_conduct_time[fix_col] = 0;
                  fn->f_conduction_time[fix_col] = 0;
                  fn->f_terminals[fix_col] = 0;
                  fn->f_synapse_type[fix_col] = 0;
                  fn->f_synapse_strength[fix_col] = 0;
                  fn->f_target_seed[fix_col] = 0;
               }
            }
            else
            {
               fn->f_targets = maxidx;
            }
         }
         axon = &fn->fiber_axon[0];  // zero unused coords
         for (entry = 0; entry < TABLE_LEN; ++entry, ++axon)
            if (axon->num_coords == 0)
               if (axon->x_coord[0] != 0)
                  memset(axon,0,sizeof(AXON));
      }
   }
}

void V6GlobalAdjustments()
{
    // V5 and older V6 files do not have this field. 
    // Keep seconds explicitly now. The # of steps changes
    // depending on the step size, .01, .05, or custom.
   if (Version <= FILEIO_FORMAT_VERSION6 && D.file_subversion == 0) // older v6 
      D.inode[GLOBAL_INODE].unode.global_node.sim_length_seconds = 
         D.inode[GLOBAL_INODE].unode.global_node.sim_length/1000.0 *
         D.inode[GLOBAL_INODE].unode.global_node.step_size;
}

void V6CellAdjustments()
{
   if (Version >= FILEIO_FORMAT_VERSION6 && D.file_subversion == 0) // early v6 
   {  // older v6 files had pop_subtype as 0, make it explicitly CELL
      for (int node = FIRST_INODE; node <= LAST_INODE; ++node)
         if (D.inode[node].node_type == CELL && D.inode[node].unode.cell_node.pop_subtype == UNUSED)
            D.inode[node].unode.cell_node.pop_subtype = CELL;
   }
   else if (Version < FILEIO_FORMAT_VERSION6)
   {
      for (int node = 0; node < MAX_INODES; ++node)  // pre-subtype file
      {
         if (D.inode[node].node_type == CELL) 
         {
            C_NODE *cn = &D.inode[node].unode.cell_node;
            QString comment(D.inode[node].comment1);
            if (cn->c_k_conductance == 0) 
               cn->pop_subtype = BURSTER_POP;
            else if (cn->c_mem_potential == 0) 
               cn->pop_subtype = PSR_POP;
            else if (comment.contains("PHRENIC",Qt::CaseInsensitive))
               cn->pop_subtype = PHRENIC_POP;
            else if (comment.contains("LUMBAR",Qt::CaseInsensitive))
               cn->pop_subtype = LUMBAR_POP;
            else if (comment.contains("ILM",Qt::CaseInsensitive) ||
                        comment.contains("PCA",Qt::CaseInsensitive))
               cn->pop_subtype = INSP_LAR_POP;
            else if (comment.contains("ELM",Qt::CaseInsensitive) ||
                        comment.contains("TA",Qt::CaseInsensitive))
               cn->pop_subtype = EXP_LAR_POP;
            else
               cn->pop_subtype = CELL;
          }
      }
   }
}

// Added in a pop_subtype for fiber. It will be zero
// in .snd files that do not have it. This means pop subtype
// is normal fiber. 
void V6FiberAdjustments()
{
      // older v6  or earlier
   if (Version < FILEIO_FORMAT_VERSION6  
        || (Version >= FILEIO_FORMAT_VERSION6 && D.file_subversion == 0))
   {
      for (int node = FIRST_INODE; node <= LAST_INODE; ++node) 
      {
         if (D.inode[node].node_type == FIBER          // older v6 file signature
             && D.inode[node].unode.fiber_node.pop_subtype == UNUSED)
         {
            D.inode[node].unode.fiber_node.pop_subtype = FIBER;
            D.inode[node].unode.fiber_node.f_begin =
               D.inode[node].unode.fiber_node.f_begin * D.inode[GLOBAL_INODE].unode.global_node.step_size;
            D.inode[node].unode.fiber_node.f_end =
               D.inode[node].unode.fiber_node.f_end * D.inode[GLOBAL_INODE].unode.global_node.step_size;
         }
      }
   }
}

int findTargetIdx(int target_num)
{
   int val = -1;
   for (int node = FIRST_INODE; node <= LAST_INODE; ++node) 
      if (D.inode[node].node_type == CELL && D.inode[node].node_number == target_num)
      {
         val = node;
         break;
      }
   return val;
}


void
write_pop_param_csv (void)
{
  char *cfile_name;
  if (asprintf(&cfile_name, "pop_param_%02d.csv",currModel) == -1) exit (1);
  FILE *f = fopen (cfile_name, "w");
  free(cfile_name);
  fprintf (f, "Population number, Population name, Size, Resting theshold (mV), THO variability (mV), Membrane time constant, Post-spike increase in K conductance, Post-spike K conductance time constant (ms),Adaptation threshold increase, Adaptation (ms), Noise amplitude, DC (mV)\n");
  fprintf (f, ",,N,THO,,TMEM,B,TGK,C,TTH\n");
  fprintf (f, "node_number,comment1,c_pop,c_resting_thresh,c_resting_thresh_sd,c_mem_potential,c_ap_k_delta,c_k_conductance,c_accom_param,c_accomodation,c_rebound_param,c_dc_injected\n");
    for (int i = 0; i < MAX_INODES; i++) {
    if (D.inode[i].node_type == CELL) {
      fprintf (f, "%d,%s,%d,%g,%g,%g,%g,%g,%g,%g,%g,%g\n",
               D.inode[i].node_number,
               D.inode[i].comment1,
               D.inode[i].unode.cell_node.c_pop,
               D.inode[i].unode.cell_node.c_resting_thresh,
               D.inode[i].unode.cell_node.c_resting_thresh_sd,
               D.inode[i].unode.cell_node.c_mem_potential,
               D.inode[i].unode.cell_node.c_ap_k_delta,
               D.inode[i].unode.cell_node.c_k_conductance,
               D.inode[i].unode.cell_node.c_accom_param,
               D.inode[i].unode.cell_node.c_accomodation,
               D.inode[i].unode.cell_node.c_rebound_param,
               D.inode[i].unode.cell_node.c_dc_injected);
    }
  }
  fclose (f);
}


// This remembers the on-disk version of the file. If we run simrun without
// writing out the V6 or later .snd file, simrun will fail because it cannot read
// earlier .snd files.
void Load_snd(char* file_name)
{
  const int sz = 4;
  char buf[sz];
  FILE *f;

  if ((f = fopen (file_name, "rb")) == 0
      || fread (buf, 1, sz, f) != sz) {
    char *msg;
    if (asprintf (&msg, "COULD NOT OPEN \"%s\": %s", file_name, strerror (errno)) == -1) exit (1);
    alert (msg);
    free (msg);
    return;
  }

  load_scr(file_name);
  if (strncmp (buf, "file", 4) == 0) // V5 or later
  {
    rewind(f);                       // get the real version
    char *line = nullptr;
    size_t len=0;
    ssize_t res = getline(&line,&len,f);
    if (res && line)
    {
       int gotit = sscanf(line,"%*s %d\n",&Version);
       free(line);
       if (!gotit)
          fprintf(stderr,"Warning: the .snd file does have have a valid header.\n");
    }
    fclose (f);
    read_fileio_file (file_name);
  }
  else
  {
    fclose (f);
    read_snd_file (file_name);        // earlier versions
    Version = FILEIO_FORMAT_VERSION5; // Treat earlier versions as V5, the in-memory
                                      // image should be V5 at this point.
  }
  fix_muparse();
  V6GlobalAdjustments();
  V6SynapseAdjustments();
  V6CellAdjustments();
  V6FiberAdjustments();
  fixTargetCounts();
  write_pop_param_csv();
  SubVersion = FILEIO_SUBVERSION_CURRENT;
}


static void
save_scr (char *sndfile)
{
  char *scrfile = strdup (sndfile); // where is this freed?
  char *filetype = strrchr (scrfile, '.');

  FILE  *f;
  int n;
  char header[80]; 
  int pos = 1;

  strcpy (filetype, ".ols");
  if ((f = fopen (scrfile, "w")) == 0) {
     free(scrfile);
    return;
  }
  for (int launch = 0; launch < MAX_LAUNCH; ++launch)
  {
     sprintf(header,"scr%d\n",pos);
     fprintf (f,"%s",header);
     for (n = 0; n < MAX_ENTRIES; n++)
        fprintf (f, "%d %d %d %d\n", n, sp_bcf[n][launch], sp_bpn[n][launch], sp_bm[n][launch]);
     putc ('\n', f);
     ++pos;

     sprintf(header,"scr%d\n",pos);
     fprintf (f,"%s",header);
     fprintf (f, "%d %d %d %d %s\n", 0, sp_bpn2[0][launch], sp_bm2[0][launch], 
                                     sp_bv2[0][launch], sp_hostname[launch]);
     for (n = 1; n < MAX_ENTRIES; n++)
        fprintf (f, "%d %d %d %d\n", n, sp_bpn2[n][launch], sp_bm2[n][launch], sp_bv2[n][launch]);
     putc ('\n', f);
     ++pos;

     sprintf(header,"scr%d\n",pos);
     fprintf (f,"%s",header);
     fprintf (f, "%d %d %d %f %f %d %d %d %d %d %d %d %d %f %f %d\n", 
           sp_aid[launch], sp_apop[launch], sp_arate[launch], 
           sp_atk[launch], sp_ascale[launch], 
           sp_p1code[launch], sp_p1start[launch], sp_p1stop[launch],
           sp_p2code[launch], sp_p2start[launch], sp_p2stop[launch],
           sp_smooth[launch], sp_sample[launch], sp_plus[launch], sp_minus[launch],
           sp_freq[launch]);
     putc ('\n', f);
     ++pos;
  }
  fclose (f);
  free(scrfile);
}



void Save_snd(char* file_name)
{
   int len;
   char inode_global_name[] ="inode_global";

   if ((len = strlen (file_name)) < 4 || strcmp (file_name + (len - 4), ".snd") != 0)
   {
      char *tmp;
      if (asprintf (&tmp, "%s.snd", file_name) == -1)
         exit (1);
      file_name = tmp;
   }

   save_scr (file_name);

   FILE *f;
   save_all = 1;
   D.inode[GLOBAL_INODE].node_type = GLOBAL;
   D.inode[SYNAPSE_INODE].node_type = SYNAPSE;
   D.file_subversion = SubVersion;
   ::struct_info_fn = inode_struct_info;
   ::struct_members_fn = inode_struct_members;

   f = save_struct_open (file_name);
   if (f)
   {
      save_struct (f, inode_global_name, &D);
      fclose (f);
   }
   else
     printf("Could not open %s for writing\n",file_name);

}

void Save_subsys(char* file_name, inode_global* sub)
{
   char inode_global_name[] ="inode_global";

   FILE *f;
   save_all = 1;

   ::struct_info_fn = inode_struct_info;
   ::struct_members_fn = inode_struct_members;
   f = save_struct_open (file_name);
   if (f)
   {
      save_struct (f, inode_global_name, sub);
      fclose (f);
   }
   else
     printf("Could not open %s for writing\n",file_name);
}


// this is the file the simulator engine needs to run the sim
void Save_sim (char *savename, char* sndname, int index)
{
  int n;
  GLOBAL_NETWORK *gn = &D.inode[GLOBAL_INODE].unode.global_node;

  save_scr (savename);
  memset(&S,0,sizeof(S));  // values passed in here

  S.file_subversion   = FILEIO_SUBVERSION_CURRENT;
  S.net.fiberpop_count = gn->total_fibers;
  S.net.cellpop_count  = gn->total_cells;
  S.step_count         = gn->sim_length;
  S.Ek                 = gn->k_equilibrium + S.Vm0;
  S.net.syntype_count  = gn->num_synapse_types;
  S.step               = gn->step_size;
  S.ispresynaptic      = D.presynaptic_flag;
  S.Gm0                = 1;
  S.lmmfr              = gn->ilm_elm_fr;
  S.spawn_number       = currModel;

  S.snd_file_name = strdup(sndname);

  S.p1code = sp_p1code[index];
  S.p1start = sp_p1start[index];
  S.p1stop = sp_p1stop[index];
  S.p2code = sp_p2code[index];
  S.p2start = sp_p2start[index];
  S.p2stop = sp_p2stop[index];
  S.ie_sample = sp_sample[index];
  S.ie_smooth = sp_smooth[index];
  S.ie_plus = sp_plus[index];
  S.ie_minus = sp_minus[index];
  S.ie_freq = sp_freq[index];

  S.net.syntype = (SynType*) malloc(S.net.syntype_count * sizeof *(S.net.syntype));
  for (n = 0; n < S.net.syntype_count; n++) 
  {
      // can be holes if deleted synapses
    if (D.inode[SYNAPSE_INODE].unode.synapse_node.syn_type[n+1] == SYN_NOT_USED)
    {
       S.net.syntype[n].EQ = 0;
       S.net.syntype[n].DCS = 0;
       S.net.syntype[n].PARENT = 0;
       S.net.syntype[n].lrnStrDelta = 0;
       S.net.syntype[n].lrnStrMax = 0;
       S.net.syntype[n].lrnWindow = 0;
       S.net.syntype[n].SYN_TYPE = SYN_NORM; // needs the type, even if not used
       continue;
    }
    S.net.syntype[n].EQ = D.inode[SYNAPSE_INODE].unode.synapse_node.s_eq_potential[n+1];
    S.net.syntype[n].DCS = exp (-S.step / D.inode[SYNAPSE_INODE].unode.synapse_node.s_time_constant[n+1]);
    S.net.syntype[n].SYN_TYPE = D.inode[SYNAPSE_INODE].unode.synapse_node.syn_type[n+1];
    S.net.syntype[n].PARENT = D.inode[SYNAPSE_INODE].unode.synapse_node.parent[n+1];
    S.net.syntype[n].lrnWindow = D.inode[SYNAPSE_INODE].unode.synapse_node.lrn_window[n+1];
    S.net.syntype[n].lrnStrMax = D.inode[SYNAPSE_INODE].unode.synapse_node.lrn_maxstr[n+1];
    S.net.syntype[n].lrnStrDelta = D.inode[SYNAPSE_INODE].unode.synapse_node.lrn_delta[n+1];
  }

   S.net.fiberpop = (FiberPop*) calloc(S.net.fiberpop_count,sizeof *(S.net.fiberpop));
   S.net.cellpop = (CellPop*) calloc(S.net.cellpop_count,sizeof *(S.net.cellpop));
   for (n = FIRST_INODE; n <= LAST_INODE; n++)
   {
      int nn = D.inode[n].node_number - 1;
      if (D.inode[n].node_type == FIBER)
      {
         F_NODE *fn = &D.inode[n].unode.fiber_node;
         if (nn >= 0 && nn < S.net.fiberpop_count)
         {
            int tn;
            FiberPop *fp = &S.net.fiberpop[nn];
            fp->probability     = fn->f_prob;
            fp->start           = fn->f_begin;
            fp->stop            =  fn->f_end < 0 ? INT_MAX/(1000.0/S.step) : fn->f_end;
            fp->infsed          = fn->f_seed;
            fp->fiber_count     = fn->f_pop;
            fp->targetpop_count = fn->f_targets;
            if (fn->pop_subtype == 0)
               cout << "Huh?" << endl;
            fp->pop_subtype     = fn->pop_subtype;
            fp->freq_type       = fn->freq_type;
            fp->frequency       = fn->frequency;
            fp->offset          = fn->offset;
            if (strlen(fn->afferent_file))
               fp->afferent_file_name = strdup(fn->afferent_file);
            else
               fp->afferent_file_name = nullptr;
            memcpy(fp->aff_val, fn->aff_val, sizeof(fp->aff_val));
            memcpy(fp->aff_prob, fn->aff_prob, sizeof(fp->aff_prob));
            fp->num_aff = fn->num_aff;
            fp->slope_scale = fn->slope_scale;

            fp->targetpop = (TargetPop *) malloc(fp->targetpop_count * sizeof *(fp->targetpop));
            int idx=0;
            int expected=fn->f_targets;
            for (tn = 0; tn < TABLE_LEN && idx < expected; ++tn)
            {
              TargetPop *tp = &fp->targetpop[idx];
              tp->IRCP  = fn->f_target_nums[tn];
              tp->MCT   = fn->f_min_conduct_time[tn];
              tp->NCT   = fn->f_conduction_time[tn];
              tp->NT    = fn->f_terminals[tn];
              tp->TYPE  = fn->f_synapse_type[tn];
              tp->STR   = fn->f_synapse_strength[tn];
              tp->INSED = fn->f_target_seed[tn];
              if (D.inode[SYNAPSE_INODE].unode.synapse_node.syn_type[tp->TYPE]== SYN_LEARN)
                 fp->haveLearn = 1;
              ++idx;
           }
           if (idx != fn->f_targets)
           {
              cout << "Error in Save_sim in # of fiber targets claimed: " << fn->f_targets << " actual " << idx << endl;
              if (idx > fn->f_targets)
                 cout << "You have a malloc problem!" << endl;
           }
        }
      }
      else if (D.inode[n].node_type == CELL)
      {
         C_NODE *cn = &D.inode[n].unode.cell_node;
         if (nn >= 0 && nn < S.net.cellpop_count)
         {
            int tn;
            CellPop *cp = &S.net.cellpop[nn];
            if (cn->pop_subtype == BURSTER_POP)
            {
               cp->DCG = -1; // old way of telling simrun it is a burster, aka hybrid IF cell
               cp->taubar_h = cn->c_accomodation;
               cp->DCTH = cn->c_accomodation;
            }
            else
            {
               cp->TGK = cn->c_k_conductance;
               cp->DCG = exp (-S.step / cn->c_k_conductance);
               cp->DCTH = cn->c_accomodation ? exp (-S.step / cn->c_accomodation) : -1;
            }
            if (cn->pop_subtype == PSR_POP)
            {
               cp->TMEM = -1;  // old way of telling simrun it is a PSR
               cp->R0 = 0;
            }
            else
            {
               cp->TMEM = cn->c_mem_potential;
               cp->R0 = -.5 * S.step / (cn->c_mem_potential * S.Gm0);
            }
            cp->Th0 = cn->c_resting_thresh + S.Vm0;
            cp->Th0_sd = cn->c_resting_thresh_sd;
            cp->B = cn->c_ap_k_delta * S.Gm0;
            cp->g_NaP_h = cn->c_k_conductance ? 0 : cn->c_ap_k_delta;

            cp->MGC             = cn->c_accom_param;
            cp->cell_count      = cn->c_pop;
            cp->targetpop_count = cn->c_targets;
            cp->noise_amp       = cn->c_rebound_param;
            cp->theta_h         = cn->c_rebound_time_k;
            cp->sigma_h         = cn->c_thresh_remove_ika;
            cp->theta_m         = cn->c_thresh_active_ika;
            cp->sigma_m         = cn->c_max_conductance_ika;
            cp->Vreset          = cn->c_pro_deactivate_ika;
            cp->Vthresh         = cn->c_pro_activate_ika;
            cp->delta_h         = cn->c_constant_ika;
            cp->GE0             = cn->c_dc_injected + (cp->DCG == -1 ? 0 : S.Gm0 * S.Vm0);

             // A bug in V6 files where an empty text control for
             // c_injected_expression was saved as a string with 0 bytes, which
             // is not the same as nullptr, which is what simrun expects. This
             // corrects that for files written before this was fixed.
            if (cn->c_injected_expression && strlen(cn->c_injected_expression) == 0)
               cp->ic_expression = nullptr;
            else
               cp->ic_expression = cn->c_injected_expression;
            cp->pop_subtype = cn->pop_subtype;
            cp->targetpop = (TargetPop*) malloc(cn->c_targets * sizeof *(cp->targetpop));
            int idx=0;
            int expected = cn->c_targets;
            for (tn = 0; tn < TABLE_LEN && idx < expected; ++tn)
            {
               TargetPop *tp = &cp->targetpop[idx];
               tp->IRCP  = cn->c_target_nums[tn];
               tp->MCT   = cn->c_min_conduct_time[tn];
               tp->NCT   = cn->c_conduction_time[tn];
               tp->NT    = cn->c_terminals[tn];
               tp->TYPE  = cn->c_synapse_type[tn];
               tp->STR   = cn->c_synapse_strength[tn];
               tp->INSED = cn->c_target_seed[tn];
               if (D.inode[SYNAPSE_INODE].unode.synapse_node.syn_type[tp->TYPE]== SYN_LEARN)
                 cp->haveLearn = 1;
               ++idx;
            }
            if (idx != cn->c_targets)  // this should "never" happen in V6 and later files
            {
               cout << "Error in Save_sim in # of cell targets claimed:" << cn->c_targets << " actual " << idx << endl;
               if (idx > cn->c_targets)
                  cout << "You have a malloc problem!" << endl;
            }
         }
      }
   }

  FILE *f;
  save_all = 0, save_state = 0, save_init = 1;
  ::struct_info_fn = simulator_struct_info;
  ::struct_members_fn = simulator_struct_members;

  f = save_struct_open (savename);
  char fname[]="simulator_global";
  if (f)
  {
     save_struct (f, fname, &S);
     fclose (f);
  }
  else
     printf("Save_sim::Could not open %s for writing\n",fname);

  for (n = 0; n < S.net.cellpop_count; n++)
    free (S.net.cellpop[n].targetpop);
  for (n = 0; n < S.net.fiberpop_count; n++)
    free (S.net.fiberpop[n].targetpop);
  free (S.snd_file_name);
  free (S.net.cellpop);
  free (S.net.fiberpop);
  free (S.net.syntype);
}


void global_loader()
{
  FILE *g_fp;
  char holder[50];
  int max_tmp;

  if ((g_fp = fopen("globals.sned","r")) != 0) {
    if (fscanf(g_fp,"%s %d %s %f %s %d %s %f",holder,&(D.inode[GLOBAL_INODE].unode.global_node.sim_length),
               holder, &(D.inode[GLOBAL_INODE].unode.global_node.k_equilibrium),
               holder, &max_tmp,
               holder, &(D.inode[GLOBAL_INODE].unode.global_node.step_size)))
    {
       D.inode[GLOBAL_INODE].unode.global_node.ilm_elm_fr = 40.0;
       D.inode[GLOBAL_INODE].unode.global_node.sim_length_seconds = 
               D.inode[GLOBAL_INODE].unode.global_node.sim_length/1000.0 *
               D.inode[GLOBAL_INODE].unode.global_node.step_size;
    }
    fclose(g_fp);
  }
  else {
    D.inode[GLOBAL_INODE].unode.global_node.sim_length = 24000;
    D.inode[GLOBAL_INODE].unode.global_node.k_equilibrium = -10.0;
    max_tmp = 5;
    D.inode[GLOBAL_INODE].unode.global_node.step_size = 0.5;
    D.inode[GLOBAL_INODE].unode.global_node.ilm_elm_fr = 40.0;
    D.inode[GLOBAL_INODE].unode.global_node.sim_length_seconds = 
               D.inode[GLOBAL_INODE].unode.global_node.sim_length/1000.0 *
               D.inode[GLOBAL_INODE].unode.global_node.step_size;
  }
  D.inode[GLOBAL_INODE].unode.global_node.max_conduction_time=(char)(max_tmp);
  D.inode[GLOBAL_INODE].comment2[0] = 0;
  D.inode[GLOBAL_INODE].unode.global_node.global_comment[0] = 0;
  D.inode[GLOBAL_INODE].unode.global_node.phrenic_equation[0] = 0;
  D.inode[GLOBAL_INODE].unode.global_node.lumbar_equation[0] = 0;
}


// this assumes global_loader called before this
void synapse_loader()
{
   FILE *s_fp;
   char holder[50];
   int loop;
   int num_synapses;

   if ((s_fp = fopen("def_syn.sned","r")) != 0)
     fscanf(s_fp,"%s %d",holder,&num_synapses);
   else
     num_synapses = 4;

   /* now set the global to number of synapses  */
   D.inode[GLOBAL_INODE].unode.global_node.num_synapse_types=(char)(num_synapses);

   if (s_fp) {
     for (loop=1;loop<=num_synapses;loop++)
     {
       fscanf(s_fp,"%s %f %f",
                       D.inode[SYNAPSE_INODE].unode.synapse_node.synapse_name[loop],
                       &(D.inode[SYNAPSE_INODE].unode.synapse_node.s_eq_potential[loop]),
                       &(D.inode[SYNAPSE_INODE].unode.synapse_node.s_time_constant[loop])
                       );
     }
     fclose(s_fp);
   }
   else {
     strcpy (D.inode[SYNAPSE_INODE].unode.synapse_node.synapse_name[1], "Excitatory_1");
     D.inode[SYNAPSE_INODE].unode.synapse_node.s_eq_potential[1] = 115.0;
     D.inode[SYNAPSE_INODE].unode.synapse_node.s_time_constant[1] = 1.5;
     strcpy (D.inode[SYNAPSE_INODE].unode.synapse_node.synapse_name[2], "Inhibitory_1");
     D.inode[SYNAPSE_INODE].unode.synapse_node.s_eq_potential[2] = -25.0;
     D.inode[SYNAPSE_INODE].unode.synapse_node.s_time_constant[2] = 1.5;
     strcpy (D.inode[SYNAPSE_INODE].unode.synapse_node.synapse_name[3], "Excitatory_2");
     D.inode[SYNAPSE_INODE].unode.synapse_node.s_eq_potential[3] = 115.0;
     D.inode[SYNAPSE_INODE].unode.synapse_node.s_time_constant[3] = 2.0;
     strcpy (D.inode[SYNAPSE_INODE].unode.synapse_node.synapse_name[4], "Inhibitory_2");
     D.inode[SYNAPSE_INODE].unode.synapse_node.s_eq_potential[4] = -25.0;
     D.inode[SYNAPSE_INODE].unode.synapse_node.s_time_constant[4] = 2.0;
   }
   V6SynapseAdjustments();
}


void fiber_loader()
{
  FILE *s_fp;
  char holder[50];
  int matches;

  if ((s_fp = fopen("def_fiber.sned","r")) != 0)
  {
    matches = fscanf(s_fp,"%s %f %s %d %s %d %s %d %s %d %s %d %s %d %s %f %s %f",
               holder, &(def_fiber.unode.fiber_node.f_prob),
               holder, &(def_fiber.unode.fiber_node.f_begin),
               holder, &(def_fiber.unode.fiber_node.f_end),
               holder, &(def_fiber.unode.fiber_node.f_seed),
               holder, &(def_fiber.unode.fiber_node.f_pop),
               holder, &(def_fiber.unode.fiber_node.pop_subtype),
               holder, &(def_fiber.unode.fiber_node.freq_type),
               holder, &(def_fiber.unode.fiber_node.frequency),
               holder, &(def_fiber.unode.fiber_node.fuzzy_range));
    fclose(s_fp);
    if (matches < 11)
    {
       def_fiber.unode.fiber_node.pop_subtype = FIBER;
       def_fiber.unode.fiber_node.freq_type = STIM_FIXED;
       def_fiber.unode.fiber_node.frequency = 1.0;
       def_fiber.unode.fiber_node.fuzzy_range = 1.0;
    }
  }
  else {
    def_fiber.unode.fiber_node.f_prob = 0.05;
    def_fiber.unode.fiber_node.f_begin = 0;
    def_fiber.unode.fiber_node.f_end = -1;
    def_fiber.unode.fiber_node.f_seed = 12345;
    def_fiber.unode.fiber_node.f_pop = 100;
    def_fiber.unode.fiber_node.pop_subtype = FIBER;
    def_fiber.unode.fiber_node.freq_type = STIM_FIXED;
    def_fiber.unode.fiber_node.frequency = 1.0;
    def_fiber.unode.fiber_node.fuzzy_range = 1.0;
    def_fiber.unode.fiber_node.slope_scale = 0.0;
  }

}

void cell_loader()
{
  FILE *s_fp;
  char holder[50];

  if ((s_fp = fopen("def_cell.sned","r")) != 0) {
    fscanf(s_fp,"%s %f %s %f %s %f %s %f %s %f %s %f %s %d %s %f %s %f %s %f %s %f %s %f %s %f %s %f %s %f %s %f",
               holder, &(def_cell.unode.cell_node.c_accomodation),
               holder, &(def_cell.unode.cell_node.c_k_conductance),
               holder, &(def_cell.unode.cell_node.c_mem_potential),
               holder, &(def_cell.unode.cell_node.c_resting_thresh),
               holder, &(def_cell.unode.cell_node.c_ap_k_delta),
               holder, &(def_cell.unode.cell_node.c_accom_param),
               holder, &(def_cell.unode.cell_node.c_pop),
               holder, &(def_cell.unode.cell_node.c_rebound_param),
               holder, &(def_cell.unode.cell_node.c_rebound_time_k),
               holder, &(def_cell.unode.cell_node.c_thresh_remove_ika),
               holder, &(def_cell.unode.cell_node.c_thresh_active_ika),
               holder, &(def_cell.unode.cell_node.c_max_conductance_ika),
               holder, &(def_cell.unode.cell_node.c_pro_deactivate_ika),
               holder, &(def_cell.unode.cell_node.c_pro_activate_ika),
               holder, &(def_cell.unode.cell_node.c_constant_ika),
               holder, &(def_cell.unode.cell_node.c_dc_injected));
    def_cell.unode.cell_node.c_resting_thresh_sd = 0;
    fclose(s_fp);
  }
  else {
    def_cell.unode.cell_node.c_accomodation = 500.0;
    def_cell.unode.cell_node.c_k_conductance = 7.0;
    def_cell.unode.cell_node.c_mem_potential = 9.0;
    def_cell.unode.cell_node.c_resting_thresh = 10.0;
    def_cell.unode.cell_node.c_resting_thresh_sd = 0;
    def_cell.unode.cell_node.c_ap_k_delta = 20.0;
    def_cell.unode.cell_node.c_accom_param = 0.3;
    def_cell.unode.cell_node.c_pop = 100;
    def_cell.unode.cell_node.c_rebound_param = 0.3;
    def_cell.unode.cell_node.c_rebound_time_k      = -48;
    def_cell.unode.cell_node.c_thresh_remove_ika   =   6;
    def_cell.unode.cell_node.c_thresh_active_ika   = -40;
    def_cell.unode.cell_node.c_max_conductance_ika =  -6;
    def_cell.unode.cell_node.c_pro_deactivate_ika  = -47.359;
    def_cell.unode.cell_node.c_pro_activate_ika    = -40;
    def_cell.unode.cell_node.c_constant_ika        = -0.00078;
    def_cell.unode.cell_node.c_dc_injected = 0.0;
  }
  def_cell.unode.cell_node.pop_subtype = CELL;
  V6CellAdjustments();
}




