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


#define DEBUG 1
#define STATIC 

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <fftw3.h>
#include <math.h>
#include <unistd.h>
#include <string.h>
#include <argp.h>
#include <time.h>
#include <float.h>
#include "util.h"

#if defined Q_OS_WIN
#include "lin2ms.h"
#endif

static time_t last_time;
static float starttime;

#define SQR(x) (x * x)

const char *argp_program_version = "simspectrum 1.0";
const char *argp_program_bug_address = "<dshuman@usf.edu>";

static char doc[] = "computes the average power spectrum of phrenic bursts"
"\vThe input is binary C floats representing phrenic population activity.\n"
"The output is lines of text on standard output, two values per line\n"
"The first value is frequency in Hz, the second is power\n"
"\nIf the -f option is used, the filtered input is written to a file named \"filtered\" in the current directory\n"
"\nFFT optimization information is read from and written to a file named \"wisdom\" in the current directory.\n"
;

static char args_doc[] = "[FILE...]";

static struct argp_option options[] = {
                           //internally, stepsize is in seconds
  {"stepsize", 's', "N", 0, "set stepsize of input data to N ms (default .5)" },
  {"filter", 'f', 0, 0, "bandpass filter the input .3 to 200 Hz before processing (default is no filter)" },
  {"period", 'p', "N", 0, "power spectrum is of first N seconds of each phrenic burst (default .5)" },
  {"fftsz", 'n', "N", 0, "power spectrum is of first N samples of each phrenic burst (overrides -p)" },
  {"window", 'w', 0, 0, "apply a Hann window to the phrenic burst data after any detrend" },
  {"detrend", 'd', 0, 0, "remove the best-fit line from the phrenic burst data" },
  {"rectify", 'r', 0, 0, "subtract the mean and then take the absolute value of the input data" },
  {"threshold", 't', "N", 0, "controls the placement of the I pulse - larger = later (default .025)" },
  {"spawnnum", 'm', "N", 0, "the I and E pulses are written to ieN.edt" },
  { 0 }
};

struct arguments
{
  char *file1;
  char **files;
  double stepsize;
  int filter;
  double period;
  int fftsz;
  int window;
  int detrend;
  int rectify;
  double threshold;
  int spawnnum;
};
struct arguments *ap;


int plot_number;

static void
plot_buf (float *buf, int bufsize, int lump, char *name)
{
  FILE *f;
  char *namebuf;

  f = fopen ("plot_data", "a");

  asprintf (&namebuf, name, plot_number++);
    
  if (f) {
    int n;

    fprintf (f, "$ DATA=CURVE2D\n%% toplabel=\"%s\"\n", namebuf);
    for (n = 0; n < bufsize; n++)
      fprintf (f, "%g %f\n", (starttime + n * lump) * ap->stepsize, buf[n]);
    fclose (f);
  }
  free (namebuf);
}

static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  /* Get the INPUT argument from `argp_parse', which we
     know is a pointer to our arguments structure. */
  struct arguments *arguments = state->input;

  switch (key)
    {
    case 's':
      arguments->stepsize = atof (arg) / 1000.0;
      break;

    case 'f':
      arguments->filter = 1;
      break;

    case 'p':
      arguments->period = atof (arg);
      break;

    case 'n':
      arguments->fftsz = atoi (arg);
      break;

    case 'w':
      arguments->window = 1;
      break;

    case 'd':
      arguments->detrend = 1;
      break;

    case 'r':
      arguments->rectify = 1;
      break;

    case 't':
      arguments->threshold = atof (arg);
      break;

    case 'm':
      arguments->spawnnum = atoi (arg);
      break;

    case ARGP_KEY_ARG:
      arguments->file1 = arg;
      arguments->files = &state->argv[state->next];
      state->next = state->argc;
      break;

    case ARGP_KEY_END:
      if (state->arg_num == 0 && isatty (0))
	argp_usage (state);
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc };

static fftwf_plan master_plan;
static double *avg;
static double cnt;

void
fit_line (float *y, int n, double *a, double *b)
{
  double sx = 0, sy = 0, sxx = 0, syy = 0, sxy = 0;
  int x;

  for (x = 0; x < n; x++) {
    sx += x;
    sy += y[x];
    sxx += x * x;
    syy += y[x] * y[x];
    sxy += x * y[x];
  }
  *b = (n * sxy - sx * sy) / (n * sxx - sx * sx);
  *a = (sy - *b * sx) / n;
}

double * 
spectrum (float *tdat, struct arguments *arguments)
{
  int fftsz = arguments->fftsz;
  fftwf_plan plan;
  int n;
  static float *fdat;
  double a, b;

  if (arguments->detrend) {
    fit_line (tdat, fftsz, &a, &b);
    for (n = 0; n < fftsz; n++)
      tdat[n] -= a + b * n;
  }

  if (arguments->window) {
    int k;
    for (k = 0; k < fftsz; k++)
      tdat[k] *= .5 * (1 - cos (2 * M_PI * k / (fftsz - 1)));
  }

  if (avg == 0) TCALLOC (avg, fftsz);
  if (fdat == 0) fdat = fftwf_malloc (fftsz * sizeof *fdat);

  //fftw doc claims that this call to fftwf_plan_r2r_1d will be quick
  //because it is the same as master_plan but with different buffers
  plan = fftwf_plan_r2r_1d (fftsz, tdat, fdat, FFTW_R2HC, FFTW_EXHAUSTIVE);

  fftwf_execute (plan);
  fftwf_destroy_plan (plan);

  // layout of fdat is (r = real, i = imaginary):
  // r0, r1, r2, r(n/2), i((n+1)/2-1), ..., i2, i1

  n = 0;
  avg[0] = cnt / (cnt + 1) * avg[n] + fabs (fdat[0]);
  for (n = 1; n < fftsz / 2; n++)
    avg[n] = cnt / (cnt + 1) * avg[n] + (SQR (fdat[n]) + SQR (fdat[fftsz - n])) / (cnt + 1);
  avg[fftsz / 2] = cnt / (cnt + 1) * avg[n] + fabs (fdat[fftsz / 2]);
  cnt++;

  return avg;
}

int
reduce (int n)
{
  while (n % 2 == 0) n /= 2;
  while (n % 3 == 0) n /= 3;
  while (n % 5 == 0) n /= 5;
  while (n % 7 == 0) n /= 7;
  return n;
}

int
good_length (int length)
{
  if (length <= 0)
    return length;
  while (reduce (length) > 1)
    length--;
  return length;
}

typedef struct {
  int next_phase;
  int next_cycle;
  signed char phase;
  char no_cycle;
} Cycle;

