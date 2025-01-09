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
#include <stdbool.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <error.h>
#include <errno.h>
#include <regex.h>
#include <assert.h>
#include <time.h>
#if defined __linux__
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif
#include "simulator.h"
#include "fileio.h"
#include "simulator_hash.h"
#include "lung.h"
#include "expr.h"
#include "inode.h"
#include "lin2ms.h"
#include "wavemarkers.h"
#include "simrun_wrap.h"
#include "common_def.h"

#ifdef __linux__
extern int sock_fd;
extern int sock_fdout;
#elif defined WIN32
extern SOCKET sock_fd;
extern SOCKET sock_fdout;
#else
#pragma message("WARNING: Unknown environment configuration, not Linux or Windows")
#endif
extern bool Debug;
extern int use_socket;
extern int write_waves;
extern int write_bdt;
extern int write_analog;
extern int write_smr;
extern int write_smr_wave;
extern int noWaveFiles;
extern int isedt;
extern char *fmt;
extern double dt_step;
extern char simmsg[];
extern char inPath[];
extern char outPath[];
extern void destroy_cmd_socket();
extern void destroy_view_socket();
extern void add_IandE();
extern int haveLearn;
time_t global_last_time;
extern int learnCPop[MAX_INODES];
extern int numCPop;
extern int learnFPop[MAX_INODES];
extern int numFPop;

/*
; exp(-.5/2)
        0.77880078307140486825
; exp(-.5/1.5)
        0.71653131057378925043
*/

extern int st (float eq, float dcs);

static const double g_L = 2.8;
static const double E_Na = 50;
static const double E_L = -65;

char wave_buf[1024*64];

int have_cmd_socket()
{
#ifdef __linux__
  if (sock_fd)
#else
  if (sock_fd != INVALID_SOCKET)
#endif
    return 1;
  else
    return 0;
}

int have_data_socket()
{
#ifdef __linux__
  if (sock_fdout)
#else
  if (sock_fdout != INVALID_SOCKET)
#endif
    return 1;
  else
    return 0;
}

// Given current afferent input value for a fiber pop, return
// the corresponding probabilty using linear interpolation
// on the value to probability lookup table for this population.
// Values outside the range return 0.
static double interpolate(FiberPop *fp, double curr_val)
{
   int num = fp->num_aff - 1;
   int idx;
   double result = 0.0; 

   for (idx = 0; idx < num; ++idx)
   {
      if (curr_val >= fp->aff_val[idx] && curr_val < fp->aff_val[idx+1])
      {
         result = (fp->aff_prob[idx+1] - fp->aff_prob[idx]) 
                  / (fp->aff_val[idx+1] - fp->aff_val[idx])
                  * (curr_val - fp->aff_val[idx]) + fp->aff_prob[idx];
         break;
      }
   }
   return result;
}


// Send the simulation plot results directly to simviewer.
static void send_wave()
{
  size_t to_send;
  size_t sent = 0, total_sent = 0;
  char* ptr = wave_buf;

  if (have_data_socket())
  {
    to_send = strlen(wave_buf);
    while (total_sent != to_send)
    {
#ifdef __linux__
      sent = send(sock_fdout, (void*)ptr,to_send,0); // blocking i/o
      if (sent == -1 && errno == EPIPE) // lost connection
      {
         fprintf(stdout,"SIMRUN: Connection to simviewer lost.\n");
         fflush(stdout);
         destroy_view_socket();
         return;
      }
      else if (sent == -1)
      {
         perror("SIMRUN");
         continue;
      }
      else
      {
         total_sent += sent;
         ptr += sent;
      }
#else
      sent = send(sock_fdout, (void*)ptr,to_send,0);
      if (sent == SOCKET_ERROR)
      {
         int sockerr = WSAGetLastError();
         if (sockerr == WSAECONNRESET || sockerr == WSAECONNABORTED)
            fprintf(stdout,"Connection to simviewer lost.\n");
         else
            fprintf(stdout,"Socket error is %d\n",sockerr);
         fflush(stdout);
         destroy_view_socket(); // most errors are fatal
         return;
      }
      else
      {
         total_sent += sent;
         ptr += sent;
      }
#endif
    }
    total_sent += sent;
    ptr += sent;
  }
  fflush(stdout);
}

// utilty to send text back to simbuild
void sendMsg(char *msg)
{
   send(sock_fd,msg,strlen(msg),0); 
}


// If simviewer (or other program) is getting wave info from a socket, wait
// until that side tells us it has got all of the packets. We do this by
// sending an EOF msg, then wait for the other side to tell us it got it
// on the same socket we are sending to.
// This can hang here forever in some cases.
void waitForDone()
{
  int got;
  char in[1];

  if (have_data_socket())
  {
    wave_buf[0] = MSG_EOF;
    wave_buf[1] = 0;
    send_wave();
    if (have_data_socket())
    {
      got = recv(sock_fdout,in,sizeof(in),0); // block until it shows up
      if (got > 0 && (unsigned char) in[0] != MSG_EOF)
        printf("got unexpected msg from simviewer, still done\n");
    }
  }
  fflush(stdout);
}

// send via network or write results to the next wave file
static void
simoutsned (void)
{
  static int recctr, flctr, nrecs;
  static FILE *wfile = 0;
  int n;
  static char *wfile_name=0;
  static char *wfile_name_tmp=0;
  static char *buffptr=wave_buf;
  char line[80];
  int time = (int)((S.stepnum + 1) * S.step / dt_step);

  if (!have_data_socket() && !write_waves) // if using socket and other side died, nothing to do
    return;
  if (recctr == 0)  // new wave block set up
  {
    nrecs = S.step_count - S.stepnum >= 100 ? 100 : S.step_count - S.stepnum;
 
    if (have_data_socket())
    {
      sprintf(line,"%c%d\n%d\n", MSG_START, S.spawn_number,flctr);
      memcpy(buffptr,line,strlen(line)); // don't want null
      buffptr += strlen(line);
      sprintf (line, "%12d %f\n", nrecs, S.step);
      memcpy(buffptr,line,strlen(line));
      buffptr += strlen(line);
      sprintf (line, "%12d\n", S.plot_count);
      memcpy(buffptr,line,strlen(line));
        buffptr += strlen(line);
      for (n = 0; n < S.plot_count; n++)
      {
        sprintf(line, "%3d %3d %3d %d %s\n", S.plot[n].pop, S.plot[n].cell, S.plot[n].var, S.plot[n].type, S.plot[n].lbl);
        memcpy(buffptr,line,strlen(line));
        buffptr += strlen(line);
      }
      for (n = 0; n < S.plot_count; n++)
      {
        sprintf (line, "%12.8f %d\n", S.plot[n].val, S.plot[n].spike);
        memcpy(buffptr,line,strlen(line));
        buffptr += strlen(line);
        if (write_smr_wave)
        {
           if (S.plot[n].var != STD_FIBER && S.plot[n].var != AFFERENT_EVENT && S.plot[n].var != AFFERENT_BOTH)
              writeWaveForm(n,time,S.plot[n].val);
           if (S.plot[n].var != AFFERENT_SIGNAL && S.plot[n].var != AFFERENT_BOTH && S.plot[n].spike)
              writeWaveSpike(n, time);
        }
      }
    }
    else // writing files
    {
      
         // don't let simviewer see file until it is complete
         // write header and first block of wave values
         if (asprintf (&wfile_name, "%swave.%02d.%04d", outPath, S.spawn_number, flctr) == -1) exit (1);
         if (asprintf (&wfile_name_tmp, "%swave.%02d.%04d.tmp", outPath, S.spawn_number, flctr) == -1) exit (1);
         
         // JAH: if simrun is run with a script it no longer produces the wave* files
         if (!noWaveFiles) 
         {
            (wfile = fopen (wfile_name_tmp, "w")) || DIE;
            fprintf (wfile, "%12d %f\n", nrecs, S.step);
            fprintf (wfile, "%12d\n", S.plot_count);
            for (n = 0; n < S.plot_count; n++) {
              fprintf (wfile, "%3d %3d %3d %d %s\n", S.plot[n].pop, S.plot[n].cell, S.plot[n].var, S.plot[n].type, S.plot[n].lbl);
            }
         }
         for (n = 0; n < S.plot_count; n++)
         {
            if (!noWaveFiles) 
            {
                 fprintf (wfile, "%12.8f %d\n", S.plot[n].val, S.plot[n].spike);
            }
            if (write_smr_wave)
            {
                 //fprintf(stderr, "val: %d\n", S.plot[n].var);
                 if (S.plot[n].var != STD_FIBER && S.plot[n].var != AFFERENT_EVENT && S.plot[n].var != AFFERENT_BOTH) {
                    //fprintf(stderr, "writing: %d\n", n);
                    writeWaveForm(n,time,S.plot[n].val);
                 }
                 if (S.plot[n].var != AFFERENT_SIGNAL && S.plot[n].var != AFFERENT_BOTH && S.plot[n].spike) {
                    writeWaveSpike(n, time);
                 }
            }
            
         }       
    }
    ++recctr;
    return;
  }
     // set up done if here, accumlate results
  for (n = 0; n < S.plot_count; n++)  
  {
    sprintf (line, "%12.8f %d\n", S.plot[n].val, S.plot[n].spike);
    if (have_data_socket())
    {
      memcpy(buffptr,line,strlen(line));
      buffptr += strlen(line);
    }
    else
    {
     if (!noWaveFiles) {
        fprintf (wfile, "%12.8f %d\n", S.plot[n].val, S.plot[n].spike);
     }
    }
    if (write_smr_wave)
    {
       if (S.plot[n].var != STD_FIBER && S.plot[n].var != AFFERENT_EVENT && S.plot[n].var != AFFERENT_BOTH)
          writeWaveForm(n,time,S.plot[n].val);
       if (S.plot[n].var != AFFERENT_SIGNAL && S.plot[n].var != AFFERENT_BOTH && S.plot[n].spike)
          writeWaveSpike(n, time);
    }
  }
  if (++recctr == nrecs) 
  {
    if (use_socket)
    {
       sprintf(line,"%c", MSG_END);
       memcpy(buffptr,line,strlen(line));
       buffptr += strlen(line);
       send_wave();
       buffptr = wave_buf;
       memset (wave_buf, 0, sizeof(wave_buf));
    }
    else
    {
       // Note: when testing this with Win10 in a VM from time to time, closing
       // the file does not actually result in a file with anything in it.
       // When simviewer tries to read it, it sees an empty file after we rename it.
       // This has not been reported native Win10, so I guess it is a VM problem.
       // Flushing seems to make this not happen.
       if (wfile) {
          fflush(wfile); 
          fclose (wfile); 
       }
       if (wfile_name) {
          rename(wfile_name_tmp,wfile_name);
          free (wfile_name);
          free (wfile_name_tmp);
       }
    }
    recctr = 0;
    if (++flctr == 10000)  // wrap 9999+1 back to 0.
      flctr = 0;
  }
}

