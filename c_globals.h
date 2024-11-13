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

#ifndef C_GLOBALS_H
#define C_GLOBALS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "util.h"
#include "inode.h"
#include "simulator.h"
#include "hash.h"

extern void global_loader();
extern void synapse_loader(); 
extern void fiber_loader();
extern void cell_loader();
extern void init_sp();
extern void get_maxes(inode_global*);
extern int get_node(unsigned char);
extern void free_node(int);
extern int findTargetIdx(int);
extern int getSynapse();
extern void freeSynapse(int);
extern int lastSynType();
extern int synTypesInUse();
extern struct StructInfo * simulator_struct_info (const char *str, unsigned int len);
extern struct StructMembers * simulator_struct_members (const char *str, unsigned int len);
extern struct StructInfo *(*struct_info_fn) (const char *str, unsigned int len);
extern struct StructMembers *(*struct_members_fn) (const char *str, unsigned int len);

/*-------------------------------------------------------------
**	Global Variables
*/


extern inode_global D;        // main database for model
extern simulator_global S;    // simbuild exports this, simrun uses it to run the sim
extern int maxCellPop;
extern int maxFiberPop;
extern int Version;
extern int SubVersion;


/* Program Variable space to follow                             */

extern I_NODE		def_fiber;	/*  Create default fiber parameters  */
extern I_NODE		def_cell;	/*  Create default fiber parameters  */
extern AXON		local_axon;     /*  temporary holder!!               */

extern bool cellsInUse[MAX_INODES];  // population numbers
extern bool fibersInUse[MAX_INODES];

extern char  snedfile[1024];  /* default file name holder          */


/*   spawn system area variables to follow ******************************/

//extern int		bdt_flag;
//extern int		wave_flag;
//extern int		smr_flag;
//extern int		smr_wave_flag;
//extern int		analog_flag;
//extern int		condi_flag;
extern int		currModel;

// analog arrays
extern long		sp_inter[MAX_SPAWN];
extern int		sp_aid[MAX_SPAWN];
extern int		sp_apop[MAX_SPAWN];
extern int		sp_arate[MAX_SPAWN];
extern float		sp_atk[MAX_SPAWN];
extern float		sp_ascale[MAX_SPAWN];
extern int		sp_p1code[MAX_SPAWN];
extern int		sp_p1start[MAX_SPAWN];
extern int		sp_p1stop[MAX_SPAWN];
extern int		sp_p2code[MAX_SPAWN];
extern int		sp_p2start[MAX_SPAWN];
extern int		sp_p2stop[MAX_SPAWN];
extern int		sp_smooth[MAX_SPAWN];
extern int	   sp_sample[MAX_SPAWN];
extern float	sp_plus[MAX_SPAWN];
extern float	sp_minus[MAX_SPAWN];
extern int	   sp_freq[MAX_SPAWN];

// bdt table arrays
extern int		sp_bcf[MAX_ENTRIES][MAX_SPAWN];
extern int		sp_bpn[MAX_ENTRIES][MAX_SPAWN];
extern int		sp_bm[MAX_ENTRIES][MAX_SPAWN];
extern char    sp_hostname[MAX_SPAWN][64];

// plot table arrays
extern int		sp_bpn2[MAX_ENTRIES][MAX_SPAWN];
extern int		sp_bm2[MAX_ENTRIES][MAX_SPAWN];
extern int		sp_bv2[MAX_ENTRIES][MAX_SPAWN];

#ifdef __cplusplus
}
#endif
#endif