typedef struct
{
  double step_sec;
  double bpm_min;
  double bpm_max;
  double cycle_sec_min;
  double cycle_sec_max;
  double cycle_step_min;
  double cycle_step_max;
  double mean;
  int mean_ok;
  int i_step_min;
  int i_step_max;
  int e_step_min;
  int e_step_max;
  int i_step_min0;
  int e_step_min0;
  int phase_step_min;
  int phase_step_max;
  int phase;
  int phase_steps;
  int cycle_steps;
  Cycle *cycle;
  int cycle_alloc;
  Cycle *cycle0;
  int cycle0_count;
  int lumping_ratio;
  int step_index;
  int cycle_index;
  int lumping_ratio_abs;
  int pass;
  int buf_left;
  int orig_bufsize;
} CycleData;

#define RAISE_MIN(a, b) ((a) = (b) > (a) ? (b) : (a))
#define LOWER_MAX(a, b) ((a) = (b) < (a) ? (b) : (a))

static int
phase_continuous (float *buf, int phase_steps, int other_phase_min_steps, int sign, double thr, double mean)
{
  int n, m;
  double sum = 0;

  for (n = 0; n < other_phase_min_steps; n++)
    sum += buf[n];
  sum -= thr * other_phase_min_steps;

  if (sign == 1) {
    for (n = 0, m = other_phase_min_steps; m < phase_steps; n++, m++) {
      if (sum <= 0)
        return 0;
      sum -= buf[n];
      sum += buf[m];
    }
    if (sum <= 0)
      return 0;
  }
  else if (sign == -1) {
    for (n = 0, m = other_phase_min_steps; m < phase_steps; n++, m++) {
      if (sum >= 0)
        return 0;
      sum -= buf[n];
      sum += buf[m];
    }
    if (sum >= 0)
      return 0;
  }
  else DIE;

  return 1;
}

//static int ci = 1117;
//static int ci = 241;
static int ci = -2;

static int lr = 0;


static double
phase_min (float *buf, int phase_steps, int phase_min_steps)
{
  int n, m;
  double sum = 0, min_sum = DBL_MAX;

  for (n = 0; n < phase_min_steps; n++)
    sum += buf[n];

  for (n = 0, m = phase_min_steps; m <= phase_steps; n++, m++) {
    if (sum < min_sum) min_sum = sum;
    sum -= buf[n];
    sum += buf[m];
  }
  return min_sum;
}

static double
phase_max (float *buf, int phase_steps, int phase_min_steps, int *atend)
{
  static int callcount;
  int n, m, uninterrupted = 0, start;
  double sum = 0, max_sum = 0;

  callcount++;

  for (n = 0; n < phase_min_steps; n++)
    sum += buf[n];

  start = 0;
  max_sum = sum;
  for (n = 0, m = phase_min_steps; m <= phase_steps; n++, m++) {
    if (0)
      if (DEBUG && (long) buf == 5325588  && phase_steps == 30)
        printf ("%s line %d: %10d sum %g \n", __FILE__, __LINE__, callcount, sum);
    if (start) {
      if (sum > max_sum) {
        max_sum = sum;
        uninterrupted = 1;
      }
      if (sum < max_sum)
        uninterrupted = 0;
    }
    else {
      if (sum > max_sum)
        start = 1;
      max_sum = sum;
    }
    sum -= buf[n];
    sum += buf[m];
  }
  if (atend)
    *atend = uninterrupted;
  //  if (DEBUG) printf ("%s line %d: %10d sum %g \n", __FILE__, __LINE__, callcount, max_sum);
  return max_sum;
}

int
cycle_continuous (float *buf, int phase_steps, int cycle_steps, double mean, CycleData *c, double sum0, double sum1)
{
  double rate0 = sum0 - phase_steps * mean;
  int phase = rate0 > 0;
  int atend, retval;
  double emax, emin, imax, imin, threshold;
  
  if (phase == 1)
    return 0;
  emax = phase_max (buf,               phase_steps,               c->e_step_min0, &atend);
  emin = phase_min (buf,               phase_steps,               c->e_step_min0);
  imax = phase_max (buf + phase_steps, cycle_steps - phase_steps, c->i_step_min0, 0     );
  imin = phase_min (buf + phase_steps, cycle_steps - phase_steps, c->i_step_min0        );

  threshold = emin + .01 * (imax - emin);
  retval = emax < threshold && emax < imin && !atend;

  if (DEBUG && c->cycle_index == 1) printf ("%s line %d:  cycle_steps %d phase_steps %d lump %d buf %ld\n", __FILE__, __LINE__,
                                            cycle_steps, phase_steps, c->lumping_ratio_abs, (long)buf);
  if (DEBUG && c->cycle_index == 1) printf ("%s line %d:  emax=%g; emin=%g; imax=%g; imin=%g; atend %d retval %d\n", __FILE__, __LINE__, emax, emin, imax, imin, atend, retval);
  
  return retval;
}

STATIC int
cycle_continuous_0 (float *buf, int phase_steps, int cycle_steps, double mean, CycleData *c, double sum0, double sum1)
{
  double rate0 = sum0 - phase_steps * mean;
  int phase = rate0 > 0;
  double mean0 = sum0 / phase_steps;
  double mean1 = sum1 / (cycle_steps - phase_steps);
  int retval = 1;
  if (phase == 1) {
    if (!phase_continuous (buf, phase_steps, c->e_step_min0, 1, (mean1 + mean)/2, mean))
      retval = 0;
    else if (!phase_continuous (buf + phase_steps, cycle_steps - phase_steps, c->i_step_min0, -1, mean, mean))
      retval = 0;
  }
  else {
    if (!phase_continuous (buf, phase_steps, c->i_step_min0, -1, mean + .5 * (mean1 - mean) / 2, mean)) {
      if (DEBUG && c->cycle_index == 44) printf ("%s line %d:  cycle_continuous E phase not\n", __FILE__, __LINE__);
      retval = 0;
    }
    else if (!phase_continuous (buf + phase_steps, cycle_steps - phase_steps, c->e_step_min0, 1, (mean0+mean)/2, mean)) {
      if (DEBUG && c->cycle_index == 44) printf ("%s line %d:  cycle_continuous I phase not\n", __FILE__, __LINE__);
      retval = 0;

    }
  }
  if (DEBUG && c->cycle_index == 44) printf ("%s line %d:  cycle_continuous: %d\n", __FILE__, __LINE__, retval);

  return retval;
}

