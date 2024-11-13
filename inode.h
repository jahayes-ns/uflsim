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



/* 2-apr-2003 added synapse name length SNMLEN define */

/*
   The hash generation breaks if you have a guard def, don't know why, seems
   like once INODE_H gets def'ed, the second stage sees no content on the
   second pass through the file.  We don't def this in the Makefile, so the
   gen_hash.sh script works and the guard def works for compiles.

   IMPORTANT: Some of the vars have a / /  string (without spaces) attached to
   them. The hash generation requires these, so don't modify them. Also, if you
   add new char* fields, be sure to tag them with / / string.

   Added later, there also seems to be other types of / / tags.
*/


#ifdef GENHASHDEF
#undef INODE_H
#endif


#ifndef INODE_H
#define INODE_H  1

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include <stdio.h>
#include <stdbool.h>

/* #define SNMLEN 15  this seems rather short. . .*/ 
#define SNMLEN 32
#define GRPLEN 64

#ifdef __GNUC__
#define INLINE inline
#else
#define INLINE
#endif

#include "util.h"
#include "common_def.h"

/*    Symbolic network Editor Structural declaration header file     */

/* this must have a historical meaning, but it makes the code mysterious  */
#define MAX_FIVES 20  /* number of groups of five in table */
#define TABLE_LEN (5 * MAX_FIVES) /* aka, 100 */

/* define node type NAMES */
/* a total of 256 types may be added in the future if needed */
#define UNUSED         0
#define FIBER          1
#define CELL           2
#define SYNAPSE        3
#define GLOBAL         4
#define celL           5     /* pre simbuild types, not used except to read old files */
#define fibeR          6
#define BURSTER_POP    7     /* these new for simbuild, use explicit types */
#define PSR_POP        8     /* instead of special values for some vars as flags */
#define PHRENIC_POP    9
#define LUMBAR_POP    10 
#define INSP_LAR_POP  11
#define EXP_LAR_POP   12
#define ELECTRIC_STIM 13    /* a type of fiber pop */
#define AFFERENT      14    /* another type of fiber pop */
#define SUBSYSTEM    100    /* global type of partial/subsets of a model */

#define MAX_NODE_TYPES 50 /* Somewhat arbitrary, but replaces some magic #'s */
                          /* in existing files that expect this to be 50 */

#define MAX_INODES   100     /* max globals + cells + fibers + axons + synapse types */
#define GLOBAL_INODE   0    /* these are special, first is global */
#define SYNAPSE_INODE (MAX_INODES-1) /* last is synapse types */
#define FIRST_INODE 1
#define LAST_INODE (SYNAPSE_INODE-1)
#define FIRST_TARGET_IDX 0

/* explicit, not inferred synapse types */
/* added for simbuild to alter how synapse types are managed. */
#define SYN_NOT_USED 0
#define SYN_NORM 1
#define SYN_PRE 2
#define SYN_POST 3
#define SYN_LEARN 4

/* type of electric stim */
#define STIM_FIXED 1 
#define STIM_FUZZY 2


/* structure to hold axon node coordinates */
/* maximum of 16 nodes - 15 segments  */
#define MAX_AXON_SEGMENTS  15

typedef	struct  {	/* structure to hold axon node coordinates           */
	int   num_coords;                    /* number of coordinates per particular axon  */
	short x_coord[MAX_AXON_SEGMENTS];
	short y_coord[MAX_AXON_SEGMENTS];
} AXON;

/* structure to hold global data structures for SNED */
typedef struct  { 	/* structure to hold global data structures for SNED */
	int total_populations; /* total number of cell+fiber pops   */
	int total_fibers;  /* total number of fiber pops        */
	int total_cells;  /* total number of cell pops         */
	int  sim_length;  /* length of simulation in millisecs  */
	float k_equilibrium;  /* potasium equilibrium potential    */
	int max_conduction_time; /* maximum conduction time           */
	int num_synapse_types; /* total number of synaptic types    */
	int max_targets;  /* max number of targets             */
	float step_size;  /* standard time increment           */
	int maximum_cells_per_pop; /* self explanatory                  */
	char global_comment[100];//string /* user-fillable comment variable    */
	char phrenic_equation[100];//string /* phrenic recruitment equation      */
	char lumbar_equation[100];//string /* lumbar recruitment equation       */
	float ilm_elm_fr ; /* ilm and elm firing rate --new for simbuild */
   float sim_length_seconds; /* length in seconds --new for simbuild */
}  GLOBAL_NETWORK;

