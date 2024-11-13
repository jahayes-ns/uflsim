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


   static inline char *
type_name (int typenum)
{
  switch (typenum)
    {
    case FIBER:
    case fibeR:
      return "fiber_node";
    case CELL:
    case celL:
      return "cell_node";
    case SYNAPSE:
      return "synapse_node";
    case GLOBAL:
      return "global_node";
    default:
      return 0;
    }
}

static char *
choose_OLD_U_NODE (void *parent)
{
  return type_name (((OLD_I_NODE *)parent)->node_type);
}


static char *
choose_U_NODE (void *parent)
{
  return type_name (((I_NODE *)parent)->node_type);
}