static double
fr_diff (float *buf, int cycle_steps, int *phase_p, int *phase_steps_p, CycleData *c)
{
  int phase_steps, phase0 = *phase_p, phase_at_max_diff = 0;
  double max_diff = 0, diff;
  int phase_step_min = c->phase_step_min;
  int phase_step_max = MIN (cycle_steps - 1, c->phase_step_max);
  double sum0 = 0, sum1 = 0, end_sum = 0, start_sum = 0;
  double this_cycle_end_rate, next_cycle_start_rate, start_rate, end_rate;
  int n, atend;

  if (0)
  {
    time_t now;
    if ((now = time (0)) > last_time) {
      fprintf (stderr, "%s line %d: fr_diff %ld \r", __FILE__, __LINE__, now);
      last_time = now;
    }
  }

  if (DEBUG && c->cycle_index == 10 && c->cycle0)
    printf ("%s line %d:  lumping_ratio: %d, phase_step_max: %d, phase0: %d, e_step_min: %d, i_step_min: %d\n",
            __FILE__, __LINE__, c->lumping_ratio, phase_step_max, phase0, c->e_step_min, c->i_step_min);

  if (phase0) {
    if (phase0 == 1)
      phase_step_max = MIN (phase_step_max, cycle_steps - c->e_step_min);
    else
      phase_step_max = MIN (phase_step_max, cycle_steps - c->i_step_min);
  }
  else
    phase_step_max = MIN (phase_step_max, cycle_steps - MIN (c->i_step_min, c->e_step_min));

  if (DEBUG && c->cycle_index == 10 && c->cycle0)
    printf ("%s line %d:  lumping_ratio: %d, phase_step_max: %d, phase0: %d, e_step_min: %d, i_step_min: %d\n",
            __FILE__, __LINE__, c->lumping_ratio, phase_step_max, phase0, c->e_step_min, c->i_step_min);

  if (c->cycle0 && c->cycle_index < c->cycle0_count) {

    RAISE_MIN (phase_step_min, (c->cycle0[c->cycle_index].next_phase - 1) * c->lumping_ratio - c->step_index);
    LOWER_MAX (phase_step_max, (c->cycle0[c->cycle_index].next_phase + 2) * c->lumping_ratio - c->step_index);
  }

  if (DEBUG && c->cycle_index == 10 && c->cycle0) {
    printf ("%s line %d:  lumping_ratio: %d, phase_step_max: %d, phase0: %d, e_step_min: %d, i_step_min: %d\n",
            __FILE__, __LINE__, c->lumping_ratio, phase_step_max, phase0, c->e_step_min, c->i_step_min);
    printf ("next_phase: %d, step_index: %d\n", c->cycle0[c->cycle_index].next_phase, c->step_index);
  }

  if (0)
  {
    static int pass;
    if (c->cycle_index == 0 && c->cycle0)
      if (pass++ < 10)
        printf ("%s, cycle_steps %d, next_phase: %d, cycle_index %d, cycle_count %d\n",
                c->cycle0 ? "yes cycle0" : "no cycle0", cycle_steps, c->cycle0[c->cycle_index].next_phase, c->cycle_index, c->cycle0_count);
  }


  for (n = cycle_steps; n < cycle_steps + c->phase_step_min; n++)
    if (n < c->buf_left)
      start_sum += buf[n];
  atend = (n >= c->buf_left);
  //  fprintf (stderr, "n: %d, buf left: %d, atend: %d\n", n, c->buf_left, atend); fflush (stderr);

  for (n = cycle_steps - 1; n >= cycle_steps - c->phase_step_min; n--)
    end_sum += buf[n];
  next_cycle_start_rate = start_sum / c->phase_step_min;
  this_cycle_end_rate   = end_sum   / c->phase_step_min;


#define LOCAL_DERIV 0

  if (LOCAL_DERIV) {
    for (n = 0; n < phase_step_min - c->phase_step_min; n++)
      sum0 += buf[n];
    for (n = phase_step_min; n < phase_step_min + c->phase_step_min; n++)
      sum1 += buf[n];
  }
  else {
    for (n = 0; n < phase_step_min; n++)
      sum0 += buf[n];
    for (n = phase_step_min; n < cycle_steps; n++)
      sum1 += buf[n];
  }

  

  for (phase_steps = phase_step_min; phase_steps <= phase_step_max; phase_steps++) {
    double rate0, rate1, mean;


    //printf ("cycle_steps %d, phase_steps %d, phase0 %d\n", cycle_steps, phase_steps, phase0);

    mean = (sum0 + sum1) / cycle_steps;
    //    printf ("mean: %g, sum0: %g, sum1: %g, cycle_steps: %d\n", mean, sum0, sum1, cycle_steps); /* debug */
//    rate0 = sum0 - phase_steps * mean;
//    rate1 = sum1 - (cycle_steps - phase_steps) * mean;
//    rate0 = (mean + sum0) / (phase_steps + 1);
//    rate1 = (mean + sum1) / (cycle_steps - phase_steps + 1);
//    rate0 = (sum0 - mean * phase_steps) / phase_steps * sqrt(phase_steps);
//    rate1 = (sum1 - mean * (cycle_steps - phase_steps)) / (cycle_steps - phase_steps) * sqrt (cycle_steps - phase_steps);
//    rate0 = sum0 - phase_steps * mean;
//    rate1 = sum1 - (cycle_steps - phase_steps) * mean;

    if (LOCAL_DERIV) {
      rate0 = (start_rate = sum0 / c->phase_step_min) + next_cycle_start_rate;
      rate1 = (end_rate = sum1 / (c->phase_step_min)) + this_cycle_end_rate;
    }
    else {
      rate0 = (start_rate = sum0 / phase_steps) + next_cycle_start_rate;
      rate1 = (end_rate = sum1 / (cycle_steps - phase_steps)) + this_cycle_end_rate;
    }

    if (0)
    if (cycle_steps == 260 && c->cycle_index == 1)
      printf ("cycle_steps: %d, step_sec: %g, c->phase_step_min: %d, phase_steps %d(%d), mean: %g, rate0: %f, ncsr: %g, ss %g, rate1: %f, tcer: %g, diff: %f\n",
              cycle_steps, c->step_sec, c->phase_step_min, phase_steps, c->cycle[0].next_cycle + phase_steps, mean, 
              rate0, next_cycle_start_rate, start_sum, rate1, this_cycle_end_rate, rate1 - rate0);
    
    
    //    printf ("rate0: %f, rate1: %f\n", rate0, rate1);

    diff = phase0 ? phase0 * (rate0 - rate1) : fabs (rate0 - rate1);

    if (sum0 == 0) diff *= 2;

    if (DEBUG && c->cycle_index == 44)
      printf ("%s line %d: lump: %d, phase_steps: %4d, cycle_steps: %4d, rate0: %10.7f, rate1: %10.7f, diff: %10.7f, step_index %6d, orig_bufsize %6d\n",
              __FILE__, __LINE__, c->lumping_ratio_abs, phase_steps, cycle_steps, rate0, rate1, diff, c->step_index, c->orig_bufsize);

    if (atend)
      next_cycle_start_rate = this_cycle_end_rate + (start_rate - end_rate) * .01;
    if (0)
      fprintf (stderr, "%g %g %g %g; %d %d, %d\n", start_rate, end_rate, next_cycle_start_rate, this_cycle_end_rate,
               SGN (start_rate - end_rate), SGN (next_cycle_start_rate - this_cycle_end_rate),
               SGN (start_rate - end_rate) != SGN (next_cycle_start_rate - this_cycle_end_rate));
    if (SGN (start_rate - end_rate) != SGN (next_cycle_start_rate - this_cycle_end_rate))
      diff = 0;

    if (c->cycle_index == ci && c->lumping_ratio == lr)
      //    if (fabs (c->step_index * c->step_sec - 197.012508) < .01)
    {
      double time0, time1, time2;
      //      time0 = 7687531 / 2000. + c->step_index * c->step_sec;
      time0 = c->step_index * c->step_sec;
      time1 = time0 + phase_steps * c->step_sec;
      time2 = time0 + cycle_steps * c->step_sec;
      printf ("diff cycle_index %d, start_sum %g, end_sum %g, step_sec: %g\n", c->cycle_index, start_sum, end_sum, c->step_sec);
      printf ("diff sum0 %g, phase_steps %d, next_cycle_start_rate %g, sum1 %g, cycle_steps %d, this_cycle_end_rate %g\n",
              sum0, phase_steps, next_cycle_start_rate, sum1, cycle_steps, this_cycle_end_rate);
      printf ("diff at %-6g %-6g %-6g: %g %g\n", time0, time1, time2, diff, c->step_sec);
    }
            
    
    if (diff >= max_diff && cycle_continuous (buf, phase_steps, cycle_steps, mean, c, sum0, sum1)) {
      max_diff = diff;
      *phase_steps_p = phase_steps;
      phase_at_max_diff = rate0 > rate1 ? 1 : -1;
      break;
    }
    sum0 += buf[phase_steps];
    sum1 -= buf[phase_steps];
    if (LOCAL_DERIV) {
      sum0 -= buf[phase_steps - phase_step_min];
      sum1 += buf[phase_steps + phase_step_min];
    }
  }

  *phase_p = phase_at_max_diff;

  if (c->cycle_index == ci && c->lumping_ratio == lr)
    printf ("return %g, cycle_steps %d\n", max_diff, cycle_steps);

  return max_diff;
}