#define SQR(a) ((a)*(a))

static bool
check_lung_used (void)
{
  for (int pn = 0; pn < S.net.cellpop_count; pn++)
    if (S.net.cellpop[pn].ic_expression)
      return true;
  return false;
}

typedef struct
{
  int count;
  char *name;
  char *var;
  int *num;
  double *rate;
} PopList;

typedef struct
{
  PopList phrenic;
  PopList abdominal;
  PopList pca;
  PopList ta;
  PopList expic;
  PopList inspic;
} MotorPops;

static void
mup_error (muParserHandle_t hParser)
{
  char *msg, *cmd;
  if (asprintf (&msg,
                "\"Fatal error parsing the equation \"%s\"\n"
                "Message:  \"%s\"\n"
                "Token:    \"%s\"\n"
                "Position: %d\n"
                "Errc:     %d\n\"",
                mupGetExpr (hParser),
                mupGetErrorMsg(hParser),
                mupGetErrorToken(hParser),
                mupGetErrorPos(hParser),
                mupGetErrorCode(hParser)) == -1) exit (1);
    if (asprintf (&cmd, "%s %s %s", simmsg, "\"SIMRUN ERROR\"", msg) == -1) exit (1);
  if (system (cmd))
  {}
  free(msg);
  free(cmd);
  error (1, 0, "%s", msg);
}

static double
spikes_per_s_per_cell (int pn)
{
  CellPop *p = S.net.cellpop + pn;
  int spike_count = 0;
  for (int cn = 0; cn < p->cell_count; cn++)
    spike_count += p->cell[cn].spike;
  if (0) {
    static FILE *f;
    if (f == NULL) f = fopen ("phrenic_spike_counts", "w");
    if (pn == 24) fprintf (f, "%d", spike_count);
    if (pn == 43) fprintf (f, " %d\n", spike_count);
  }
  
  return (double)spike_count / p->cell_count / (S.step / 1000);
}

double
mup_eval (PopList pops, char *eqn, void **parser)
{
  if (eqn == NULL) {            /* PCA or TA */
    if (pops.num[0] < 0) 
      error_at_line (1, 0, __FILE__, __LINE__, "No %s0 pop found", pops.name);
    return spikes_per_s_per_cell (pops.num[0]);
  }
  if (*parser == NULL) {
    *parser = mupCreate (muBASETYPE_FLOAT);
    mupSetErrorHandler (*parser, mup_error);
    for (int i = 0; i < pops.count; i++)
      if (pops.num[i] >= 0) {
        char *varname;
        if (asprintf (&varname, "%s%d", pops.var, i) == -1) exit (1);
        mupDefineVar(*parser, varname, &pops.rate[i]);  
        free (varname);
      }
    mupSetExpr (*parser, eqn);
  }
  
  for (int i = 0; i < pops.count; i++)
    if (pops.num[i] >= 0)
      pops.rate[i] = spikes_per_s_per_cell (pops.num[i]);

  return mupEval (*parser);
}

PopList
get_poplist_by_name (char *name, char *n2, char *var)
{
  char *regex;
  regex_t re, re_pre;
  char *pattern = "(^|[^[:alnum:]])%s([0-9]+)?($|[^[:alnum:]])";
  if (asprintf (&regex, pattern, name) == -1) 
    exit (1);

  if (regcomp (&re, regex, REG_EXTENDED | REG_ICASE)) 
    error (1, 0, "%s", "poplist regcomp failed");
  free (regex);

  if (asprintf (&regex, pattern, "pre") == -1)
     exit (1);
  if (regcomp (&re_pre, regex, REG_NOSUB | REG_EXTENDED | REG_ICASE)) 
    error (1, 0, "%s", "poplist regcomp pre failed");
  free (regex);

  PopList pl = {0};
  pl.name = name;
  pl.var = var;

  int count = 0;
  for (int pn = 0; pn < S.net.cellpop_count; pn++) 
  {
    regmatch_t pmatch[3];
    if (S.net.cellpop[pn].name
        && regexec (&re, S.net.cellpop[pn].name, 3, pmatch, 0) == 0
        && regexec (&re_pre, S.net.cellpop[pn].name, 0, 0, 0) != 0) 
    {
      int pli = 0;
      if (pmatch[2].rm_eo > pmatch[2].rm_so)
        pli = atoi (S.net.cellpop[pn].name + pmatch[2].rm_so);
      if (pli >= pl.count) {
        int newcount = pli + 1;
        pl.num = realloc (pl.num, newcount * sizeof *pl.num);
        pl.rate = realloc (pl.rate, newcount * sizeof *pl.rate);
        for (int i = pl.count; i < newcount; i++)
          pl.num[i] = -1;
        pl.count = newcount;
      }
      if (pl.num[pli] != -1) 
      {
        char *msg, *cmd;
        if (asprintf (&msg,
                      "\"%d %d %d\n"
                      "There is more than one population with the word %s in its name\n"
                      "numbered %d and without the word \"pre\"\n"
                      "There must be exactly one.  Aborting sim...\n\"",
                      pn, pl.num[pli], pli,
                      n2 ? n2 : name, pli) == -1) exit (1);
        if (asprintf (&cmd, "%s %s %s", simmsg, "\"SIMRUN ERROR\"", msg) == -1) exit (1);
        if (system (cmd)) {}
        error (1, 0, "%s", msg);
      }
      pl.num[pli] = pn;
      count++;
    }
  }
  if (count == 0) 
  {
    char *msg, *cmd;
    if (asprintf (&msg,
                  "\"There are no populations with the word %s in their names\n"
                  "and without the word \"pre\"\n"
                  "There must be exactly one.  Aborting sim...\n\"", n2 ? n2 : name) == -1) exit (1);
    if (asprintf (&cmd, "%s %s %s", simmsg, "\"SIMRUN ERROR\"", msg) == -1) exit (1);
    if (system (cmd)) {}
    error (1, 0, "%s", msg);
    free(msg);
    free(cmd);
  }
  regfree (&re);
  regfree (&re_pre);
  return pl;
}

