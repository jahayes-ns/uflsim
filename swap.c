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
#include "swap.h"

static INLINE void
swap_axon_bytes (AXON *axon)
{
	int n;
  
	for (n = 0; n < TABLE_LEN; n++) {
		SWAPLONG (axon[n].num_coords);
		SWAPSHORT (axon[n].x_coord);
		SWAPSHORT (axon[n].y_coord);
	}
}

static INLINE void
swap_fnode_bytes (F_NODE *fnode)
{
	SWAPSHORT (fnode->fiber_x);
	SWAPSHORT (fnode->fiber_y);
	swap_axon_bytes (fnode->fiber_axon);
	SWAPLONG (fnode->f_prob);
	SWAPLONG (fnode->f_begin);
	SWAPLONG (fnode->f_end);
	SWAPLONG (fnode->f_seed);
	SWAPLONG (fnode->f_pop);
	SWAPLONG (fnode->f_targets);
	SWAPLONG (fnode->f_target_nums);
	SWAPLONG (fnode->f_min_conduct_time);
	SWAPLONG (fnode->f_conduction_time);
	SWAPSHORT(fnode->f_terminals);
	SWAPLONG (fnode->f_synapse_type);
	SWAPLONG (fnode->f_synapse_strength);
	SWAPLONG (fnode->f_target_seed);
}

static INLINE void
swap_cnode_bytes (C_NODE *cnode)
{
	swap_axon_bytes (cnode->cell_axon);

	SWAPSHORT(cnode->cell_x);
	SWAPSHORT(cnode->cell_y);
	SWAPLONG (cnode->c_accomodation);
	SWAPLONG (cnode->c_k_conductance);
	SWAPLONG (cnode->c_mem_potential);
	SWAPLONG (cnode->c_resting_thresh);
	SWAPLONG (cnode->c_ap_k_delta);
	SWAPLONG (cnode->c_accom_param);
	SWAPLONG (cnode->c_pop);
	SWAPLONG (cnode->c_targets);
	SWAPLONG (cnode->c_target_nums);
	SWAPLONG (cnode->c_min_conduct_time);
	SWAPLONG (cnode->c_conduction_time);
	SWAPSHORT(cnode->c_terminals);
	SWAPLONG (cnode->c_synapse_type);
	SWAPLONG (cnode->c_synapse_strength);
	SWAPLONG (cnode->c_target_seed);
	SWAPLONG (cnode->c_rebound_param);
	SWAPLONG (cnode->c_rebound_time_k);
	SWAPLONG (cnode->c_thresh_remove_ika);
	SWAPLONG (cnode->c_thresh_active_ika);
	SWAPLONG (cnode->c_max_conductance_ika);
	SWAPLONG (cnode->c_pro_deactivate_ika);
	SWAPLONG (cnode->c_pro_activate_ika);
	SWAPLONG (cnode->c_constant_ika);
	SWAPLONG (cnode->c_dc_injected);
}

static INLINE void
swap_snode_bytes (S_NODE *snode)
{
	SWAPLONG (snode->s_eq_potential);
	SWAPLONG (snode->s_time_constant);
}

static INLINE void
swap_gnode_bytes (GLOBAL_NETWORK *gnode)
{
	SWAPLONG (gnode->total_populations);
	SWAPLONG (gnode->total_fibers);
	SWAPLONG (gnode->total_cells);
	SWAPLONG (gnode->sim_length);
	SWAPLONG (gnode->k_equilibrium);
	SWAPLONG (gnode->max_conduction_time);
	SWAPLONG (gnode->num_synapse_types);
	SWAPLONG (gnode->max_targets);
	SWAPLONG (gnode->step_size);
	SWAPLONG (gnode->maximum_cells_per_pop);
}

void
swap_inode_bytes (int before)
{
	int n;

	SWAPLONG (D.num_used_nodes);
	SWAPLONG (D.num_free_nodes);

	SWAPLONG (D.inode[0].node_type);
	SWAPLONG (D.inode[0].node_number);
	swap_gnode_bytes (&D.inode[0].unode.global_node);

	SWAPLONG (D.inode[99].node_type);
	SWAPLONG (D.inode[99].node_number);
	swap_snode_bytes (&D.inode[99].unode.synapse_node);

	for (n = 1; n < 99; n++) {
		if (before) {
			SWAPLONG (D.inode[n].node_type);
			SWAPLONG (D.inode[n].node_number);
		}

		switch (D.inode[n].node_type) {
		case FIBER:
		case fibeR:
			swap_fnode_bytes (&D.inode[n].unode.fiber_node);
			break;
		case CELL:
		case celL:
			swap_cnode_bytes (&D.inode[n].unode.cell_node);
			break;
		case SYNAPSE:
			swap_snode_bytes (&D.inode[n].unode.synapse_node);
			break;
		case GLOBAL:
			swap_gnode_bytes (&D.inode[n].unode.global_node);
			break;
		}
		if (!before) {
			SWAPLONG (D.inode[n].node_type);
			SWAPLONG (D.inode[n].node_number);
		}

	}
}