typedef struct
{
  int phase;
  int phase_steps;
  int cycle_steps;
} Peak;

static int
max_fr_diff (float *buf, int cycle_index, CycleData *c)
{
  int cycle_steps;
  int phase0 = c->phase;
  int cycle_step_min = c->cycle_step_min;
  int cycle_step_max = c->cycle_step_max;
  double fr_diff_max = 0;
  static Peak *peak;
  static int peak_alloc;
  int peak_count, peakidx = 0;

  if (DEBUG && c->cycle_index == 44) printf ("%s line %d:  max_fr_diff\n", __FILE__, __LINE__);
  if (cycle_index == 0) // the first "cycle" is always a gap.
    return 0;

  if (0)
  {
    time_t now;
    if ((now = time (0)) > last_time) {
      fprintf (stderr, "%s line %d: max_fr_diff %ld \r", __FILE__, __LINE__, now);
      last_time = now;
    }
  }


  c->phase_step_min = (phase0 == 1) ? c->i_step_min : c->e_step_min;
  c->phase_step_max = (phase0 == 1) ? c->i_step_max : c->e_step_max;

  if (c->cycle0 && c->cycle_index < c->cycle0_count) {
    RAISE_MIN (cycle_step_min, (c->cycle0[c->cycle_index].next_cycle - 1) * c->lumping_ratio - c->step_index);
    LOWER_MAX (cycle_step_max, (c->cycle0[c->cycle_index].next_cycle + 1) * c->lumping_ratio - c->step_index);
  }
  LOWER_MAX (cycle_step_max, c->buf_left - c->phase_step_min);

  if (DEBUG && c->cycle_index == 10)
    printf ("%s line %d: lump: %d, phase: %d to %d, cycle: %d to %d, step_index: %d\n",
            __FILE__, __LINE__, c->lumping_ratio_abs, c->phase_step_min, c->phase_step_max, cycle_step_min, cycle_step_max, c->step_index);

  peak_count = 0;
  for (cycle_steps = cycle_step_min; cycle_steps <= cycle_step_max; cycle_steps++) {
    int phase_steps = 0, phase = phase0;
    double fr_diff_val;


    if (0)
    {
      time_t now;
      if ((now = time (0)) > last_time) {
        fprintf (stderr, "%s line %d: cycle_index %d, cycle_steps %d of %d \r", __FILE__, __LINE__, cycle_index, cycle_steps, cycle_step_max);
        last_time = now;
      }
    }

    if (DEBUG && c->cycle_index == 207) printf ("%s line %d:  lumping_ratio: %d, cycle_steps %d\n", __FILE__, __LINE__, c->lumping_ratio, cycle_steps);
    if ((fr_diff_val = fr_diff (buf, cycle_steps, &phase, &phase_steps, c)) >= fr_diff_max
        && (phase0 == 0 || phase == phase0)) {
      if (DEBUG && c->cycle_index == 44) printf ("%s line %d:  fr_diff_val: %g\n", __FILE__, __LINE__, fr_diff_val);
      fr_diff_max = fr_diff_val;
      if (peak_count == 0) {    /* just take largest peak */
        if (peak_alloc == peak_count)
          TREALLOC (peak, ++peak_alloc);
        peakidx = peak_count++;
      }
      peak[peakidx].phase = phase;
      peak[peakidx].phase_steps = phase_steps; 
      peak[peakidx].cycle_steps = cycle_steps;
      
    }
  }

  if (peak_count > 1 && cycle_index >= 2) {
    int prev_cycle_steps = c->cycle[cycle_index - 1].next_cycle -  c->cycle[cycle_index - 2].next_cycle;
    int n, diff, min = c->cycle_step_max, n_at_min = 0;
    for (n = 0; n < peak_count; n++)
      if ((diff = abs (peak[n].cycle_steps - prev_cycle_steps)) < min) {
        min = diff;
        n_at_min = n;
      }
    peakidx = n_at_min;
  }

  if (peak_count > 0) {
    c->phase = peak[peakidx].phase;
    c->phase_steps = peak[peakidx].phase_steps;
    c->cycle_steps = peak[peakidx].cycle_steps;
  }
    
  if (!(cycle_index == 0 || c->phase == phase0)) {
    fprintf (stderr, "cycle_index: %d, lumping_ratio: %d\n", c->cycle_index, c->lumping_ratio);
    DIE;
  }
  return peak_count;
}


void
check_end (CycleData *c, int bufsize, int n)
{
  c->buf_left =  bufsize - n;
  if (bufsize - n < c->cycle_step_max) {
    c->cycle_step_max = bufsize - n;
    if (c->phase == 1) {
      c->e_step_min = MIN (c->e_step_min, MAX (0, c->cycle_step_max - c->i_step_max));
      c->i_step_min = MIN (c->i_step_min, c->cycle_step_max);
    }
    if (c->phase == -1) {
      c->i_step_min = MIN (c->i_step_min, MAX (0, c->cycle_step_max - c->e_step_max));
      c->e_step_min = MIN (c->e_step_min, c->cycle_step_max);
    }
  }
}

