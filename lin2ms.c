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

// A grab bag of functions linux has that mswin does not. All in one place.

#include <stdio.h> /* needed for vsnprintf */
#include <stdlib.h> /* needed for malloc-free */
#include <errno.h>
#include "lin2ms.h"


/* getdelim for uClibc
 *
 * Copyright (C) 2000 by Lineo, inc. and Erik Andersen
 * Copyright (C) 2000,2001 by Erik Andersen <andersen@uclibc.org>
 * Written by Erik Andersen <andersen@uclibc.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Library General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Library General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */


/* Read up to (and including) a TERMINATOR from STREAM into *LINEPTR
   (and null-terminate it). *LINEPTR is a pointer returned from malloc (or
   NULL), pointing to *N characters of space.  It is realloc'd as
   necessary.  Returns the number of characters read (not including the
   null delimiter), or -1 on error or EOF.  */

#ifdef WIN32

static ssize_t
getdelim(char **linebuf, size_t *linebufsz, int delimiter, FILE *file)
{
  static const int GROWBY = 80; /* how large we will grow strings by */

  int ch;
  size_t idx = 0;

  if (file == NULL || linebuf==NULL || linebufsz == NULL) {
    errno = EINVAL;
    return -1;
  }

  if (*linebuf == NULL || *linebufsz < 2) {
    *linebuf = malloc(GROWBY);
    if (!*linebuf) {
      errno = ENOMEM;
      return -1;
    }
    *linebufsz += GROWBY;
  }

  while (1) {
    ch = fgetc(file);
    if (ch == EOF)
      break;
    /* grow the line buffer as necessary */
    while (idx > *linebufsz-2) {
      *linebuf = realloc(*linebuf, *linebufsz += GROWBY);
      if (!*linebuf) {
	errno = ENOMEM;
	return -1;
      }
    }
    (*linebuf)[idx++] = (char)ch;
    if ((char)ch == delimiter)
      break;
  }

  if (idx != 0)
    (*linebuf)[idx] = 0;
  else if ( ch == EOF )
    return -1;
  return idx;
}


ssize_t
getline(char **linebuf, size_t *linebufsz, FILE *file)
{
  return getdelim (linebuf, linebufsz, '\n', file);
}

void error(int status, int errnum, const char* format, ...)
{
   fflush(stdout);
   fprintf(stdout,"Error status %d num %d",status,errnum);
   va_list argptr;
   va_start(argptr,format);
   vfprintf(stdout,format,argptr);
   va_end(argptr);
   fprintf(stdout,"\n");
   fflush(stdout);
   exit(1);
}

void error_at_line(int status, int errnum, const char* filename, unsigned int linenum,const char* format,...)
{
   fflush(stdout);
   fprintf(stdout,"Error status %d num %d file %s line number %u",status,errnum,filename,linenum);
   va_list argptr;
   va_start(argptr,format);
   vfprintf(stdout,format,argptr);
   va_end(argptr);
   fprintf(stdout,"\n");
   fflush(stdout);
   exit(1);
}



// This is Windows equivalent of malloc_usable_size(void*) In Linux, this often
// returns a number larger than the requested size of the allocated memory. In
// Windows it always returns the requested size, not the (perhaps) larger
// amount that may have actually been allocated.
size_t malloc_usable_size(void* block)
{
   if (block == NULL)
      return 0;
   size_t bytes;
   bytes = _msize(block); 
   return bytes;
}

#else
 

// linux

// It would be nice to be able to read DOS text files. The getline function
// doesn't work correctly since some callers don't know how to deal with \r\n.
// Since getline is called in a lot of places, we intercept the call to the C
// lib's getline function by adding -Wl,--wrap=getline to the link command.
// It sends all getline calls to here. This is a substitute, we don't call the
// real getline (but we could by calling __real_getline(args))

size_t __wrap_getline(char **linebuf, size_t *linebufsz, FILE *file)
{
  static const int GROWBY = 80; /* how large we will grow strings by */

  int ch;
  size_t idx = 0;

  if (file == NULL || linebuf==NULL || linebufsz == NULL) {
    errno = EINVAL;
    return -1;
  }

  if (*linebuf == NULL || *linebufsz < 2) {
    *linebuf = malloc(GROWBY);
    if (!*linebuf) {
      errno = ENOMEM;
      return -1;
    }
    *linebufsz += GROWBY;
  }

  while (1) {
    ch = fgetc(file);
    if (ch == EOF)
      break;
    /* grow the line buffer as necessary */
    while (idx > *linebufsz-2) {
      *linebuf = realloc(*linebuf, *linebufsz += GROWBY);
      if (!*linebuf) {
	errno = ENOMEM;
	return -1;
      }
    }
    if (ch == '\r') // DOS file, toss the char
       continue;
    (*linebuf)[idx++] = (char)ch;
    if ((char)ch == '\n')
      break;
  }

  if (idx != 0)
    (*linebuf)[idx] = 0;
  else if ( ch == EOF )
    return -1;
  return idx;
}

#endif

#ifdef __cplusplus
extern "C" }
#endif