/*  This TYPE is the Fiber node sub-data structure */
typedef struct  {        /*  This TYPE is the Fiber node sub-data structure  */
	short fiber_x; /* graphical x position       */
	short fiber_y; /* graphical y position       */
	AXON fiber_axon[TABLE_LEN];
	/* graphical axon coordinates        */
	float f_prob;   /* probability of this f pop firing  */
	int  f_begin;  /* time this f pop begins firing     */
	int  f_end;   /* time this f pop ends firing  (ms) */
	int  f_seed;   /* random number seed for this f pop */
	int  f_pop;   /* number of fibers in this f pop    */
	int f_targets;  /* number of targets of this f pop   */
   char group[GRPLEN];//string;
	int f_target_nums[TABLE_LEN]; /* id's of this f pop's targets      */
	int f_min_conduct_time[TABLE_LEN]; /* minimum conduction time to target */
	int f_conduction_time[TABLE_LEN]; /* conduction time to target         */
	short f_terminals[TABLE_LEN]; /* number of f terminals per target  */
	int  f_synapse_type[TABLE_LEN]; /* f target's synapse type           */
	float f_synapse_strength[TABLE_LEN]; /* f target's synapse strength       */
	int  f_target_seed[TABLE_LEN]; /* fiber pop linkup seed for target  */
   int pop_subtype; /* can now have types of fiber pops */
   int freq_type;     /* electric stim subtype params fixed or fuzzy */
   float frequency;   /* how often to fire stim (Hz) */
   float fuzzy_range; /* if fuzzy, width of range to randomly vary stim fire (ms) */
   char afferent_prog[1024];//string
   char afferent_file[1024];//string
   int num_aff;
   int offset;
   double aff_val[MAX_AFFERENT_PROB];
   double aff_prob[MAX_AFFERENT_PROB];
   double slope_scale;
} F_NODE;




/*  This TYPE is the Cell node sub-data structure */
typedef struct  {        /*  This TYPE is the Cell node sub-data structure   */
	short cell_x;  /* graphical x positions       */
	short cell_y;  /* graphical y positions       */
	AXON cell_axon[TABLE_LEN];
	/* graphical axon coordinates        */
	float c_accomodation;  /* time const for this c pop accom   */
	float c_k_conductance;  /* potasium                          */
	float c_mem_potential;  /* cell pop membrane potential       */
	float c_resting_thresh;  /* cell pop resting threshold        */
	float c_resting_thresh_sd;  /* cell pop resting threshold SD       */
	float c_ap_k_delta;  /* cell pop ap potasium cond delta   */
	float c_accom_param;  /* cell pop accomodation parameter   */
	int  c_pop;   /* number of cells in this c pop     */
	int c_targets;  /* number of targets of this c pop   */
   char group[GRPLEN];//string;
	int c_target_nums[TABLE_LEN]; /* id's of this c pop's targets      */
	int c_min_conduct_time[TABLE_LEN]; /* minimum conduction time to target */
	int c_conduction_time[TABLE_LEN]; /* conduction time to target         */
	short c_terminals[TABLE_LEN]; /* number of c terminals per target  */
	int  c_synapse_type[TABLE_LEN]; /* c target's synapse type           */
	float c_synapse_strength[TABLE_LEN]; /* c target's synapse strength       */
	int  c_target_seed[TABLE_LEN]; /* cell pop linkup seed for target   */
	float c_rebound_param; /* cell pop rebound parameter        */
	float c_rebound_time_k; /* cell pop rebound time constant    */
	float c_thresh_remove_ika; /* cell pop threshold to remove ika  */
	float c_thresh_active_ika; /* cell pop threshold to activate ika*/
	float c_max_conductance_ika; /* cell pop maximum ika conductance  */
	float c_pro_deactivate_ika; /* ika deactivation removal constant */
	float c_pro_activate_ika; /* ika activation constant           */
	float c_constant_ika;  /* ika time constant                 */
	float c_dc_injected;  /* dc injected currrent per pop      */
	char   *c_injected_expression;//string  /* injected current as a function of volume */
   char  pop_subtype; /* many types of cells */
} C_NODE;

/* the synapse definition node */
typedef struct  {                       /* create the synapse definition node*/
	char synapse_name[TABLE_LEN][SNMLEN];//string
	/* synapse name */
	float s_eq_potential[TABLE_LEN];
	/* equilibrium potential */
	float s_time_constant[TABLE_LEN];
	/* synaptic time constant */
   char syn_type[TABLE_LEN];
   /* synapse type, new for simbuild, unused=0 normal=1, pre=2, post=3 */
   int parent[TABLE_LEN];
   /* what norm synapse is associated with pre/post/maybe other types? */
   // # ticks previous firings are remembered
   int  lrn_window[TABLE_LEN];   
   // maximum strength value
   double lrn_maxstr[TABLE_LEN];
   // increase/decrease factor
   double lrn_delta[TABLE_LEN];
} S_NODE;

#include "old_inode.h"

/* each I_NODE has one of these, only one union member is in use. */
typedef union	{
	GLOBAL_NETWORK global_node; /* global-type node */
	C_NODE cell_node;           /* cell-type node */
	F_NODE fiber_node;          /* fiber-type node */
	S_NODE synapse_node;        /* synapse parameter node */
}  U_NODE;

/* I-NODE */
typedef  struct  {			/* actual I-NODE creation            */
	int node_type;  /* i-node type descriptor            */
	int node_number;  /* the actual node number of this type*/
                     /* used for population value */
	char comment1[100];//string  /* first node comment                */
	char comment2[100];//string  /* second node comment               */
	U_NODE unode;
} I_NODE;

extern void init_nodes (void);
extern void swap_inode_bytes (int);

typedef struct
{
   int      file_subversion;
	int      malloc_debug;
	char     doc_file[1024];//string
	I_NODE   inode[MAX_INODES];
	int      num_used_nodes[MAX_NODE_TYPES];
	int      num_free_nodes;
	int    presynaptic_flag;
	char   baby_lung_flag;
} inode_global;

extern inode_global D;

#define MAX_ENTRIES 1000
#define MAX_SPAWN 20
#define MAX_LAUNCH MAX_SPAWN
#define REBOUND 1
void alert (char *msg) ;

#endif