void
save_chan (float *v, int count, char *name)
{
  FILE *f = fopen (name, "wb");
  float min = v[0], max = v[0];
  double mid, scale;
  int n;
  short sval;

  for (n = 1; n < count; n++) {
    if (v[n] > max)
      max = v[n];
    if (v[n] < min)
      min = v[n];
  }
  mid = (min + max) / 2;
  scale = 65534. / (max - min);
  for (n = 0; n < count; n++) {
    sval = (v[n] - mid) * scale;
    fwrite (&sval, sizeof sval, 1, f);
  }
  fclose (f);
}

static int
find_neg_zero_xing (float *buf, int bufsize, int earliest_start, CycleData *c)
{
  int fft_start = earliest_start - c->cycle_step_max * 1.1 - 10;
  int fft_end = earliest_start + c->cycle_step_max * 1.1 + 10;
  int fft_len;
  float *env;
  int n, i;
  fftwf_plan plan;
  double period, freq;
  
  if (fft_start < 0)
    fft_start = 0;
  if (fft_end > bufsize)
    fft_end = bufsize;
  //  printf ("fft_start: %d, fft_end: %d, bufsize: %d\n", fft_start, fft_end, bufsize);
  if ((fft_len = fft_end - fft_start) < 3)
    return 0;
  period = fft_len * c->step_sec;
  freq = 1 / period;
  
  env = fftwf_malloc (fft_len * sizeof *env);
  for (i = 0, n = fft_start; n < fft_end; n++, i++)
    env[i] = abs (buf[n]);

  if (DEBUG) plot_buf (env, fft_len, c->lumping_ratio_abs, "zero_xing before %d");

  plan = fftwf_plan_r2r_1d (fft_len, env, env, FFTW_R2HC, FFTW_ESTIMATE);
  fftwf_execute (plan);
  fftwf_destroy_plan (plan);
  env[0] = 0;
  for (n = 1; n < fft_len / 2.0; n++)
    if (n * freq * 60 < c->bpm_min || n * freq * 60 > c->bpm_max)
      env[n] = env[fft_len-n] = 0;
  if (n * 2 == fft_len)  
    env[fft_len/2] = 0;
  plan = fftwf_plan_r2r_1d (fft_len, env, env, FFTW_HC2R, FFTW_ESTIMATE);
  fftwf_execute (plan);
  fftwf_destroy_plan (plan);
  if (DEBUG) plot_buf (env, fft_len, c->lumping_ratio_abs, "zero_xing after %d");
  save_chan (env, fft_len, "fft.chan");
  /* FIXME make sure there's enough space before and after transition */
  if (DEBUG) printf ("%s line %d: lump %d, cycle_index: %d, earliest_start: %d, fft_start: %d, fft_len: %d\n",
                     __FILE__, __LINE__, c->lumping_ratio_abs, c->cycle_index, earliest_start, fft_start, fft_len);

  for (n = earliest_start == fft_start ? 1 : earliest_start - fft_start; n < fft_len; n++)
    if (env[n] < 0 && env[n-1] >= 0)
      break;
  free (env);
  if (n == fft_len && fft_end == bufsize)
    return -1;
  n < fft_len || DIE;           /* there must be a zero crossing */
  //  printf ("fnz: %d\n", n + fft_start);
  return n + fft_start;
}

static void
find_slope_min (float *buf, int bufsize, int es, int start, int dir, int first_up, int *up_p, int *down_p, CycleData *c)
{
  int pre_n, post_n;
  double presum, postsum, diff, prevdiff;
  int n, up = 0, down = 0;
  int lstop, rstop;
  static int presum0, postsum0;

  if (buf == 0) {
    presum0 = postsum0 = 0;
    return;
  }
  pre_n = c->i_step_min;
  post_n = c->e_step_min;
  lstop = MAX (pre_n, es + 1);
  rstop = bufsize - post_n;
  *up_p = *down_p = 0;
  if (lstop < 0 || lstop >= bufsize || rstop < 0 || rstop >= bufsize
      || lstop > start || rstop <= start) {
    //    printf ("fsm: start: %d, lstop: %d, rstop: %d, bufsize: %d\n", start, lstop, rstop, bufsize);
    return;
  }

  if (presum0 + postsum0 == 0) {
    for (n = start - pre_n; n < start; n++)
      presum0 += buf[n];
    for (n = start; n < start + post_n; n++)
      postsum0 += buf[n];
  }
  presum = presum0;
  postsum = postsum0;
  prevdiff = postsum - presum;
  
// |---+---+---|---+---+---|---
//   ^           ^           ^
//   n - pre_n   n           n + post_n

  for (n = start; lstop <= n && n < rstop; n += dir) {
    presum += -dir * buf[n - pre_n];
    presum += dir * buf[n];
    postsum += -dir * buf[n];
    postsum += dir * buf[n + post_n];
    if (0) if (DEBUG && c->cycle_index == 2) printf ("%s line %d: start: %d, n: %d, presum: %g, postsum: %g\n", __FILE__, __LINE__, start, n, presum, postsum);
    diff = postsum - presum;
    //    printf ("fsm: %d(%d)first_up %d, diff %g, prevdiff %g, (%g)\n", n, dir, first_up, diff, prevdiff, c->step_sec);
    /* the two positions being compared are n and n + 1 */
    /* if dir ==  1, n+1 is latest */
    /* if dir == -1, n   is latest */
    if (diff < prevdiff)
      down = dir == 1 ? n : n + 1;
    if (diff > prevdiff) {
      up = dir == 1 ? n + 1 : n;
      if (first_up)
        break;
    }
    if (up && down)
      break;
    prevdiff = diff;
  }
  *up_p = up;
  *down_p = down;
  (up >= 0 && down >= 0) || DIE;
  if (0) if (DEBUG && c->cycle_index == 2) printf ("%s line %d: fsm returns up %d, down %d \n", __FILE__, __LINE__, up, down);
  return;
}

