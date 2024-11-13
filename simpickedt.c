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
#include <limits.h>

#if HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

const char *argp_program_version = "simpickedt 1.0";
const char *argp_program_bug_address = "<dshumanr@usf.edu>";

static char doc[] = "picks a waveform out of specified .bdt or .edt file(s)"
"\vThe output is text, one value per line, on standard output";

static char args_doc[] = "[FILE...]";

static struct argp_option options[] = {
  {"offset", 'o', "N", 0, "add N to each analog value" },
  {"analog_id", 'a', "N", 0, "output data for analog id N" },
  {"spike_id",  's', "N", 0, "output data for spike id N" },
  {"ids",     'c',  0,  0, "output all ids on stderr" },
  {"starttime", 't',  0,  0, "first line of output is start time in units of the sample period" },
  { 0 }
};

struct arguments
{
  char *file1;
  char **files;
  int analog_id;
  int spike_id;
  int ids;
  int offset;
  int starttime;
};

static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  /* Get the INPUT argument from `argp_parse', which we
     know is a pointer to our arguments structure. */
  struct arguments *arguments = state->input;

  switch (key)
    {
    case 'o':
      arguments->offset = atoi (arg);
      break;

    case 'a':
      arguments->analog_id = atoi (arg);
      break;

    case 's':
      arguments->spike_id = atoi (arg);
      break;

    case 'c':
      arguments->ids = 1;
      break;

    case 't':
      arguments->starttime = 1;
      break;

    case ARGP_KEY_ARG:
      arguments->file1 = arg;
      arguments->files = &state->argv[state->next];
      state->next = state->argc;
      break;

    case ARGP_KEY_END:
      if ((state->arg_num == 0 && isatty (0)) || (arguments->analog_id && arguments->spike_id))
	argp_usage (state);
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc };

static char analog_ids[25];
static char spike_ids[1000];

static int
process_file (char *filename, FILE *f, struct arguments *arguments)
{
  static char *inbuf = 0;
  static size_t len = 0;
  int badfile = 0;
  int last_time = INT_MIN, code_idx = 0, line_idx = 0, start_time = 0;

  if (getline (&inbuf, &len, f) == -1
      || (getline (&inbuf, &len, f) == -1))
    badfile = 1;
  if (!badfile)
    while (getline (&inbuf, &len, f) != -1) {
      unsigned code, time;
      if (sscanf (inbuf + 5, "%u", &time) != 1
	  || (inbuf[5] = 0, sscanf (inbuf, "%u", &code) == -1)) {
	badfile = 1; break;
      }
      if (line_idx == 0)
        start_time = time;
      if (last_time == INT_MIN)
        last_time = time - 1;
      if (code > 1000 && code < 4096) {
	fprintf (stderr, "bad code: %u\n", code);
	badfile = 1; break;
      }
      if (arguments->analog_id && (int)(code / 4096) == arguments->analog_id) {
	static int interval, last_code;
#       define VAL(code) ((((code & 0x800) == 0) ? code & 0x7ff : code | 0xfffff000u) + arguments->offset)
        if (code_idx == 1) {
          if ((interval = time - last_time) <= 0) {
            fprintf (stderr, "error: first interval is <= 0\n");
            badfile = 1; break;
          }
          fprintf (stderr, "interval is %d ticks\n", interval); 
          if (arguments->starttime)
            printf ("%g\n", (double)start_time / interval);
          printf ("%d\n", VAL (last_code));
        }
        if (code_idx > 0)
          printf ("%d\n", VAL (code));
        if (code_idx > 1 && (int)time - last_time != interval) {
          fprintf (stderr, "error: intervals vary: %d - %d != %d\n", time, last_time, interval);
          badfile = 1; break;
        }
        last_time = time;
        last_code = code;
        code_idx++;
      }
      else if ((int)code == arguments->spike_id) {
	int n;
        if ((int)time <= last_time) {
          fprintf (stderr, "error: spike code %u time %u is not later than previous spike at time %d\n",
                   code, time, last_time);
          badfile = 1; break;
        }
        for (n = last_time + 1; n < (int)time; n++)
          printf ("0\n");
        printf ("1\n");
	last_time = time;
      }
      if (!badfile && arguments->ids) {
	if (code / 4096 > 0)
	  analog_ids[code/4096] = 1;
	else
	  spike_ids[code] = 1;
      }
      line_idx++;
    }
  if (badfile) {
    fprintf (stderr, "%s is incomplete or is not a valid .bdt/.edt file\n", filename);
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

  arguments.analog_id = 0;
  arguments.spike_id = 0;
  arguments.file1 = 0;
  arguments.files = 0;
  arguments.ids = 0;
  arguments.offset = 0;
  arguments.starttime = 0;
  
  argp_parse (&argp, argc, argv, 0, 0, &arguments);
  if ((filename = arguments.file1))
    process_filename (filename, &arguments, 1);
  if (arguments.files)
    for (filename = arguments.files[n = 0]; arguments.files[n]; filename = arguments.files[++n])
      process_filename (filename, &arguments, 1);
  if (arguments.file1 == 0)
    process_filename ("-", &arguments, 1);
  if (arguments.ids) {
    int n;
    fprintf (stderr, "analog ids:");
    for (n = 1; n <= 24; n++)
      if (analog_ids[n])
	fprintf (stderr, " %d", n);
    fprintf (stderr, "\n");
    fprintf (stderr, "spike ids:");
    for (n = 1; n < 1000; n++)
      if (spike_ids[n])
	fprintf (stderr, " %d", n);
    fprintf (stderr, "\n");
  }
  return 0;
}
