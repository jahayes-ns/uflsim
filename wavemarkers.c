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
#include "wavemarkers.h"

// "impossible" text values to mark start, end of packet, and 
// for simrun to signal eof
// unicode probably breaks this, but it probably breaks everything.
const unsigned char MSG_START = 0xfd;
const unsigned char MSG_END = 0xfe; 
const unsigned char MSG_EOF = 0xff;

// If adding more, make sure these are not P, R, U, T, these used in simloop
const unsigned char PORT_MSG = 'O';
const unsigned char SCRIPT_MSG = 'C';
const unsigned char SIM_MSG = 'I';
const unsigned char SND_MSG = 'N';