static int
find_cycle_start (float *buf, int bufsize, int earliest_start, CycleData *c)
{
  int l_up, l_down, r_up, r_down;
  int retval = 0;
  int start;
  int es = earliest_start;

  if (c->cycle0)
    start = c->cycle0[c->cycle_index].next_cycle * c->lumping_ratio + c->lumping_ratio / 2;
  else
    start = find_neg_zero_xing (buf, bufsize, earliest_start, c);

  if (start == -1)
    return 0;

  if (DEBUG && c->cycle_index == 0) printf ("%s line %d: find_neg_zero_xing returns %d\n", __FILE__, __LINE__, start);
  
  //  printf ("xstart: %d (%g)\n", start, start * c->step_sec);

  find_slope_min (0, 0, 0, 0, 0, 0, 0, 0, 0);
  find_slope_min (buf, bufsize, es, start, -1, 1, &l_up, &l_down, c);
  //  printf ("find_slope_min l_up %d, l_down %d\n", l_up, l_down);
                             /* LOOK FOR A LOCAL MINIMUM */
  if (l_up && l_down)        /*    --\__/--|             */
    retval =  l_up + 1;      /*            |             */
  else {                     /*            |             */
    find_slope_min (buf, bufsize, es, start,  1, 1, &r_up, &r_down, c);
    //    printf ("find_slope_min take 2 l_up %d, r_up %d, r_down %d\n", l_up, r_up, r_down);
    if (r_up && r_down)      /*            |--\__/--     */
      retval =  r_down;      /*            |             */
    else if (l_up && r_up) { /*            |             */
      //      printf ("l_up and r_up\n");  |
      retval =  l_up + 1;    /*        --\_|_/--         */
    }                        /*            |             */
    else if (l_up) {         /*            |             */
      find_slope_min (buf, bufsize, es, start, -1, 0, &l_up, &l_down, c);
      //      printf ("find_slope_min take 2 l_up %d, l_down %d\n", l_up, l_down);
      if (l_up && l_down)    /*            |             */
        retval =  l_up + 1;  /* --\__/--\__|___________  */
    }                        /*            |             */
    else if (r_up) {         /*            |             */
      find_slope_min (buf, bufsize, es, start,  1, 0, &r_up, &r_down, c);
      //      printf ("find_slope_min take 2 r_up %d, r_down %d\n", r_up, r_down);
      if (r_up && r_down)    /*            |             */
        retval =  r_down;    /*  __________|___/--\__/-- */
    }
  }
  //  printf ("find_cycle_start returns %d, earliest start %d\n", retval, earliest_start);
  return retval;
}

static int zero_e_phase;

static void
maybe_clip (float *buf, int bufsize, CycleData *c)
{
  int n, ingap = 0, start = 0, count = 0;

  for (n = 0; n < bufsize; n++)
    if (!ingap && buf[n] == 0)
        start = n, ingap = 1;
    else if (ingap && buf[n] != 0)
      count += n - start >= c->e_step_min0, ingap = 0;
  
  if (count > 0 && bufsize / count < c->cycle_step_max)
    zero_e_phase = 1;
  return;

    for (n = 0; n < bufsize; n++)
      if (buf[n] > 1)
        buf[n] = 1;
}

static void
phase_adjust (float *buf, CycleData *c)
{
  double emax, imax, threshold;
  int n;
  emax = phase_max (buf,                  c->phase_steps,                  c->e_step_min0, 0);
  imax = phase_max (buf + c->phase_steps, c->cycle_steps - c->phase_steps, c->i_step_min0, 0);

  emax /= c->e_step_min0;
  imax /= c->i_step_min0;
  threshold = emax + ap->threshold * (imax - emax);
  n = c->phase_steps - 1;
  while (n < c->cycle_steps && buf[n] < threshold)
    n++;
  
  c->phase_steps = n;

  while (buf[c->cycle_steps - 2] > buf[c->cycle_steps - 1])
    c->cycle_steps--;
}

static void
get_cycles (float *buf, int bufsize, CycleData *c)
{
  int n;
  static int pass;

  pass++;

  maybe_clip (buf, bufsize, c);

  if (DEBUG) {
    plot_buf (buf, bufsize, c->lumping_ratio_abs, "get_cycles %d");
    printf ("get_cycles begin\n");
  }

  for (n = 0; n < bufsize; n += c->cycle_steps) {
    int i;
    if (0)
    {
      static time_t last_time;
      time_t now;
      if ((now = time(0)) > last_time) {
        printf ("%d of %d (%lld%%)\r", n, bufsize, n * 100LL / bufsize);
        fflush (stdout);
        last_time = now;
      }
    }
    if (c->cycle0 && c->cycle_index >= c->cycle0_count) { 
      //      printf ("reached previous limit at cycle index %d, step_sec %g\n", c->cycle_index, c->step_sec);
      return;
    }

    check_end (c, bufsize, n);

    if (c->cycle0)
      c->phase = c->cycle0[c->cycle_index].phase;
    
    if (DEBUG && c->cycle_index > 200)
      printf ("%s line %d cycle_index %d n %d bufsize %d, cycle_step_max %g\n",
              __FILE__, __LINE__, c->cycle_index, n, bufsize, c->cycle_step_max);
    if (!max_fr_diff (buf + n, c->cycle_index, c)) {
      int cycle_start;
      c->cycle[c->cycle_index].no_cycle = 1;
      if (n + c->cycle_step_max > bufsize)
        return;
      cycle_start = find_cycle_start (buf, bufsize, n, c);
      if (DEBUG && c->cycle_index == 44) printf ("%s line %d: lump %d: cycle_start %g\n",
                                                __FILE__, __LINE__, c->lumping_ratio_abs, starttime + cycle_start * c->lumping_ratio_abs);
      if (c->cycle0 && c->cycle0[c->cycle_index].no_cycle == 0) {
        if (0)
        printf ("cycle %d not confirmed, phase %g, end %g\n", c->cycle_index,
                c->cycle0[c->cycle_index].next_phase * c->lumping_ratio * c->step_sec,
                c->cycle0[c->cycle_index].next_cycle * c->lumping_ratio * c->step_sec
                );
      }
      if (cycle_start) {
        c->cycle_steps = cycle_start - n;
        c->phase_steps = 0;
      }
      else {
        if (0) {
          if (c->cycle0) {
            if (c->cycle_index < c->cycle0_count - 1)
              printf ("cycles %d to %d not confirmed\n", c->cycle_index, c->cycle0_count - 1);
          }
          else
            printf ("no cycles found after %g seconds\n", n * c->step_sec);
        }
        return;
      }
    }
    else {
      if (0) if (DEBUG && c->cycle_index == 2) printf ("%s line %d: max_fr_diff returned true\n", __FILE__, __LINE__);
      c->cycle[c->cycle_index].no_cycle = 0;
    }
    if (c->step_index + c->cycle_steps >= c->orig_bufsize) {
      c->cycle[c->cycle_index].no_cycle = 1;
      return;
    }
    c->cycle[c->cycle_index].phase = c->phase;

    {
      double sum = 0;
      n + c->cycle_steps <= bufsize || DIE;
      for (i = 0; i < c->cycle_steps; i++)
        sum += buf[n + i];
      c->mean = sum / c->cycle_steps;
      c->mean_ok = 1;
    }
    (n >= 0 && n + c->phase_steps < bufsize && n + c->cycle_steps < bufsize) || DIE;

    if (c->lumping_ratio_abs == 1)
      phase_adjust (buf + n, c);

    if (0)
    if (zero_e_phase && c->lumping_ratio_abs == 1) {
      int i;
      for (i = n + c->phase_steps; i > n; i--)
        if (buf[i] == 0) {
          c->phase_steps = i - n;
          break;
        }
    }

    c->cycle[c->cycle_index].next_phase = c->step_index + c->phase_steps;
    c->cycle[c->cycle_index].next_cycle = c->step_index + c->cycle_steps;

    if (DEBUG && c->cycle_index == 1) printf ("%s line %d: lump %d: step_index %d, phase_steps %d, cycle_steps %d, next_cycle %d, no_cycle %d\n",
                                              __FILE__, __LINE__, c->lumping_ratio_abs,
                                              c->step_index, c->phase_steps,  c->cycle_steps,
                                              c->cycle[c->cycle_index].next_cycle, c->cycle[c->cycle_index].no_cycle);

    if (0)
      if (pass == 1 && c->cycle_index == 0)
        printf ("c->cycle[%d].next_cycle: %d, step_index %d\n", c->cycle_index, c->cycle[c->cycle_index].next_cycle, c->step_index);

    if (DEBUG) printf ("get_cycles cycle index %d: %d steps out of %d\n", c->cycle_index, c->cycle_steps, bufsize);

    c->step_index += c->cycle_steps;
    if (++c->cycle_index == c->cycle_alloc)
      TREALLOC (c->cycle, c->cycle_alloc += 100);

    
  }

  printf ("reached previous limit at cycle index %d, step_sec %g\n", c->cycle_index, c->step_sec);
  return;
}

