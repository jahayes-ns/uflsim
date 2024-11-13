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

#if defined Q_OS_WIN
#include "lin2ms.h"
#endif

const char *argp_program_version = "simpickwave 1.0";
const char *argp_program_bug_address = "<dshuman@hsc.usf.edu>";
static char doc[] = "picks a waveform out of a specified set of wave.* files"
"\vThe wave number for the -w option is the position in the wave.* file.\n"
"The first position is 1, not 0.\n";

static char args_doc[] = "[FILE...]";

static struct argp_option options[] = {
  {"wave",  'w', "N",      0,  "output data for wave number N" },
  {"spawn",  'n', "N",      0,  "take input from wave.* files with spawn number N" },
  {"spikes",  's', 0,      0,  "pick spike data instead of waveform data" },
  { 0 }
};

struct arguments
{
  char *file1;
  char **files;
  int wave;
  int spawn;
  int spikes;
};

static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  /* Get the INPUT argument from `argp_parse', which we
     know is a pointer to our arguments structure. */
  struct arguments *arguments = state->input;

  switch (key)
    {
    case 's':
      arguments->spikes = 1;
      break;

    case 'n':
      arguments->spawn = atoi (arg);
      break;

    case 'w':
      arguments->wave = atoi (arg);
      break;

    case ARGP_KEY_ARG:
      arguments->file1 = arg;
      arguments->files = &state->argv[state->next];
      state->next = state->argc;
      break;

    case ARGP_KEY_END:
      if (state->arg_num == 0 && arguments->spawn == -1 && isatty (0))
	argp_usage (state);
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc };

static int
process_file (char *filename, FILE *f, int wave, int spikes)
{
  static char *inbuf = 0;
  static size_t len = 0;
  int numsteps, numwaves, n, badfile = 0, step_n, spike;
  double stepsize, val;

  if (getline (&inbuf, &len, f) == -1
      || sscanf (inbuf, "%d %lf", &numsteps, &stepsize) != 2
      || getline (&inbuf, &len, f) == -1
      || sscanf (inbuf, "%d", &numwaves) != 1)
    badfile = 1;
  if (!badfile)
    for (n = 0; n < numwaves; n++)
      if (getline (&inbuf, &len, f) == -1)
	badfile = 1;
  for (step_n = 0; step_n < numsteps && !badfile; step_n++)
    for (n = 0; n < numwaves && !badfile; n++)
      if (getline (&inbuf, &len, f) == -1
	  || sscanf (inbuf, "%lf %d", &val, &spike) != 2)
	badfile = 1;
      else if (n + 1 == wave) {
	if (spikes)
	  printf ("%d\n", spike);
	else
	  printf ("%g\n", val);
      }
  if (badfile) {
    fprintf (stderr, "%s is incomplete or is not a valid wave.* file\n", filename);
    return 0;
  }
  return 1;
}

int
process_filename (char *filename, struct arguments *arguments, int complain)
{
  int ok;
  FILE *f;

  if (strcmp (filename, "-") == 0) {
    while (1)
      if (process_file ("standard input", stdin, arguments->wave, arguments->spikes) == 0)
	return 0;
    return 1;
  }
  if ((f = fopen (filename, "r")) == 0) {
    if (complain)
      fprintf (stderr, "cannot open %s: %s\n", filename, strerror (errno));
    return 0;
  }
  else
    ok = process_file (filename, f, arguments->wave, arguments->spikes);
  fclose (f);
  return ok;
}

int
main (int argc, char **argv)
{
  char *filename;
  struct arguments arguments;
  int n;

  arguments.wave = 1;
  arguments.spikes = 0;
  arguments.spawn = -1;
  arguments.file1 = 0;
  arguments.files = 0;
  
  argp_parse (&argp, argc, argv, 0, 0, &arguments);
  if (arguments.spawn >= 0) {
    int n, ok = 1;
    for (n = 0; n <= 9999 && ok; n++) {
      if (asprintf (&filename, "wave.%02d.%04d", arguments.spawn, n) == -1) exit (1);
      ok = process_filename (filename, &arguments, n == 0);
      free (filename);
    }
  }
  if ((filename = arguments.file1))
    process_filename (filename, &arguments, 1);
  if (arguments.files)
    for (filename = arguments.files[n = 0]; arguments.files[n]; filename = arguments.files[++n])
      process_filename (filename, &arguments, 1);
  if (arguments.spawn == -1 && arguments.file1 == 0)
    process_filename ("-", &arguments, 1);
  return 0;
}