static MotorPops
get_motor_pops (void)
{
  MotorPops m;
  m.phrenic = get_poplist_by_name ("phrenic", "\"phrenic\"", "P");
  m.abdominal = get_poplist_by_name ("lumbar", "\"lumbar\"", "L");
  m.pca = get_poplist_by_name ("(PCA|ILM)", "\"PCA\" or \"ILM\"", "");
  fprintf(stderr, "pca %0.1f\n", *m.pca.rate);
  
  m.ta = get_poplist_by_name ("(TA|ELM)", "\"TA\" or \"ELM\"", "");
  fprintf(stderr, "ta %0.1f\n", *m.ta.rate);
#ifdef INTERCOSTALS
  m.expic = get_poplist_by_name ("int", "\"int\"", "");
  m.inspic = get_poplist_by_name ("ext", "\"ext\"", "");
#endif
  return m;
}

static State state;

static inline double
get_GE0 (CellPop *p)
{
  if (!p->ic_expression)
    return p->GE0;
  if (!p->ic_evaluator)
    assert ((p->ic_evaluator = (char *) xp_set (p->ic_expression)));
//  printf("volume %f\n", state.volume);
  return xp_eval ((Expr *)p->ic_evaluator, state.volume);
}

static double
limit (double x, double lo, double hi)
{
  if (x > hi)
    return hi;
  else if (x < lo)
    return lo;
  return x;
}


/*
This gets called a once per simulation loop. I benchmarked it an on my
laptop it takes about 700 microseconds to run, so, not a lot of overhead.
If a paused msg, we stay here until we get a resume or terminate.

*/

void chk_for_cmd()
{
  int got;
  char msg[1];
  bool pause;

  if (!have_cmd_socket()) // check for commands if started up by simbuild
    return;
  pause = false;

  do
  {
    msg[0] = 0;
#ifdef __linux__
    got = recv(sock_fd,msg,sizeof(msg),MSG_DONTWAIT);
    if (got == 0 && errno == EPIPE)
    {
      fprintf(stdout,"Connection to simbuild lost.\n");
      fflush(stdout);
      destroy_cmd_socket();
      return;
    }
    else if (got == -1 && errno != EAGAIN && errno != EWOULDBLOCK)
    {
     fprintf(stdout,"SIMRUN: Error getting msg from simbuild, error is: %d\n",errno);
      fflush(stdout);
    }
#else
    got = recv(sock_fd,msg,sizeof(msg),0); // was set to nonblocking in setup
    if (got == SOCKET_ERROR)
    {
      int err = WSAGetLastError();
      if (err != WSAEWOULDBLOCK)
      {
        if (err != WSAECONNRESET)  // simviewer went away
        {
          fprintf (stdout,"Error getting msg from simbuild, error is: %d\n",WSAGetLastError());
        }
        else
          fprintf(stdout,"Connection to simbuild lost.\n");
        fflush(stdout);
        destroy_cmd_socket();
        return;
      }
    }
#endif
    if (msg[0] == 'P') // pause
    {
      pause = true;
      fprintf(stdout,"Pausing\n");
      fflush(stdout);
    }
    else if (msg[0] == 'R')  // resume
    {
      pause = false;
      fprintf(stdout,"Starting back up\n");
      fflush(stdout);
    }
    else if (msg[0] == 'U') // update
    {
      update ();
    }
    else if (msg[0] == 'T') // terminate
    {
      sigterm = true;
      pause = false;
      fprintf(stdout,"simrun got terminate command\n");
      fflush(stdout);
    }
    else if (msg[0] != 0) // unknown msg
    {
      fprintf(stdout,"Got unknown command %c\n",msg[0]);
      fflush(stdout);
    }
    if (pause) // .5 sec
    {
      usleep(500000); // We can hang here forever if simbuild goes away
                      // without unpausing us, but we could also be
    }                 // paused over the weekend, so there is no "too long"
  } while (pause);
}

static void decayLearnCells()
{
   CellPop *cp;
   for (int pn = 0; pn < numCPop; ++pn)
   {
      cp = S.net.cellpop + learnCPop[pn];
      Cell *c = cp->cell;
      for (int cn = 0; cn < cp->cell_count; cn++, ++c) 
      {
           Target *target = c->target;
           for (int tidx = 0; tidx < c->target_count; tidx++, ++target)
           {
              Syn *syn = target->syn;
              if (S.net.syntype[syn->stidx].SYN_TYPE == SYN_LEARN)
              {
                 LEARN *lrn_ptr = syn->lrn;
                 for (int slot = 0; slot < syn->lrn_size; ++slot,++lrn_ptr)
                 {
                    if (lrn_ptr->recv_pop != LRN_FREE)
                    {
                       if (lrn_ptr->ariv_time <= 1)  // remove
                       {
                          lrn_ptr->recv_pop = LRN_FREE;
                          lrn_ptr->send_term = 0;
                          lrn_ptr->recv_term = 0;
                          lrn_ptr->ariv_time = 0;
                       }
                       else
                          --lrn_ptr->ariv_time;
                    }
                 }
              }
           }
       }
    }
}

// Sender has fired, add to target history
static void updateLrnSyns(Target *target, Syn* syn, int sender)
{
   LEARN *lrn_ptr = syn->lrn;
   bool found = false;
   int slot, old_size, new_size;

   for (slot = 0; slot < syn->lrn_size; ++slot,++lrn_ptr)
   {
      if (lrn_ptr->recv_pop == LRN_FREE)
      {
         found = true;
         break;
      }
   }
   if (!found) // grow the array, init the new slots
   {
      old_size = syn->lrn_size;
      new_size = old_size  + LRN_GROWBY;
      syn->lrn_size = new_size;
      lrn_ptr = realloc(target->syn->lrn,sizeof(LEARN) *new_size);
      target->syn->lrn = lrn_ptr;
      lrn_ptr += old_size; // skip to start of new slots
      for (slot = old_size; slot < new_size; ++slot,++lrn_ptr)
      {
        lrn_ptr->recv_pop = LRN_FREE;
        lrn_ptr->send_term = 0;
        lrn_ptr->recv_term = 0;
        lrn_ptr->ariv_time = 0;
      }

      lrn_ptr = target->syn->lrn + old_size; // 1st new slot
   }
   lrn_ptr->send_term = sender;
   lrn_ptr->recv_pop = syn->cpidx;
   lrn_ptr->recv_term = syn->cidx;
   // the fortran program from the MacG book generated a random number
   // between 1 and the conductance time. We more or less have already
   // done with the target->delay calculation for each terminal.
   lrn_ptr->ariv_time = target->delay + 1 + syn->lrnWindow; 
}


static void decayLearnFibers()
{
   FiberPop *p;
   for (int pn = 0; pn < numFPop; ++pn)
   {
      p = S.net.fiberpop + learnFPop[pn];
      Fiber *f = p->fiber;
      for (int fn = 0; fn < p->fiber_count; ++fn, ++f) 
      {
         Target *target = f->target;
         for (int tidx = 0; tidx < f->target_count; tidx++, ++target)
         {
            Syn *syn = target->syn;
            if (S.net.syntype[syn->stidx].SYN_TYPE == SYN_LEARN)
            {
               LEARN *lrn_ptr = syn->lrn;
               for (int slot = 0; slot < syn->lrn_size; ++slot,++lrn_ptr)
               {
                  if (lrn_ptr->recv_pop != LRN_FREE)
                  {
                     if (lrn_ptr->ariv_time <= 1) 
                     {
                        lrn_ptr->recv_pop = LRN_FREE;
                        lrn_ptr->send_term = 0;
                        lrn_ptr->recv_term = 0;
                        lrn_ptr->ariv_time = 0;
                      }
                      else
                        --lrn_ptr->ariv_time;
                    }
                 }
              }
           }
       }
    }
}