float *
lump (float *buf, int bufsize, int lumping_ratio)
{
  float *lbuf;
  int n;

  if (lumping_ratio == 1)
    return buf;
  TCALLOC (lbuf, bufsize / lumping_ratio + 1);
  for (n = 0; n < bufsize; n++)
    lbuf[n / lumping_ratio] += buf[n];
  return lbuf;
}

static void
init_cycle_data (CycleData *c)
{
  c->mean_ok = 0;
  c->mean = 0;
  c->phase = -1;
  c->phase_steps = 0;
  c->cycle_steps = 0;
  c->bpm_min = 5;
  c->bpm_max = 60;
  c->cycle_sec_min = 60 / c->bpm_max;
  c->cycle_sec_max = 60 / c->bpm_min;
  c->cycle_step_min = c->cycle_sec_min / c->step_sec;
  c->cycle_step_max = c->cycle_sec_max / c->step_sec;
  c->i_step_min0 = c->i_step_min = floor (c->cycle_step_min / 4);
  c->i_step_max = ceil (3 * c->cycle_step_max / 3);
  c->e_step_min0 = c->e_step_min = floor (c->cycle_step_min / 4);
  c->e_step_max = ceil (3 * c->cycle_step_max / 3);
  c->cycle_index = 0;
  c->step_index = 0;
}

static inline int
next_up (float *buf, int last)
{
  int n;
  for (n = last - 1; n >= 0; n--) {
    if (buf[n] > buf[last])
      return 1;
    if (buf[n] < buf[last])
      return 0;
  }
  return 0;
}

static Cycle *
get_envelope (float **bufp, int bufsize, struct arguments *arguments, int *cycle_countp)
{
  float *buf = *bufp;
  CycleData c;
  int lumping_ratio, prev_lumping_ratio;
  double step = arguments->stepsize;
  double starting_resolution = .05;
  int pass_ratio = 32;

  if (DEBUG)
    plot_buf (buf, bufsize, 1, "data %d");

  for (lumping_ratio = 1; lumping_ratio * step < starting_resolution; lumping_ratio *= 2)
    ;

  if (starting_resolution / (lumping_ratio / 2 * step) < (lumping_ratio * step) / starting_resolution)
    lumping_ratio /= 2;

  c.cycle0 = 0;
  //  printf ("CYCLE0 = 0\n");
  
  c.cycle0_count = 0;
  c.lumping_ratio = 0;
  c.pass = 0;
  while (1) {
    int n, lbufsize;
    float *lbuf = lump (buf, bufsize, lumping_ratio);
    lbufsize = bufsize / lumping_ratio;
    c.step_sec = step * lumping_ratio;
    c.lumping_ratio_abs = lumping_ratio;
    init_cycle_data (&c);
    c.orig_bufsize = lbufsize;
    TREALLOC (lbuf, lbufsize += c.cycle_step_min);
    for (n = c.orig_bufsize; n < lbufsize; n++)
      lbuf[n] = 0;
    if (lumping_ratio == 1)
      *bufp = lbuf;
    if (0)
    printf ("lumping_ratio: %d, lbufsize: %d, residue %d, last bin %f(%d), first bin %f\n",
            lumping_ratio, lbufsize, bufsize % lumping_ratio, lbuf[lbufsize - 1], lbufsize - 1, lbuf[0]);
    if (c.cycle0)
      TMALLOC (c.cycle, c.cycle_alloc = c.cycle0_count);
    else
      TMALLOC (c.cycle, lbufsize / 2);
    //    printf ("calling get_cycles\n"); fflush (stdout);
    if (DEBUG) printf ("%s line %d: get_cycles lump %d\n", __FILE__, __LINE__, lumping_ratio);
    get_cycles (lbuf, lbufsize, &c);
    c.pass++;
    //    printf ("get_cycles returned: %d cycles\n", c.cycle_index); fflush (stdout);
    if (lumping_ratio > 1)
      free (lbuf);
    if (lumping_ratio == 1)
      break;
    free (c.cycle0);
    c.cycle0 = c.cycle;
    //    printf ("CYCLE0 = CYCLE\n");
    c.cycle0_count = c.cycle_index;
    //    printf ("\n ************* cycle_count %d\n", c.cycle_index);
    prev_lumping_ratio = lumping_ratio;
    if (0)                      /* debug */
    if (bufsize % lumping_ratio != 0)
      c.cycle0_count--;
    if (lumping_ratio / pass_ratio >= pass_ratio)
      lumping_ratio /= pass_ratio;
    else if (lumping_ratio / pass_ratio <= 1)
      lumping_ratio = 1;
    else {
      int n = 1;
      while (lumping_ratio / n > n) 
        n *= 2;
      lumping_ratio /= n;
    }
    c.lumping_ratio = prev_lumping_ratio / lumping_ratio;
  }
  free (c.cycle0);
  *cycle_countp = c.cycle_index;
  return c.cycle;
}

