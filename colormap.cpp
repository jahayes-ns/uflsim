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
#include <iostream>
#include <QColor>
#include "inode.h"
#include "colormap.h"

// These are the origian color names, with a few adjustments
// to match the X color rgb values to the closest SVG color values
// Green is lime in SVG, and there is nothing that comes close to 
// matching the magenta value.

static const char *color[] = 
{
	    "lime",
	    "red",
	    "blue",
	    "yellow",
	    "magenta",
	    "cyan",
	    "white",
	    "orange",
	    "hot pink",
	    "violet",
	    "steelblue",
	    "mediumblue",
	    "tan",
	    "lightgray",
	    "skyblue",
	    "turquoise",
	    "seagreen",
	    "palegreen",
	    "greenyellow",
	    "limegreen",
	    "yellowgreen",
	    "forestgreen",
	    "olivedrab",
	    "khaki",
	    "lightyellow",
	    "gold",
	    "indianred",
	    "lightgray",
	    "beige",
	    "sandybrown",
	    "chocolate",
	    "firebrick",
	    "navy",
	    "brown",
	    "darkorange",
	    "coral",
	    "lightblue",
	    "orangered",
	    "aquamarine",
	    "darkgreen",
	    "lightcyan",
	    "deeppink",
	    "pink",
	    "lightpink",
	    "crimson",
	    "mediumvioletred",
	    "darkviolet",
	    "chartreuse",
	    "blueviolet",
	    "aquamarine",
	    "lightgreen",
	    "slateblue",
	    "azure",
	    "lavender"
};


QColor synapseColors[MAX_INODES];

void createSynapseColors()
{
   int ncolors = sizeof color / sizeof (char *);

   for (int syncol = 0; syncol < MAX_INODES; syncol++)
   {
      synapseColors[syncol] = QColor(color[syncol%ncolors]);
//      std::cout << color[syncol%ncolors] << " " <<  synapseColors[syncol].red()
//                              << " " <<  synapseColors[syncol].green()
//                              << " " <<  synapseColors[syncol].blue() << std::endl;
   }
}

