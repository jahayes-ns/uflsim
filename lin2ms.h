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
#ifdef __cplusplus
extern "C" {
#endif


#if defined(WIN_32) || defined(_WIN32)

#include <winsock2.h>
#undef HAVE_MALLOC_USABLE_SIZE
#undef HAVE_LIBJUDY // we don't have this in Windows, 
                    // there is a substitute function for it.

ssize_t getline(char **linebuf, size_t *linebufsz, FILE *file) ;

#endif


#ifdef __cplusplus
}
#endif