/* This is the simulation calculation engine.
*/
void simloop ()
{
  int nf;
  double noise_decay = exp (-S.step / 1.5);
  S.seed = 314159;
  double ticks_in_sec = ceil(1000.0/S.step);
  char msg[2048]={0};
  bool lung_is_used = check_lung_used ();
  bool doFibCalc, skipFib;
  MotorPops m;
  double signal;
  if (Debug)
    fprintf (stdout, "\n%s line %d, cellpop_count %d\n", __FILE__, __LINE__,
             S.net.cellpop_count);

  if (lung_is_used) 
  {
    if (!S.phrenic_equation || S.phrenic_equation[0] == 0)
      S.phrenic_equation = strdup ("P0/100");
    if (!S.lumbar_equation || S.lumbar_equation[0] == 0)
      S.lumbar_equation = strdup ("L0/20");
    m = get_motor_pops ();

    fprintf(stdout,"motor pops are: phrenic");
    for (int i = 0; i < m.phrenic.count; i++)
      if (m.phrenic.num[i] >= 0)
        fprintf(stdout," %d", m.phrenic.num[i] + 1);
    fprintf(stdout,", abdominal");
    for (int i = 0; i < m.abdominal.count; i++)
      if (m.abdominal.num[i] >= 0)
        fprintf(stdout," %d", m.abdominal.num[i] + 1);
    fprintf (stdout,", elm/ta %d, ilm/pca %d", m.ta.num[0] + 1, m.pca.num[0] + 1);
#ifdef INTERCOSTALS
    fprintf(stdout,", ext %d, int %d", m.ta.num[0] + 1, m.pca.num[0] + 1, m.inspic.num[0] + 1, m.expic.num[0] + 1);
#endif
    fprintf(stdout,"\n");
    
    state = lung ((Motor) {0,0,0,0}, S.step);
  }
  else 
     fprintf(stdout,"Lung model is not used\n");
  if (Debug)
  {
     fprintf (stdout, "\n%s line %d, cellpop_count %d\n", __FILE__, __LINE__,
               S.net.cellpop_count);
     fprintf (stdout, "\n%s line %d\n", __FILE__, __LINE__);
  }

   // MAIN LOOP, work until done or get a TERM signal or get a quit command
  for ( ; S.stepnum < S.step_count && !sigterm; S.stepnum++) 
  {
    double GEsum0; // debug var
    int pn;
    static time_t now, last_time = 0;

    Motor mr;
    State next_state = {0};
    
    if (lung_is_used) 
    {
      mr.phrenic = mup_eval (m.phrenic, S.phrenic_equation, &S.pe_evaluator);
      mr.abdominal = mup_eval (m.abdominal, S.lumbar_equation, &S.le_evaluator);
      
      // JAH: lma (laryngeal muscle activation) is defined in lung.c as pca-ta and goes from 1 (open) to -1 (closed)
      mr.pca = mup_eval (m.pca, 0, 0);
      mr.ta = mup_eval (m.ta, 0, 0);
#ifdef INTERCOSTALS
      mr.expic = mup_eval (m.expic, 0, 0);
      mr.inspic = mup_eval (m.inspic, 0, 0);
#endif
     // printf("phrenic: %f \n", mr.phrenic);
      next_state   = lung (mr, S.step);
    }

    if (have_cmd_socket() && (S.stepnum % (int) ticks_in_sec) == 0)
    {
        snprintf(msg,sizeof(msg)-1,"TIME\n%d\n", (int)(S.stepnum/ticks_in_sec));
        sendMsg(msg); // progress rpt for simbuild 
    }
    if ((now = time (0)) > last_time)
	    global_last_time = last_time = now;

    nf = 0;
    for (pn = 0; pn < S.net.cellpop_count; pn++)  // cells
    {
      CellPop *p = S.net.cellpop + pn;
      int cn;
      for (cn = 0; cn < p->cell_count; cn++) 
      {
        Cell *c = p->cell + cn;

        double Gsum = 0, GEsum = 0, Prob = 0, Vm, Gk;
        int sidx, tidx, pp_idx;

        for (sidx = 0; sidx < c->syn_count; sidx++) 
        {
          Syn *s = c->syn + sidx;

          if (S.ispresynaptic) 
          {
            int type_of_syn = S.net.syntype[s->stidx].SYN_TYPE;
            if (type_of_syn == SYN_NOT_USED) // you've got a bug
              fprintf(stdout,"Unexpected unused synapse in simloop\n");
            if (type_of_syn == SYN_NORM)
            {
               Syn *chk = c->syn;
               double post = 1;
                  // do we have a post item associated with us?
               for (pp_idx = 0; pp_idx < c->syn_count; ++pp_idx, ++chk) 
               {
                  if (chk->synparent && chk->synparent == s->stidx+1)
                  {
                     if (chk->syntype == SYN_POST)
                     {
                       post = chk->G;
                       break; // at most one of these
                     }
                  }
               }
                // Normalize Conductance sum += syn Normalized Conductance
                // * Post syn Normalized Conductance or 1
               Gsum += s->G * post;

               // Excitatory Conductance sum += syn Normalized Conductance
               // * Post syn Normalized Conductance or 1 
               // * (syn Equlibrium Potentential - offset if burster)
               GEsum += s->G * post * (s->EQ - (p->pop_subtype  == BURSTER_POP ? 65. : 0));

               if (p->pop_subtype == PSR_POP) 
                  // for pulmonary stretch receptor
                  // Probabilty += syn Normalized Conductance
                  // * post syn Normalized Conductance or 1
                  // * 1 / syn Decay of Potential in a Compartment
                  // NOTE: Prob is always zero except for this case.
                  Prob += s->G * post * (1. - s->DCS); /* PSR or other external object */
            }
          }
          else  // NOT pre/post synaptic
          {
            // Normalize Conductance sum += syn Normalized Conductance
            Gsum += s->G;

            // Excitatory Conductance sum += syn Normalized Conductance
            // * (syn Equlibrium Potentential - offset if burster)
            GEsum += (double)s->G * (s->EQ - (p->pop_subtype == BURSTER_POP ? 65. : 0));

            if (p->pop_subtype == PSR_POP) 
               // for pulmonary stretch receptor
               // probabilty += syn Normalized Conductance
               // * 1 / syn Decay of Potential in a Compartment
               Prob += s->G * (1. - s->DCS); /* PSR or other external object */
           }
        }
#define NOISE_FIRING_PROBABILITY .05
#define NOISE_EQ 70

        if (p->noise_amp)
        {
          double ranval;
          double gnoise_e = c->gnoise_e;
          double gnoise_i = c->gnoise_i;
          gnoise_e *= noise_decay;
          gnoise_i *= noise_decay;
          if ((ranval = ran (&p->noise_seed)) < NOISE_FIRING_PROBABILITY)
            gnoise_e += p->noise_amp;
          if ((ranval = ran (&p->noise_seed)) < NOISE_FIRING_PROBABILITY)
            gnoise_i += p->noise_amp;
          // Excitatory Conductance sum += some noise
          Gsum += gnoise_e + gnoise_i;
          // Excitatory Conductance sum += some noise
          GEsum += gnoise_e * ( NOISE_EQ - (p->pop_subtype == BURSTER_POP ? 65. : 0));
          GEsum += gnoise_i * (-NOISE_EQ - (p->pop_subtype == BURSTER_POP ? 65. : 0));
          c->gnoise_e = gnoise_e;
          c->gnoise_i = gnoise_i;
        }
        GEsum0 = GEsum; // this only used in a debug statement later

        // Vm is potential in millivolts?  V in MacGregor is Potential
        // Copy Potential(mv) 
        // V in Mac. is Potential
        Vm = c->Vm_prev = c->Vm;

        // copy Potassium Conductance for cell
        Gk = c->Gk;
        if (p->pop_subtype == PSR_POP)  /* PSR or other external object */
        {
           // if Potential < Prob then DC = Decay Constant For Threshold
           // else DC = Decay Constant for Potassium Action
          double DC = Vm < Prob ? p->DCTH : p->DCG;
          if (S.stepnum == 0) // special case, 1st time in loop
            Vm = 0;
          // Potential = Potential - Prob * Decay Constant + Prob
          Vm = (Vm - Prob) * DC + Prob;
          // Thr is Resting Threshold + random gaussian number * optional std dev
          // if Potential > Thr then if random # <= Potential  - threshold
          // spike is 1 (fired) else 0 (not)
          c->spike = (Vm > c->Thr) ? (ran (&S.seed) <= (Vm - c->Thr)) : 0;
        }
        else 
        {
             /* hybrid Integrate and Fire (IF) cell (Breen, et. al. model) */
          if (p->pop_subtype == BURSTER_POP)   
          {
            double G_NaP;
            // m_inf=1/(1 + exp ((Potential - c_thresh_active_iak) / c_max_conductance_ika))
            double m_inf = 1 / (1 + exp ((Vm - p->theta_m) / p->sigma_m));

            // h_inf=1/(1+exp((Potential - rebound_time_k) / max_conductance_ika))
            double h_inf = 1 / (1 + exp ((Vm - p->theta_h) / p->sigma_h));

            // tau_h= accomodation / cosh((Potential-rebound_time_k) / (2 * max_conductance_ika)
            double tau_h = p->taubar_h / cosh ((Vm - p->theta_h) / (2 * p->sigma_h));
            if (S.stepnum == 0)  // special case, 1st time
            {
               Gk = .43;  // initial Potassium Conductance
               Vm = -52;  // initial millivolts
            }
            // Potassium Conductance = h_inf + (Potassium Conductance) * exp(-S.step/tau_h)
            Gk = h_inf + (Gk - h_inf) * exp (-S.step / tau_h);

            // G_NaP = Persisten Sodium Current(?) * m_inf * Potassium conductance.
            G_NaP = p->g_NaP_h * m_inf * Gk;
             // more complicated stuff
            Gsum += G_NaP + g_L;
            GEsum += G_NaP * E_Na + g_L * E_L + get_GE0 (p);
          }
          else 
          {
            // if (spike) Potassium Conductance = Sensitivity to Potassium Conductance
            //            + (Potassium Conductance - Sensitivity to Potassium Conductance)
            // else       Potassium Conductance = Potassium Conductance 
            //                                  * Decay Constant for Potassium Action
            Gk = c->spike ? p->B + (Gk - p->B) * p->DCG : Gk * p->DCG;

            // Normalized Conductance sum +=  Potassium Conductance + Gm0
            // (I can't find any place in the code where Gm0 is anything but 1.)
            Gsum += Gk + S.Gm0;

            // Something to do with Excitatory Conductance derived from an
            // injected expression based on a formula or an evaluated equation.
            // Formula is: if Decay Constant for Potassium == -1
            //                GE0 = DC Injected Current * 0 
            //             else
            //                GE0 = DC Injected Current *  Gm0 * Vm0;
            //             GM0 seems to always be 1, Vm0 seems to always be 0
            double GE0 = get_GE0 (p);

            if(Debug)
            {
               static char done[100];
               if (!done[pn]) 
               {
                 // printf ("pn = %d, GE0: %g\n", pn, GE0);
                 done[pn] = 1;
              }

              static double last_volume = -1;
              if (p->ic_expression && state.volume != last_volume) 
              {
                //  printf ("pn = %d, GE0: %.17e, volume: %.17e\n", pn, GE0, state.volume);
                last_volume = state.volume;
              }
            }
            // Excitatory Conductance sum += Excitatory Conductance from injected current
            //                  * Potassium Conductance * Potassium Equilibrium Potential
            GEsum += GE0 + Gk * S.Ek;
          }

          // R0 is 0 for PSR, otherwise,
          // R0 = -.5 * step size in ms / membrane time constant  * Gm0(always 1?)
          // Potential = Excitatory Conductance sum / Normalized Conductance sum
          //             + (Potential - Excitatory Conducance sum/Normalized Conductance sum
          //             * exp(Normalized Conductance sim * R0
          //
          Vm = GEsum/Gsum + (Vm - GEsum/Gsum) * exp(Gsum * p->R0);

         if (0) // debug stuff, 0 -> 1 to turn it on
         {
           static FILE *f;
           if (f == NULL)
             f = fopen ("simloop.dbg", "w");
           if (pn == 37)
           fprintf (f, "step: %d, cn: %d, Vm: %g %g %g %g %g %g %g %g %g %d %g %g | %g %g %g %g %g\n",
                    S.stepnum, cn, Vm, GEsum, Gsum, c->Vm, p->R0, GEsum0, p->GE0, Gk, S.Ek, c->spike, p->B, p->DCG,
                    c->Thr, p->Th0, p->MGC, S.Vm0, p->DCTH);
            if (S.stepnum == 6) 
            {
              fclose (f);
              exit (0);
            }
         }
       
         if (p->pop_subtype == BURSTER_POP)
            // Thr = Threshold Voltage
           c->Thr = p->Vthresh;
         else 
         {
            // Potential time(?) = Resting Threshold + Accomodation Paramter 
            //                     * (Potential - Vm0(always 0?))
           double Vt = p->Th0 + p->MGC * (Vm - S.Vm0);

           // Thr starts as Resting Threshold + random gaussian number * optional std dev
           // Thr = Vt + (Thr - Vt) * Decay Constant for Threshold
           c->Thr = Vt + (c->Thr - Vt) * p->DCTH;
         }
         // spike is 1 if Potential >= Threshold
         //          0 if not
         c->spike = Vm >= c->Thr;
       }

       if (c->spike)  // fired?
       {
         int widx;
         if (write_bdt)
         {
           for (widx = 0; widx < S.cwrit_count; widx++)
           {
             if (S.cwrit[widx].pop == pn + 1) 
             {
               if  (S.cwrit[widx].cell == cn + 1)
               {
                 fprintf (S.ofile, fmt, 100 + widx + 1, (int)((S.stepnum + 1) * S.step / dt_step));
                 fprintf (S.ofile, "%c",0x0a);
               }
               else if (S.cwrit[widx].cell == 999999999) /* Thu Feb  1 08:42:27 EST 2007: what is this? ROC */
               {
                    fprintf (S.ofile, fmt, 21 + cn, (int)((S.stepnum + 1) * S.step / dt_step));
                      // if running in DOS, no way now to view bdt, 
                      // so use Linux newline char
                    fprintf (S.ofile, "%c",0x0a);
               }
             }
           }
         }
         if (write_smr)
         {
           for (widx = 0; widx < S.cwrit_count; widx++)
           {
             if (S.cwrit[widx].pop == pn + 1) 
             {
               if  (S.cwrit[widx].cell == cn + 1)
               {
                 writeSpike(100 + widx + 1, (int)((S.stepnum + 1) * S.step / dt_step));
               }
               else 
                  if (S.cwrit[widx].cell == 999999999) /* Thu Feb  1 08:42:27 EST 2007: what is this? ROC */
                  {
                    fprintf (S.ofile, fmt, 21 + cn, (int)((S.stepnum + 1) * S.step / dt_step));
                    fprintf (S.ofile, "%c",0x0a); // if running in DOS, no way now to view 
                                                  // bdt, so make this linux text format
                  }
             }
           }
         }
         if (pn + 1 == S.nanlgpop) 
           nf++;
           // Increase strength value in current slot in the q array
         for (tidx = 0; tidx < c->target_count; tidx++) 
         {
           if (Debug) {printf("Cell Fire\n");}
           Target *target = c->target + tidx;
           Syn *syn = target->syn;
           if (target->disabled)
             continue;
           int type_of_syn=S.net.syntype[syn->stidx].SYN_TYPE;
           switch (type_of_syn)
           {
              case SYN_NORM:
                 syn->q[(S.stepnum + target->delay) % syn->q_count] += target->strength;
                 break;
              case SYN_LEARN: 
                 syn->q[(S.stepnum + target->delay) % syn->q_count] += target->syn->lrn_strength;
                 {if(Debug)printf("update target %d slot: %d\n",target->syn->cidx,(S.stepnum + target->delay) % syn->q_count);}
                 updateLrnSyns(target,syn,cn);
                 break;
               case SYN_PRE:
               case SYN_POST: // == 1 has no effect
                  if (target->strength < 1)
                     syn->q[(S.stepnum + target->delay) % syn->q_count] *= target->strength;
                   else  if (target->strength > 1)
                      syn->q[(S.stepnum + target->delay) % syn->q_count] += target->syn->lrn_strength;
                   break;
               default:
                  printf("Unknown synapse type %d\n",type_of_syn);
                  break;
           }

           if(Debug){ printf("Cell %d %d %.2lf\n",target->delay, (S.stepnum + target->delay) % syn->q_count, syn->q[(S.stepnum + target->delay) % syn->q_count]);}
         }
         // we fired. If we we have any learning
         // input synapses, reward them.
         LEARN *lrn_ptr;
         double delta;
         Syn* lsyn = c->syn;
         int lsyn_num, lrn_num;
         for (lsyn_num = 0; lsyn_num < c->syn_count; ++lsyn_num, ++lsyn)
         {
            if (lsyn->syntype != SYN_LEARN)
               continue;
            lrn_ptr = lsyn->lrn;
            bool have_history = false;
            {if(Debug)printf("Using learn synapse for pop: %d cell: %d\n",pn,cn);}
            for (lrn_num = 0; lrn_num < lsyn->lrn_size; ++lrn_num,++lrn_ptr)
            {
               if (lrn_ptr->recv_pop == LRN_FREE)
                  continue;
               if (lrn_ptr->ariv_time > lsyn->lrnWindow)
               {
                  have_history = true; // may not use it, but pending
                  continue;            // so don't forget below
               }
               have_history = true;
//               {if(Debug)printf("tick: %d pop: %d cell:%d rcv: %d %lf ->" ,S.stepnum, pn, cn, lrn_ptr->recv_term,lsyn->lrn_strength);}
               // Hebbian learning equation from MacGregor
               delta = lsyn->lrnStrDelta * (fabs(lsyn->lrnStrMax - lsyn->lrn_strength));
               lsyn->lrn_strength += delta;
               // can overflow if initial > max 
               // or underflow because it does if neg delta
               if (lsyn->lrnStrDelta > 0)
               {
                  if (lsyn->lrn_strength > lsyn->lrnStrMax)
                     lsyn->lrn_strength = lsyn->lrnStrMax;
               }
               else if (lsyn->lrnStrDelta < 0)
               {
                  if (lsyn->lrn_strength < lsyn->lrnStrMax)
                     lsyn->lrn_strength = lsyn->lrnStrMax;
               }

               {if(Debug)printf("tick: %d cell fire USE HISTORY: pop: %d cell %d syn: %d new str: %lf\n" ,S.stepnum,pn, cn,lsyn_num,lsyn->lrn_strength);
               fflush(stdout);}
            }

            // if cell fired but no pending input events, unlearn
            if (!have_history)
            {
               delta = lsyn->lrnStrDelta * (fabs(lsyn->lrnStrMax - lsyn->lrn_strength));
               lsyn->lrn_strength -= delta;
               if (lsyn->lrnStrDelta >= 0)
               {
                  if (lsyn->lrn_strength < lsyn->initial_strength)
                     lsyn->lrn_strength = lsyn->initial_strength;
               }
               else
               {
                  if (lsyn->lrn_strength > lsyn->initial_strength)
                     lsyn->lrn_strength = lsyn->initial_strength;
               }
               {if(Debug)printf("tick: %d cell fire NO HISTORY pop: %d cell %d syn %d new str: %lf\n",S.stepnum,pn,cn,lsyn_num,lsyn->lrn_strength);}
               fflush(stdout);
            }
         }
         if (p->pop_subtype == BURSTER_POP) 
         {
           Vm = ((11.085 * Gk) - 6.5825) * Gk + p->Vreset;
           Gk += p->delta_h - .5 * 0.0037 * Gk;
         }
       }
       c->Vm = Vm; // remember these for next time around the loop
       c->Gk = Gk;
      }
    }
//edit here to continue equation comments
   for (pn = 0; pn < S.net.fiberpop_count; pn++)      // fibers
   {
      FiberPop *p = S.net.fiberpop + pn;
      skipFib = false;
      doFibCalc = false;
      int fn;
      int widx;
      signal = 0.0;
      if (S.stepnum >= p->start - 1 && S.stepnum < p->stop - 1)
      {
        if (p->pop_subtype == ELECTRIC_STIM)
        {
          skipFib = true; // don't do this unless hit the next tick
          if (p->next_stim == S.stepnum) // time to apply stim
          {
             doFibCalc = true;
             skipFib = false;
             switch(p->freq_type)
             {
                case STIM_FIXED:
                   p->next_stim += ticks_in_sec / p->frequency;
                   break;

                case STIM_FUZZY:
                   p->next_fixed += ticks_in_sec / p->frequency;
                   int fuzz_min = -(((p->fuzzy_range/2)/S.step));
                   int fuzz = (int)(ran(&p->infsed) * (p->fuzzy_range/S.step));
                   int next_targ = fuzz_min + fuzz;
                   p->next_stim = p->next_fixed + next_targ;
                   break;
                default:
                   fprintf(stdout,"Unknown type of electric stim frequency\n");
                   break;
             }
           }
        }
        else if (p->pop_subtype == AFFERENT)
        {
           nextExternalVal(p,&signal);
           if(Debug){printf("Got %lf\n",signal);}
           p->probability = interpolate(p,signal);
           if(Debug){printf("pn: %d  Prob for %lf is %lf\n",pn,signal,p->probability);}
           if (p->slope_scale) // slope of prev signal and current
           {
              if (S.stepnum > 0)
              {
                 p->probability += (signal - p->prev_signal) * p->slope_scale;
                 if (p->probability > 1.0)
                    p->probability = 1.0;
                 if (p->probability < 0.0)
                    p->probability = 0.0;
                 if(Debug){printf("slope %lf  slope prob mod: %lf\n", 
                       (signal - p->prev_signal),
                       (signal - p->prev_signal) * p->slope_scale);}
              }
              p->prev_signal = signal;
           }
        }
        else if (p->pop_subtype == 0) // may be 0 due to bug,
           p->pop_subtype = FIBER;    // assume normal fiber


        for (fn = 0; fn < p->fiber_count; fn++) 
        {
          int tidx;
          Fiber *f = p->fiber + fn;
          f->state = 0;
          f->signal = signal; // same for all 
          double ranval = ran(&p->infsed);
          //  force estim evt  or  normal/aff, use prob
          if (doFibCalc        || (!skipFib && ranval <= p->probability))
          {
            if(Debug){printf("Fiber fire\n");}
            f->state = 1; // an event occurred
            doFibCalc = false;
            if (write_bdt)
            {
              for (widx = 0; widx < S.fwrit_count; widx++)
                if (S.fwrit[widx].pop == pn + 1 && S.fwrit[widx].cell == fn + 1)
                {
                  fprintf (S.ofile, fmt, 100+ S.cwrit_count + widx + 1, (int)((S.stepnum + 1) * S.step / dt_step));
                  fprintf (S.ofile, "%c",0x0a);
                }
            }
            if (write_smr)
            {
              for (widx = 0; widx < S.fwrit_count; widx++)
                if (S.fwrit[widx].pop == pn + 1 && S.fwrit[widx].cell == fn + 1)
                {
                  writeSpike(100+ S.cwrit_count + widx + 1, (int)((S.stepnum + 1) * S.step / dt_step));
                }
            }
             // Targets means terminals. The starting origin for the q delay 
             // arrays are randomly set during build_network.
             // Fiber fired. Increase strength value in the q arrays at the
             // current index determined by the index expression below.
             // In effect, potentials are stored in a delay array.
// ** Only the sender knows when it fires, so it has to add  
// ** history to the receiver's history list.
// ** Only the receiver knows when it fires, so it has to 
// ** do the searching and rewarding. 
// ** 
// ** 

            Target *target = f->target;
            for (tidx = 0; tidx < f->target_count; tidx++, ++target)
            {
              Syn *syn = target->syn;
              int type_of_syn = S.net.syntype[syn->stidx].SYN_TYPE;
                // if NORM, one calc, LEARN, also update history, 
                // both pre and post get same calc, depending on sign
               switch (type_of_syn)
               {
                  case SYN_NORM:
                     syn->q[(S.stepnum + target->delay) % syn->q_count] += target->strength;
                    break;
                  case SYN_LEARN:
                    syn->q[(S.stepnum + target->delay) % syn->q_count] += target->syn->lrn_strength;
                    updateLrnSyns(target,syn,fn);
                     break;
                  case SYN_PRE:
                  case SYN_POST: // == 1 has no effect
                     if (target->strength < 1)
                        syn->q[(S.stepnum + target->delay) % syn->q_count] *= target->strength;
                      else  if (target->strength > 1)
                         syn->q[(S.stepnum + target->delay) % syn->q_count] += target->strength - 1.;
                      break;
                  default:
                      printf("Unknown synapse type %d\n",type_of_syn);
                      break;
                }
               if(Debug){printf("  Fiber: cpop %d  cell %d  syntype %d delay: %d index: %d val: %.2lf\n", 
                        syn->cpidx, syn->cidx, syn->stidx,
                        target->delay,
                        (S.stepnum + target->delay) % syn->q_count,
                        syn->q[(S.stepnum + target->delay) % syn->q_count]);}
             }
           }
         }
      }
      else
      {
         // If the last state was 1, it persists. Make sure it is zero.
        for (fn = 0; fn < p->fiber_count; fn++) 
        {
          (p->fiber + fn)->state = 0;
          (p->fiber + fn)->signal = 0;
        }
      }
   }
        // Cells and Fibers states updated for this tick.
        // This appears to propogate action potentials 
        // down the axons by updating the q array of each axon/synapse
    for (pn = 0; pn < S.net.cellpop_count; pn++) 
    {
      CellPop *p = S.net.cellpop + pn;
      int cn;
      for (cn = 0; cn < p->cell_count; cn++) 
      {
        Cell *c = p->cell + cn;
        int sidx;
        if (!S.ispresynaptic) 
        {
          for (sidx = 0; sidx < c->syn_count; sidx++) 
          {
            Syn *s = c->syn + sidx;
            if(Debug){
               printf("step: %d ", S.stepnum);
               if (s->q[S.stepnum % s->q_count])
                  printf("Cell Using slot %d %.2lf\n", S.stepnum % s->q_count,
                                                       s->q[S.stepnum % s->q_count]);
               else
                  printf(" Cell Delay slot %d\n", S.stepnum % s->q_count);
            }

             // this is where the q array values are used
            s->G = (double)s->G * s->DCS + s->q[S.stepnum % s->q_count];
            s->q[S.stepnum % s->q_count] = 0;
          }
        }
        else 
        {
          int pp_idx;

            // Walk through the syn list. For normal type, if using pre/post
            // synaptic modifiers, look for any that belong to current normal syn
          for (sidx = 0; sidx < c->syn_count; ++sidx) 
          {
            Syn *norm, *pre = 0, *post = 0;
            float *norm_q;
            int stidx = c->syn[sidx].stidx;
            int type_of_syn=S.net.syntype[stidx].SYN_TYPE;
            if (type_of_syn == SYN_PRE || type_of_syn == SYN_POST)
              continue;
            norm = c->syn + sidx;
            norm_q = norm->q + S.stepnum % norm->q_count;
            Syn *chk = c->syn;
              // do we have a pre and/or post item associated with current normal?
            for (pp_idx = 0; pp_idx < c->syn_count; ++pp_idx, ++chk) 
            {
               if (chk->synparent && chk->synparent == norm->stidx+1)
               {
                  if (chk->syntype == SYN_PRE)
                     pre = chk;
                  else if (chk->syntype == SYN_POST)
                     post = chk;
               }
            }
            if (pre) 
            {
              float *pre_q = pre->q + S.stepnum % pre->q_count;
              double G = pre->G;
              norm_q[0] *= G;
              G = (G - 1) * pre->DCS + 1;
              if (pre_q[0]  < 1) 
                 G *= (double)pre_q[0];
              else
                 G += (double)pre_q[0] - 1;
              pre_q[0] = 1;
              pre->G = G;
            }
            norm->G = (double)norm->G * norm->DCS + norm_q[0];
            norm_q[0] = 0;
            if (post) 
            {
              float *post_q = post->q + S.stepnum % post->q_count;
              double G = post->G;
              G = (G - 1) * post->DCS + 1;
              if (post_q[0] < 1)
                G *= (double)post_q[0];
              else
                G += (double)post_q[0] - 1;
              post_q[0] = 1;
              post->G = G;
            }
          }
        }
      }
    }

    if (S.outsned == 'e')   // save waveforms?
    {
       int n, i, spike_count, mult;
       static struct {int spkcntcnt; int sum; int *spkcntlst;} *pop_plot;
       static int pop_plot_size;
         // pop summaries (if we have any) will need this. indexed by n, so
         // some not used.
       if (pop_plot_size < S.plot_count) {
          TREALLOC (pop_plot, S.plot_count);
          memset (pop_plot + pop_plot_size, 0, (S.plot_count - pop_plot_size) * sizeof *pop_plot);
          pop_plot_size = S.plot_count;
       }

       for (n = 0; n < S.plot_count; n++) 
       {
         int p = S.plot[n].pop - 1;
         int c = S.plot[n].cell - 1;
         if (S.plot[n].var > 0 && (p < 0 || p >= S.net.cellpop_count))
           continue;
         S.plot[n].spike = 0;
         S.plot[n].type = S.plot[n].var >= 0 && S.net.cellpop[p].pop_subtype == BURSTER_POP;
            // what kind of plot do we save?
         switch (S.plot[n].var) 
         {
           case 1:
             if (c < S.net.cellpop[p].cell_count)
             {
                S.plot[n].val    = S.net.cellpop[p].cell[c].Vm_prev;
                if (S.net.cellpop[p].pop_subtype == BURSTER_POP)   /* hybrid IF population */
                  S.plot[n].val += 50;
                S.plot[n].spike = S.net.cellpop[p].cell[c].spike;
             }
             else
                S.plot[n].val =  S.plot[n].spike = 0;
             break;
           case 2:
             if (c < S.net.cellpop[p].cell_count)
             {
                if (S.net.cellpop[p].pop_subtype == BURSTER_POP)
                  S.plot[n].val    = S.net.cellpop[p].cell[c].Gk * 60;
                else
                  S.plot[n].val    = -20 + S.net.cellpop[p].cell[c].Gk * 10;
             }
             break;
           case 3:
             if (c < S.net.cellpop[p].cell_count)
             {
                S.plot[n].val    = S.net.cellpop[p].cell[c].Thr;
                if (S.net.cellpop[p].pop_subtype == BURSTER_POP)
                  S.plot[n].val += 50;
             }
             else
                S.plot[n].val =  S.plot[n].spike = 0;
             break;
           case -1:
             S.plot[n].val    = (state.volume - (p + 1) / 10000.) / ((c + 1) / 10000.);
             //S.plot[n].val    = (state.volume);
             //printf("volume2: %f\n", state.volume);
             break;
           case -2:
             S.plot[n].val    = (state.flow - (p + 1) / 10000.) / ((c + 1) / 10000.);
             break;
           case -3:
             S.plot[n].val    = (state.pressure - (p + 1) / 10000.) / ((c + 1) / 10000.);
             break;
           case -4:
             S.plot[n].val    = (state.Phr_d - (p + 1) / 10000.) / ((c + 1) / 10000.);
             break;
           case -5:
             S.plot[n].val    = (state.u - (p + 1) / 10000.) / ((c + 1) / 10000.);
             break;
           case -6:
             S.plot[n].val    = (state.lma - (p + 1) / 10000.) / ((c + 1) / 10000.);
             break;
           case -7:
             S.plot[n].val    = (state.Vdi - (p + 1) / 10000.) / ((c + 1) / 10000.);
             break;
           case -8:
             S.plot[n].val    = (state.Vab - (p + 1) / 10000.) / ((c + 1) / 10000.);
             break;
           case -9:
             S.plot[n].val    = (state.Vdi_t - (p + 1) / 10000.) / ((c + 1) / 10000.);
             break;
           case -10:
             S.plot[n].val    = (state.Vab_t - (p + 1) / 10000.) / ((c + 1) / 10000.);
             break;
           case -11:
             S.plot[n].val    = (state.Pdi - (p + 1) / 10000.) / ((c + 1) / 10000.);
             break;
           case -12:
             S.plot[n].val    = (state.Pab - (p + 1) / 10000.) / ((c + 1) / 10000.);
             break;
           case -13:
             S.plot[n].val    = (state.PL - (p + 1) / 10000.) / ((c + 1) / 10000.);
             break;
           case -14:
             S.plot[n].val    = (limit (state.Phr_d, 0, 1) - (p + 1) / 10000.) / ((c + 1) / 10000.);
             break;
           case -15:
             S.plot[n].val    = (limit (state.u, 0, 1) - (p + 1) / 10000.) / ((c + 1) / 10000.);
             break;
           case -16:
             S.plot[n].val    = (limit (state.lma, -1, 1) - (p + 1) / 10000.) / ((c + 1) / 10000.);
             break;
           case STD_FIBER:
           case AFFERENT_EVENT:
             S.plot[n].val   = 0;
             if (c < S.net.fiberpop[p].fiber_count)
                S.plot[n].spike = S.net.fiberpop[p].fiber[c].state;
             else
                S.plot[n].spike = 0;
             break;
           case AFFERENT_SIGNAL:
             if (c < S.net.fiberpop[p].fiber_count)
             {
                S.plot[n].val   = S.net.fiberpop[p].fiber[c].signal + S.net.fiberpop[p].offset;
                S.plot[n].type   = S.net.fiberpop[p].offset;
             }
             else
                S.plot[n].val = S.plot[n].type = 0;
             S.plot[n].spike = 0;
             break;
           case AFFERENT_BOTH: 
             if (c < S.net.fiberpop[p].fiber_count)
             {
                S.plot[n].val   = S.net.fiberpop[p].fiber[c].signal + S.net.fiberpop[p].offset;
                S.plot[n].type   = S.net.fiberpop[p].offset;
                S.plot[n].spike = S.net.fiberpop[p].fiber[c].state;
             }
             else
                S.plot[n].val = S.plot[n].spike = S.plot[n].type = 0;
             break;

           case AFFERENT_INST: 
           case AFFERENT_BIN: 
             {
               double binwidth_in_ms;
               int spkcntcnt;
               mult = (c & 0xffff) + 1;
               binwidth_in_ms = c >> 16;
               spkcntcnt = S.plot[n].var == AFFERENT_INST ? 1 : floor (binwidth_in_ms / S.step + .5);
               int sclidx = S.stepnum % spkcntcnt;
               double spikes_per_second_per_fiber;
               if (pop_plot[n].spkcntcnt != spkcntcnt) {
                 free (pop_plot[n].spkcntlst);
                 TCALLOC (pop_plot[n].spkcntlst, spkcntcnt);
                 pop_plot[n].spkcntcnt = spkcntcnt;
               }
               spike_count = 0;
               for (i = 0; i < S.net.fiberpop[p].fiber_count; i++)
                 spike_count += S.net.fiberpop[p].fiber[i].state;
               pop_plot[n].sum += spike_count - pop_plot[n].spkcntlst[sclidx];
               pop_plot[n].spkcntlst[sclidx] = spike_count;
               spikes_per_second_per_fiber = (pop_plot[n].sum / (spkcntcnt * S.step / 1000.0)
                         / (S.net.fiberpop[p].fiber_count ? S.net.fiberpop[p].fiber_count : 1));
               S.plot[n].val = (double) spikes_per_second_per_fiber / mult;
             }
             break;

           default:
             if (S.plot[n].var < LAST_FIBER) // just in case
             {
               printf("There is a plot type simrun do not know.\n");
               break;
             }
             {  // this is greater than 4, 
                // if > 4, var-4 is is binwidth in ms. can't have a single 
                // case of 4 or greater, so this is the last test
               int mult = c + 1;
               double binwidth_in_ms = S.plot[n].var-4;
                // how many slots in array?
               int spkcntcnt = S.plot[n].var == 4 ? 1 : floor (binwidth_in_ms / S.step + .5);
               int sclidx = S.stepnum % spkcntcnt;
               double spikes_per_second_per_cell;
               if (pop_plot[n].spkcntcnt != spkcntcnt) {
                 free (pop_plot[n].spkcntlst);
                 TCALLOC (pop_plot[n].spkcntlst, spkcntcnt);
                 pop_plot[n].spkcntcnt = spkcntcnt;
               }
               spike_count = 0;
               for (i = 0; i < S.net.cellpop[p].cell_count; i++)
                 spike_count += S.net.cellpop[p].cell[i].spike;
               pop_plot[n].sum += spike_count - pop_plot[n].spkcntlst[sclidx];
               pop_plot[n].spkcntlst[sclidx] = spike_count;
               spikes_per_second_per_cell = (pop_plot[n].sum / (spkcntcnt * S.step / 1000.0)
                         / (S.net.cellpop[p].cell_count ? S.net.cellpop[p].cell_count : 1));
               S.plot[n].val = (double) spikes_per_second_per_cell / mult;
             }
             break;
          }
       }
       simoutsned ();
     }

     if (write_analog)
     {
       static int nanlgtot, nanlgcnt, nanlglst;
       nanlgtot += nf;
       nanlgcnt++;
       if (nanlgcnt == 1 / S.step * S.nanlgrate)
       {
         int nval;
         nanlgtot *= S.sclfct;
         nanlglst = nanlglst * S.dcgint + nanlgtot;
         nval = nanlglst + 2048;
         if (nval > 4095) 
           nval = 4095;
         if (nval < 0) 
           nval = 0;
         int aval = S.nanlgid * 4096 + nval;
         int time = (int)((S.stepnum + 1) * S.step / dt_step);
         if (write_bdt)
         {
           fprintf (S.ofile, fmt, aval, time);
           fprintf (S.ofile, "%c",0x0a);
         }
         if (write_smr)
           writeWave(aval, time); 
         nanlgtot = 0;
         nanlgcnt = 0;
       }
     }

     if (haveLearn)
     {
        decayLearnCells();
        decayLearnFibers();
     }
     chk_for_cmd();
     state = next_state;

  } // END OF MAIN LOOP

  snprintf(msg,sizeof(msg)-1,"TIME\n%.2f\n", S.stepnum/ticks_in_sec);
  if (have_cmd_socket())
    send(sock_fd,&msg,strlen(msg),0); // progress rpt for simbuild 
  fprintf(stdout,"simloop exited\n");
  fflush(stdout);

  if (write_bdt)
  {
    fflush(S.ofile);
    fclose(S.ofile);
    add_IandE();
  }

  if (have_data_socket())
    waitForDone();

/*
#if __linux__
  /* TODO no shell for windows, create a function that does what .sh file does. */
/*  if (write_analog && access("/usr/local/bin/simpower_spectrum.sh", X_OK) == 0)
  {
     char *cmd;
     if (isedt)
     {
        if (asprintf (&cmd, "simpower_spectrum.sh %d %d edt&", S.spawn_number, S.nanlgrate) == -1)
           exit (1);
     }
     else
     {
        if (asprintf (&cmd, "simpower_spectrum.sh %d %d bdt&", S.spawn_number, S.nanlgrate) == -1)
        exit (1);
     }
     fprintf(stdout,"%s\n", cmd);
     if (system (cmd)){} 
        free(cmd);
  }
#endif*/
  fprintf(stdout,"simloop function returning\n");
  fflush(stdout);
}


