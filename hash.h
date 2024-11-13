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

#ifndef HASH_H
#define HASH_H
struct StructInfo 
{
  char *name;
  int offset;
  void (*val)(char *, void *, int);
  char *(*str)(void *);
  char *struct_name;
  int *dim;
  int size;
  int count;
  unsigned int ptr:1;
  unsigned int tag:1;
  unsigned int direct:1;
  unsigned int indirect:1;
  unsigned int state:1;
  unsigned int skip:1;
  unsigned int string:1;
  unsigned int init:1;
};
typedef struct StructInfo StructInfo;

struct StructMembers
{
    char *name;
    char *(*choose)();
    char **member;
};
typedef struct StructMembers StructMembers;

char *str_char (void *p);
char *str_float (void *p);
char *str_double (void *p);
char *str_int (void *p);
char *str_short (void *p);
char *str_long (void *p);
char *str_float_star (void *p);
char *str_double_star (void *p);
char *str_int_star (void *p);
char *str_FILE_star (void *p);
char *str_char_star (void *p);
void ato_char (char *valstr, void *buf, int size);
void ato_float (char *valstr, void *buf, int size);
void ato_double (char *valstr, void *buf, int size);
void ato_int (char *valstr, void *buf, int size);
void ato_short (char *valstr, void *buf, int size);
void ato_long (char *valstr, void *buf, int size);
void ato_FILE_star (char *valstr, void *ptr, int size);
void ato_float_star (char *valstr, void *ptr, int size);
void ato_int_star (char *valstr, void *ptr, int size);
void ato_char_star (char *valstr, void *ptr, int size);
void ato_char_string (char *valstr, void *ptr, int size);
char *str_char_string (void *p);
#endif
