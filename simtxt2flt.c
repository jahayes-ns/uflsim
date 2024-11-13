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
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <argp.h>

#if HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

const char *argp_program_version = "txt2flt 1.0";
const char *argp_program_bug_address = "<roconnor@hsc.usf.edu>";

static char doc[] = "converts text numbers to binary numbers"
"\vThe output is binary numbers on standard output.";

static char args_doc[] = "[FILE...]";

static struct argp_option options[] = {
  {"double", 'd', 0, 0, "output binary double precision C floating point values" },
  {"short", 's', 0, 0, "output binary C short integer values" },
  {"int", 'i', 0, 0, "output binary C int (integer) values" },
  {"float", 'f', 0, 0, "output binary single precision C floating point values (default)" },
  { 0 }
};

struct arguments
{
  char *file1;
  char **files;
  char type;
};

static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  /* Get the INPUT argument from `argp_parse', which we
     know is a pointer to our arguments structure. */
  struct arguments *arguments = state->input;

  switch (key)
    {
    case 'd':
    case 'f':
    case 's':
    case 'i':
      arguments->type = key;
      break;

    case ARGP_KEY_ARG:
      arguments->file1 = arg;
      arguments->files = &state->argv[state->next];
      state->next = state->argc;
      break;

    case ARGP_KEY_END:
      if ((state->arg_num == 0 && isatty (0)))
	argp_usage (state);
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc };

static int
process_file (char *filename, FILE *f, struct arguments *arguments)
{
  if (arguments->type == 'd') {
    double val;
    while (fscanf (f, "%lf", &val) == 1)
      fwrite (&val, sizeof val, 1, stdout);
  }
  else if (arguments->type == 'f') {
    float val;
    while (fscanf (f, "%f", &val) == 1)
      fwrite (&val, sizeof val, 1, stdout);
  }
  else if (arguments->type == 's') {
    short val = 0;
    while (fscanf (f, "%hd", &val) == 1)
      fwrite (&val, sizeof val, 1, stdout);
  }
  else if (arguments->type == 'i') {
    int val;
    while (fscanf (f, "%d", &val) == 1)
      fwrite (&val, sizeof val, 1, stdout);
  }
  return 0;
}

int
process_filename (char *filename, struct arguments *arguments, int complain)
{
  int ok;
  FILE *f;

  if (strcmp (filename, "-") == 0) {
    while (1)
      if (process_file ("standard input", stdin, arguments) == 0)
	return 0;
    return 1;
  }
  if ((f = fopen (filename, "r")) == 0) {
    if (complain)
      fprintf (stderr, "cannot open %s: %s\n", filename, strerror (errno));
    return 0;
  }
  else
    ok = process_file (filename, f, arguments);
  fclose (f);
  return ok;
}

int
main (int argc, char **argv)
{
  char *filename;
  struct arguments arguments;
  int n;

  arguments.type = 'f';
  arguments.file1 = 0;
  arguments.files = 0;
  
  argp_parse (&argp, argc, argv, 0, 0, &arguments);
  if ((filename = arguments.file1))
    process_filename (filename, &arguments, 1);
  if (arguments.files)
    for (filename = arguments.files[n = 0]; arguments.files[n]; filename = arguments.files[++n])
      process_filename (filename, &arguments, 1);
  if (arguments.file1 == 0)
    process_filename ("-", &arguments, 1);
  return 0;
}
