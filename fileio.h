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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#if !HAVE_MALLOC_USABLE_SIZE && HAVE_CONFIG_H
size_t malloc_usable_size (void*);
#endif

#include <stdio.h>

#define FILEIO_FORMAT_VERSION5 5   // 5 & earlier same to us since all get converted to 5
#define FILEIO_FORMAT_VERSION6 6   // new for simbuild
#define FILEIO_SUBV_V61 1
#define FILEIO_SUBV_V62 2
#define FILEIO_FORMAT_CURRENT FILEIO_FORMAT_VERSION6
#define FILEIO_SUBVERSION_CURRENT FILEIO_SUBV_V62

#ifdef __cplusplus
extern "C" {
#endif
void print_member (FILE *f, char *struct_name, char *member, void *struc);
void load_struct (FILE *f, char *struct_name, void *struct_ptr, int ptr_valid);
void save_struct (FILE *f, char *struct_name, void *struct_ptr);
FILE * save_struct_open (char *filename);
FILE * load_struct_open (char *filename);
int load_struct_read_version (FILE *f);

FILE * load_struct_open_simbuild (char *filename);
int load_struct_read_version_simbuild (FILE *f);

extern int save_all;
extern int save_state;
extern int save_init;
extern int Version;
#ifdef __cplusplus
}
#endif
