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

#ifdef GENHASHDEF
#undef OLD_INODE_H
#endif


#ifndef OLD_INODE_H
#define OLD_INODE_H

#define OLD_MAX_INODES 100

typedef	struct  {	/* structure to hold axon node coordinates           */
	int	num_coords;		 /* number of coordinates per particular axon  */
	short	x_coord[15];		 /* maximum of 16 nodes - 15 segments  */
	short	y_coord[15];		 /* maximum of 16 nodes - 15 segments  */
} OLD_AXON;

typedef struct  { 	/* structure to hold global data structures for SNED */
	int	total_populations;	/* total number of cell+fiber pops   */
	int	total_fibers;		/* total number of fiber pops        */
	int	total_cells;		/* total number of cell pops         */
	int 	sim_length;		/* length of simulation in milisecs  */
	float	k_equilibrium;		/* potasium equilibrium potential    */
	int	max_conduction_time;	/* maximum conduction time           */
	int	num_synapse_types;	/* total number of synaptic types    */
	int	max_targets;		/* max number of targets             */
	float	step_size;		/* standard time increment           */
	int	maximum_cells_per_pop;	/* self explanatory                  */
  char	global_comment[100];//string	/* user-fillable comment variable    */
}  OLD_GLOBAL_NETWORK;

typedef struct  {        /*  This TYPE is the Fiber node sub-data structure  */
	short	fiber_x,fiber_y;	/* graphical x and y positions       */
	OLD_AXON fiber_axon[TABLE_LEN];
					/* graphical axon coordinates        */
	float	f_prob;			/* probability of this f pop firing  */
	int 	f_begin;		/* time this f pop begins firing     */
	int 	f_end;			/* time this f pop ends firing       */
	int 	f_seed;			/* random number seed for this f pop */
	int 	f_pop;			/* number of fibers in this f pop    */
	int	f_targets;		/* number of targets of this f pop   */
	int	f_target_nums[TABLE_LEN];
					/* id's of this f pop's targets      */
	int	f_conduction_time[TABLE_LEN];
					/* conduction time to target         */
	short	f_terminals[TABLE_LEN];
					/* number of f terminals per target  */
	int 	f_synapse_type[TABLE_LEN];
					/* f target's synapse type           */
	float	f_synapse_strength[TABLE_LEN];
					/* f target's synapse strength       */
	int 	f_target_seed[TABLE_LEN];
					/* fiber pop linkup seed for target  */
}	OLD_F_NODE;



typedef struct  {        /*  This TYPE is the Cell node sub-data structure   */
	short	cell_x,cell_y;		/* graphical x and y positions       */
	OLD_AXON cell_axon[TABLE_LEN];
					/* graphical axon coordinates        */
	float	c_accomodation;		/* time const for this c pop accom   */
	float	c_k_conductance;		/* potasium                          */
	float	c_mem_potential;		/* cell pop membrane potential       */
	float	c_resting_thresh;		/* cell pop resting threshold        */
	float	c_ap_k_delta;		/* cell pop ap potasium cond delta   */
	float	c_accom_param;		/* cell pop accomodation parameter   */
	int 	c_pop;			/* number of cells in this c pop     */
	int	c_targets;		/* number of targets of this c pop   */
	int	c_target_nums[TABLE_LEN];
					/* id's of this c pop's targets      */
	int	c_conduction_time[TABLE_LEN];
					/* conduction time to target         */
	short	c_terminals[TABLE_LEN];
					/* number of c terminals per target  */
	int 	c_synapse_type[TABLE_LEN];
					/* c target's synapse type           */
	float	c_synapse_strength[TABLE_LEN];
					/* c target's synapse strength       */
	int 	c_target_seed[TABLE_LEN];
					/* cell pop linkup seed for target   */
	float	c_rebound_param;	/* cell pop rebound parameter        */
	float	c_rebound_time_k;	/* cell pop rebound time constant    */
	float	c_thresh_remove_ika;	/* cell pop threshold to remove ika  */
	float	c_thresh_active_ika;	/* cell pop threshold to activate ika*/
	float	c_max_conductance_ika;	/* cell pop maximum ika conductance  */
	float	c_pro_deactivate_ika;	/* ika deactivation removal constant */
	float	c_pro_activate_ika;	/* ika activation constant           */
	float	c_constant_ika;		/* ika time constant                 */
	float	c_dc_injected;		/* dc injected currrent per pop      */	
}	OLD_C_NODE;

typedef struct  {                       /* create the synapse definition node*/
  char	synapse_name[TABLE_LEN][SNMLEN];//string
					/* synapse name                      */
	float	s_eq_potential[TABLE_LEN];
					/* equilibrium potential             */
	float	s_time_constant[TABLE_LEN];
					/* synaptic time constant            */
}	OLD_S_NODE;


typedef union {
  OLD_GLOBAL_NETWORK global_node;	/* global-type node          */
  OLD_C_NODE	 cell_node;	/* cell-type node                    */
  OLD_F_NODE	 fiber_node;	/* fiber-type node                   */
  OLD_S_NODE	 synapse_node;	/* synapse parameter node            */
}  OLD_U_NODE;


typedef  struct  {			/* actual I-NODE creation            */
  int	node_type;		/* i-node type descriptor            */
  int	node_number;		/* the actual node number of this type*/
  char	comment1[100];		/* first node comment                */
  char	comment2[100];		/* second node comment               */
  OLD_U_NODE unode;
} OLD_I_NODE;

#endif

