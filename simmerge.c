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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

extern ssize_t getline(char **, size_t *, FILE *);

int
hdr_parse (char *line, int len, int *codelen, int *timelen)
{
  int n = 0;

  while (line[n] == ' ' && n < len)
    n++;
  if (n == len)
    return 0;
  while (line[n] != ' ' && n < len)
    n++;
  if (n == len)
    return 0;
  *codelen = n;
  while (line[n] == ' ' && n < len)
    n++;
  if (n == len)
    return 0;
  while (!isspace (line[n]) && n < len)
    n++;
  if (n == len)                 /* NL is inluded in len */
    return 0;
  *timelen = n - *codelen;

  return 1;
}

static inline void 
parse (char *line, int len, int codelen, int timelen, long *codep, unsigned long *timep)
{
  if (codelen >= len)
    return;
    
  *timep = -1;
  if ((*timep = strtoul (line + codelen, 0, 10)) < 0)
    return;
  line[codelen] = 0;
  *codep = strtol (line, 0, 10);
}

static inline int
get_code_and_time (char **linep, size_t *allocp, FILE *f, ssize_t *lenp, int codelen, int timelen, long *codep, unsigned long *timep)
{
  *codep = -1;
  while (*lenp >= 0 && *codep < 0) {
    *lenp = getline (linep, allocp, f);
    if (*lenp >= 0) parse (*linep, *lenp, codelen, timelen, codep, timep);
  }
  return *lenp;
}

int
main (int argc, char **argv)
{
  FILE *f0, *f1 = 0;
  char *line0 = NULL, *line1 = NULL;
  size_t alloc0 = 0, alloc1 = 0;
  ssize_t len0, len1;
  int codelen0, timelen0;
  int codelen1, timelen1;
  int codelen, timelen;
  char *filename1, *header;
  int scale0 = 1, scale1 = 1;
  long code0 = -1, code1 = -1;
  unsigned long time0 = 0, time1 = 0;

  if (argc < 2) {
    fprintf (stderr, "usage: %s input1.[eb]dt [input2.[eb]dt] > output.[eb]dt\n", argv[0]);
    return 1;
  }
  if ((f0 = fopen (argv[1], "r")) == 0) {
    fprintf (stderr, "Can't open %s for read: %s\n", argv[1], strerror (errno));
    exit (1);
  }
  if (argc > 2 && (f1 = fopen (argv[2], "r")) == 0) {
    fprintf (stderr, "Can't open %s for read: %s\n", argv[2], strerror (errno));
    return 2;
  }
  if (argc == 2) {
    filename1 = "stdin";
    f1 = stdin;
  }
  else
    filename1 = argv[2];

  len0 = getline(&line0, &alloc0, f0);
  if (len0 >= 0 && !hdr_parse (line0, len0, &codelen0, &timelen0)) {
    fprintf (stderr, "bad header in %s, aborting\n", argv[1]);
    return 3;
  }

  len1 = getline(&line1, &alloc1, f1);
  if (len1 >= 0 && !hdr_parse (line1, alloc1, &codelen1, &timelen1)) {
    fprintf (stderr, "bad header in %s, aborting\n", filename1);
    return 4;
  }

  if (len0 < 0 && len1 < 0)
    return 0;
  else if (len0 < 0 && len1 >= 0) {
    header = strdup (line1);
    codelen = codelen1;
    timelen = timelen1;
  }
  else if (len0 >= 0 && len1 < 0) {
    header = strdup (line0);
    codelen = codelen0;
    timelen = timelen0;
  }
  else {
    if (codelen0 >= codelen1 && timelen0 >= timelen1) {
      header = strdup (line0);
      codelen = codelen0;
      timelen = timelen0;
      if (timelen0 >= 10 && timelen1 < 10)
        scale1 = 5;
    }
    else if (codelen0 <= codelen1 && timelen0 <= timelen1) {
      header = strdup (line1);
      codelen = codelen1;
      timelen = timelen1;
      if (timelen1 >= 10 && timelen0 < 10)
        scale0 = 5;
    }
    else {
      printf ("Can't decide what format to use.\n");
      printf ("%s is I%dI%d\n", argv[1], codelen0, timelen0);
      printf ("%s is I%dI%d\n", filename1, codelen1, timelen1);
      printf ("aborting\n");
      return 5;
    }
  }

  if (len0 >= 0) len0 = getline(&line0, &alloc0, f0); /* second header line */
  if (len1 >= 0) len1 = getline(&line1, &alloc1, f1);

  printf ("%s%s", header, header);

  get_code_and_time (&line0, &alloc0, f0, &len0, codelen, timelen, &code0, &time0);
  get_code_and_time (&line1, &alloc1, f1, &len1, codelen, timelen, &code1, &time1);

  while (len0 >= 0 || len1 >= 0) {
    if (len1 < 0 || (len0 >= 0 && len1 >= 0 && time0 * scale0 <= time1 * scale1)) {
      printf ("%*ld%*ld\n", codelen, code0, timelen, time0 * scale0);
      get_code_and_time (&line0, &alloc0, f0, &len0, codelen, timelen, &code0, &time0);
    }
    else {
      printf ("%*ld%*ld\n", codelen, code1, timelen, time1 * scale1);
      get_code_and_time (&line1, &alloc1, f1, &len1, codelen, timelen, &code1, &time1);
    }
  }
  return 0;
}  