static int
process_file (char *filename, FILE *f, struct arguments *arguments)
{
  float *buf = 0, *filtered = 0;
  int bufalloc = 0, bufsize, fftsz = arguments->fftsz;
  int remaining, readcnt, n, filefloats;
  int sofar = 0;

  fread (&starttime, sizeof starttime, 1, f) == 1 || DIE;
  fprintf (stderr, "starttime: %g\n", starttime);

  TMALLOC (buf, bufalloc = 1024*1024);

  remaining = bufalloc - sofar;
  while ((readcnt = fread (buf + sofar, sizeof *buf, remaining, f)) == remaining) {
    sofar += readcnt;
    TREALLOC (buf, bufalloc += 1024*1024);
    remaining = bufalloc - sofar;
  }
  sofar += readcnt;
  filefloats = sofar;
  TREALLOC (buf, filefloats);

  if (filefloats == 0)
    exit (0);

  if (arguments->period) {
    fprintf (stderr, "fftsz %d\n", arguments->fftsz);
    fprintf (stderr, "frequency resolution: %f\n", 1 / arguments->period);
    bufsize = good_length (filefloats);
    filtered = fftwf_malloc (bufsize * sizeof *filtered);

    if (arguments->filter)
      {
        fftwf_plan plan;
        double period = bufsize * arguments->stepsize;
        double freq = 1 / period;

        plan = fftwf_plan_r2r_1d (bufsize, buf, filtered, FFTW_R2HC, FFTW_ESTIMATE);
        fftwf_execute (plan);
        fftwf_destroy_plan (plan);

        n = 0;
        if (n * freq < .3 || n * freq > 200)
          filtered[n] = 0;
        for (n = 1; n < bufsize / 2.0; n++)
          if (n * freq < .3 || n * freq > 200)
            filtered[n] = filtered[bufsize-n] = 0;
        n = bufsize / 2;
        if (n * 2 == bufsize && (n * freq < .3 || n * freq > 200))
          filtered[bufsize/2] = 0;

        plan = fftwf_plan_r2r_1d (bufsize, filtered, filtered, FFTW_HC2R, FFTW_ESTIMATE);
        fftwf_execute (plan);
        fftwf_destroy_plan (plan);

        for (n = 0; n < bufsize; n++)
          filtered[n] /= bufsize;
        {
          FILE *f;
          int n;
          (f = fopen ("filtered", "w")) || DIE;
          for (n = 0; n < bufsize; n++)
            fprintf (f, "%g\n", filtered[n]);
          fclose (f);
        }
      }
    else memcpy (filtered, buf, bufsize * sizeof *filtered);
  }
  else
    bufsize = filefloats;

  fprintf (stderr, "filefloats: %d, bufsize %d\n", filefloats, bufsize);
  if (arguments->rectify)
  {
    double sum = 0, mean;
    for (n = 0; n < filefloats; n++)
      sum += buf[n];
    mean = sum / filefloats;
    for (n = 0; n < filefloats; n++)
      buf[n] = fabs (buf[n] - mean);
  }

  {
    int breaths = 0, n, cycle_count;
    double *avg = 0;
    Cycle *cycle = get_envelope (&buf, filefloats, arguments, &cycle_count);
    double wss = 0;
    
    {
      FILE *f;
      int cycle_start = 0;
      char *filename;
      if (asprintf (&filename, "ie%d.edt", arguments->spawnnum) == -1) exit (1);
      if ((f = fopen (filename, "w"))) {
        int t0 = (int)starttime * 10000LL / floor (1 / arguments->stepsize + .5);
        fprintf (f, "   33   3333333\n");
        fprintf (f, "   33   3333333\n");
        for (n = 0; n < cycle_count; n++) {
          if (cycle[n].no_cycle == 0) {
            int i, e;
            if (cycle_start) {
              fprintf (f, "%5d%10u\n", 98, t0 + (unsigned)floor (cycle_start * arguments->stepsize * 10000 + .5));
              cycle_start = 0;
            }
            fprintf (f, "%5d%10u\n", 97, i = t0 + (unsigned) floor (cycle[n].next_phase * arguments->stepsize * 10000 + .5));
            fprintf (f, "%5d%10u\n", 98, e = t0 + (unsigned) floor (cycle[n].next_cycle * arguments->stepsize * 10000 + .5));
            if (DEBUG && n == 44) printf ("%s line %d: I: %d, E: %d\n", __FILE__, __LINE__, i, e);
          }
          else cycle_start = cycle[n].next_cycle;
        }
        fclose (f);
      }
      free (filename);
    }
    if (!arguments->period)
      return 0;

    if (arguments->window) {
      int k;
      double ck;
      for (k = 0; k < fftsz; k++) {
	ck = .5 * (1 - cos (2 * M_PI * k / (fftsz - 1)));
	wss += ck * ck;
      }
      wss *= fftsz * fftsz;
    }
    else wss = fftsz * fftsz;
    
    for (n = 0; n < cycle_count; n++)
      if (cycle[n].no_cycle == 0)
        avg = spectrum (filtered + cycle[n].next_phase, arguments), breaths++;

    fprintf (stderr, "%d breaths\n", breaths);
    if (breaths)
    {
      int k;
      for (k = 0; k < fftsz; k++)
	avg[k] /= wss;
    }
    
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

  ap = &arguments;
  
  if (DEBUG) {
    FILE *f = fopen ("plot_data", "w");
    if (f) fclose (f);
  }

  arguments.stepsize = .5e-3;
  arguments.filter = 0;
  arguments.period = .5;
  arguments.fftsz = 0;
  arguments.window = 0;
  arguments.detrend = 0;
  arguments.rectify = 0;
  arguments.file1 = 0;
  arguments.files = 0;
  arguments.threshold = .025;
  arguments.spawnnum = 0;
  
  argp_parse (&argp, argc, argv, 0, 0, &arguments);

  if (arguments.fftsz)
    arguments.period = arguments.fftsz * arguments.stepsize;
  else
    arguments.fftsz = good_length ((int)(arguments.period / arguments.stepsize));

  {
    FILE *w;
    if ((w = fopen ("wisdom", "rb")) != 0) {
      fftwf_import_wisdom_from_file (w);
      fclose (w);
    }
  }
  {
    float *buf1, *buf2;
    int fftsz = arguments.fftsz;
    TMALLOC (buf1, fftsz);
    TMALLOC (buf2, fftsz);
    master_plan = fftwf_plan_r2r_1d (arguments.fftsz, buf1, buf2, FFTW_R2HC, FFTW_EXHAUSTIVE | FFTW_UNALIGNED);
    free (buf1);
    free (buf2);
  }
  {
    FILE *w;
    (w = fopen ("wisdom", "wb")) || DIE;
    fftwf_export_wisdom_to_file (w);
    fclose (w);
  }


  if ((filename = arguments.file1))
    process_filename (filename, &arguments, 1);
  if (arguments.files)
    for (filename = arguments.files[n = 0]; arguments.files[n]; filename = arguments.files[++n])
      process_filename (filename, &arguments, 1);
  if (arguments.file1 == 0)
    process_filename ("-", &arguments, 1);

  if (arguments.period)
  {
    int n, fftsz = arguments.fftsz;
    double period = fftsz * arguments.stepsize;
    double freq = 1 / period;
    for (n = 0; n <= fftsz / 2; n++)
      if (avg)
        printf ("%g %g\n", n * freq, avg[n]);
  }

  return 0;
}
