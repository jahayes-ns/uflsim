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

#if defined WIN32
#define _CRT_RAND_S
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <float.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include "lin2ms.h"
#include "wavemarkers.h"

#if defined __linux__
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <linux/sockios.h>
#include <netdb.h>
#endif

#if defined WIN32
#include <libloaderapi.h>
#include <ws2tcpip.h>
#include <fcntl.h>
#endif

#include "muParserDLL.h"
#include <unistd.h>
#include <stdbool.h>
#include <getopt.h>
#include <errno.h>
#include <libgen.h>
#include "simulator.h"
#include "simrun_wrap.h"
#include "simrun_wrap.h"
#include "inode.h"

extern int have_cmd_socket();
extern int have_data_socket();
void destroy_cmd_socket();
void destroy_view_socket();
simulator_global S;
int Debug=0;
int write_waves;
int use_socket;
int write_bdt;
int write_analog;
int write_smr;
int write_smr_wave;
int noWaveFiles = 0;
int isedt = false;
char bdt_fmt[] = "%5d%8d";
char edt_fmt[] = "%5d%10d";
char *fmt;
double dt_step= 0.5;
char simmsg[2048];
char sim_fname[1024];
char *script_ptr;
int script_size;
char *sim_ptr;
int sim_size;
char *snd_ptr;
int snd_size;
unsigned short simbuild_port=0;
char host_name[128] = "localhost";
char outFname[2048];
char inPath[2048];
char outPath[2048];
bool haveAff = false;
int condi_flag = 0;
int haveLearn = 0;
int learnCPop[MAX_INODES];
int numCPop;
int learnFPop[MAX_INODES];
int numFPop;

#ifdef __linux__
int sock_fd=0;
int sock_fdout=0;
#elif defined WIN32
WSADATA wsa;
SOCKET sock_fd=INVALID_SOCKET;
SOCKET sock_fdout=INVALID_SOCKET;
#else
#pragma message("WARNING: Unknown environment configuration, not Linux or Windows")
#endif
unsigned short viewerPortNum;

static inline int
nograph (char *line, ssize_t len)
{
  int n;

  for (n = 0; n < len; n++)
    if (isgraph ((unsigned char)line[n]))
      return 0;
  return 1;
}

static inline void
delete_newline (char *line, ssize_t *len)
{
  while (*len > 0 && (line[*len - 1] == '\r' || line[*len - 1] == '\n'))
    line[--*len] = 0;
}

static int get_yes_no (char *prompt) {
  static char *inbuf = 0;
  static size_t len = 0;
  char response;

  do {
    fputs (prompt, stdout);
    fflush (stdout);
    if (getline (&inbuf, &len, stdin) == -1)
      DIE;
  } while ((sscanf (inbuf, "%c", &response) != 1
       || strchr ("yYnN", response) == 0)
      && (isatty (0) || DIE)
      && putchar ('\a'));
  return tolower (response);
}

static int get_int (char *prompt, int min, int max)
{
  static char *inbuf = 0;
  static size_t len = 0;
  int val;

  do {
    fputs (prompt, stdout);
    fflush (stdout);
    if (getline (&inbuf, &len, stdin) == -1)
      exit (1);
  } while ((sscanf (inbuf, "%d", &val) != 1
       || val < min || val > max)
      && putchar ('\a'));
  return val;
}

static double get_dbl (char *prompt, double min, double max)
{
  static char *inbuf = 0;
  static size_t len = 0;
  double val;

  do {
    fputs (prompt, stdout);
    fflush (stdout);
    if (getline (&inbuf, &len, stdin) == -1)
      exit (1);
  } while ((sscanf (inbuf, "%lf", &val) != 1
       || val < min || val > max)
      && (isatty (0) || DIE)
      && putchar ('\a'));
  return val;
}

int sigterm;

static void sigterm_handler (int signum)
{
  fprintf(stdout,"sim got term signal\n");
  fflush(stdout);
  sigterm = 1;
}


void setupViewer()
{
  int conn_try;
  char port_str[64];
  struct sockaddr_in *serv_addr;
  struct addrinfo hints, *ptr;
  struct addrinfo *server = NULL;
  int lookup;

  sprintf(port_str,"%d",viewerPortNum);
  fprintf(stdout,"SIMRUN: Got viewer host: %s, port: %s\n",host_name, port_str);
  fflush(stdout);

  memset(&hints,0,sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  lookup = getaddrinfo(host_name, port_str, &hints, &server);
  if (lookup != 0)
  {
     fprintf(stdout,"SIMRUN: ERROR, can't find viewer host %s  %s.\n",host_name,gai_strerror(lookup));
     freeaddrinfo(server);
     return;
  }

  for (ptr = server; ptr != NULL; ptr = ptr->ai_next)
  {
     sock_fdout = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
     if (sock_fdout == -1)
     {
        perror("socket");
        continue;
     }
     fprintf(stdout,"SIMRUN: Got socket %d\n",sock_fdout);
     fflush(stdout);
     break;
   }
   if (sock_fdout < 0)
   {
#ifdef __linux__
      fprintf(stdout,"Could not open simviewer socket, error is %d\n",errno);
#else
      char *msg, *cmd;
      asprintf (&msg,
               "\"Error opening simviewer socket, error is: %d\n\"",WSAGetLastError());
      asprintf(&cmd, "%s %s %s", simmsg, "\"SIMRUN ERROR\"", msg);
      system(cmd);
      free(msg);
      free(cmd);
#endif
      return;
   }
   conn_try = 60;
   while (conn_try)
    {
      if (connect(sock_fdout,ptr->ai_addr, ptr->ai_addrlen) < 0)
      {
        usleep(500000);
        --conn_try;
        fprintf(stdout,"SIMRUN: No connection with simviewer, try again later\n");
        fprintf(stdout,"errno: %d\n",errno);
        fflush(stdout);
      }
      else
         break;
   }

   if (conn_try)
   {
     fprintf(stdout,"SIMRUN: Connected to simviewer.\n");
     serv_addr = (struct sockaddr_in *)ptr->ai_addr;
     fprintf(stdout,"SIMRUN: Local simviewer connect port is %d\n",serv_addr->sin_port);
     fflush(stdout);
   }
   else
   {
#if __linux__
     fprintf(stdout,"Could not connect to simviewer, error is %d\n",errno);
     fflush(stdout);
#else
     char *msg, *cmd;
     asprintf (&msg,
               "\"Could not connect to simviewer, error is: %d\n\"",WSAGetLastError());
     asprintf(&cmd, "%s %s %s", simmsg, "\"SIMRUN ERROR\"", msg);
     system (cmd);
     free(msg);
     free(cmd);
#endif
     close(sock_fdout);
#ifdef __linux__
     sock_fdout=0;
#else
     sock_fdout=INVALID_SOCKET;
#endif
   }
   freeaddrinfo(server);
}


// If using network connection and it is established, we need to wait
// for the requested items to be received. This can include:
// script?.txt contents
// spawn?.sim contents
// simviewer port address
// Stay here until we get what we want or time out and give up.
// It is not easy to break this up into a function-per-msg because we
// have no control over the order the other side sends stuff. We only 
// know what we need based on some conditions.
// The bool params tells us what to wait for.
const int INITIAL_SIZE = 1024*64;
const int INC_STEP = 1024*16;
bool get_essentials(bool get_socket, bool get_script, bool get_sim, bool get_snd)
{
  bool have_script = !get_script;
  bool have_sim = !get_sim; 
  bool have_socket = !get_socket;
  bool have_snd = !get_snd;
  bool have_start = false;
  char *buff = 0, *curr_buff;
  size_t curr_size = 0;
  size_t alloc_size = INITIAL_SIZE;
  unsigned char curr_id = 0;
  int got;
  unsigned char val;
  int conn_try = 60; // will wait a minute

  while (!have_socket || !have_script || !have_sim || !have_snd)
  {
#ifdef __linux__
    got = recv(sock_fd,&val,sizeof(val),MSG_DONTWAIT);
#else
    got = recv(sock_fd,&val,sizeof(val),0); // was set to nonblocking in setup
    if (got == SOCKET_ERROR)
      got = 0;
#endif
  if (got == 1)
  {
    if (val == MSG_START)
    {
      alloc_size = INITIAL_SIZE;
      buff = malloc(alloc_size);
      curr_buff = buff;
      have_start = true;
      curr_size = 0;
      curr_id = 0;
      continue;
    }

    if (have_start)
    {
      if (val == MSG_END)
      {
        if (curr_id == PORT_MSG)
        {
          unsigned short port;
          if (sscanf(buff,"%hu",&port) == 1)
          {
             viewerPortNum = port;
             have_socket = true;
             fflush(stdout);
             setupViewer();
          }
          free(buff);
        }
        else if (curr_id == SCRIPT_MSG)
        {
          fprintf(stdout,"Got script, %ld bytes\n",curr_size);
          have_script = true;
          script_ptr = buff;
          script_size = curr_size;
        }
        else if (curr_id == SIM_MSG)
        {
          fprintf(stdout,"Got sim, %ld bytes\n",curr_size);
          have_sim = true;
          sim_ptr = buff;
          sim_size = curr_size;
        }
        else if (curr_id == SND_MSG)
        {
          fprintf(stdout,"Got snd, %ld bytes\n",curr_size);
          have_snd = true;
          snd_ptr = buff;
          snd_size = curr_size;
        }
        else
        {
          fprintf(stdout,"Unknown message %c from simbuild\n", curr_id);
        }
        have_start = false;
        curr_id = 0;
        continue;
      }
      if (curr_id == 0) // next byte is msg type
      {
        curr_id = val;
        continue;
      }
      else // accumulate bytes, resize buff in needed
      {
        *curr_buff = val;
        ++curr_buff;
        ++curr_size; 
        if (curr_size == alloc_size)
        {
          alloc_size += INC_STEP;
          buff = realloc(buff,alloc_size);
          curr_buff = buff + curr_size;
         }
       }
     }
   }
   else
   {
      fflush(stdout);
      usleep(1000000);
      --conn_try;
      if (conn_try == 0) // give up
      {
        fprintf(stdout,"Waiting for msg timed out\n");
        fflush (stdout);
#if defined WIN32
        if (use_socket && !have_socket)
        {
          char *msg, *cmd;
          asprintf (&msg,
         "\"Failed to obtain port number from simbuild, error is: %d\n\"",WSAGetLastError());
          asprintf(&cmd, "%s %s %s", simmsg, "\"SIMRUN ERROR\"", msg);
          system (cmd);
          free(msg);
          free(cmd);
        }
#endif
        return false;
      }
    }
  }
  fprintf(stdout,"Got the essentials\n");
  fflush(stdout);
  return true;
}


// Set up as a server with simbuild being a client, if possible (may not be running).
// Also connect as client with simviewer, if possible (may not be running).
bool create_socket()
{
   char port_str[64];
   struct sockaddr_in *serv_addr;
   struct addrinfo hints, *ptr;
   struct addrinfo *server = NULL;
   int lookup;

   sprintf(port_str,"%d",simbuild_port);
   fprintf(stdout,"SIMRUN: simbuild connection host: %s port: %s\n",host_name,port_str);

#if defined WIN32
   if (WSAStartup(MAKEWORD(2,2),&wsa) != 0)
	{
      char *msg, *cmd; // popup an error dialog box
      asprintf (&msg,
               "\"Failed to initialize winsock, error is: %d\n\"",WSAGetLastError());
      asprintf(&cmd, "%s %s %s", simmsg, "\"SIMRUN ERROR\"", msg);
      system (cmd);
      free(msg);
      free(cmd);
		return false;
	}
#endif
   memset(&hints,0,sizeof(hints));
   hints.ai_family = AF_INET;
   hints.ai_socktype = SOCK_STREAM;
   hints.ai_protocol = IPPROTO_TCP;
   lookup = getaddrinfo(host_name, port_str ,&hints, &server);
   if (lookup != 0)
   {
      fprintf(stdout,"SIMRUN: ERROR, can't find simbuild host %s  %s.\n",host_name,gai_strerror(lookup));
      freeaddrinfo(server);
      return false;
   }

   for (ptr = server; ptr != NULL; ptr = ptr->ai_next)
   {
      sock_fd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
      if (sock_fd == -1)
      {
         perror("socket");
         continue;
      }
      break;
   }
   if (connect(sock_fd,ptr->ai_addr, ptr->ai_addrlen) < 0)
   {
     fprintf(stdout,"SIMRUN: ERROR connecting to simbuild\n");
     fprintf(stdout,"errno: %d\n",errno);
     fflush(stdout);
     destroy_cmd_socket();
     freeaddrinfo(server);
     return false;
   }
   else
   {
     fprintf(stdout,"SIMRUN: Connected to simbuild.\n");
     serv_addr = (struct sockaddr_in *)ptr->ai_addr;
     fprintf(stdout,"SIMRUN: Connect port is %d\n",serv_addr->sin_port);
     fflush(stdout);
     freeaddrinfo(server);
   }
#if defined WIN32
   long nb= 1;
   ioctlsocket(sock_fd,FIONBIO,&nb);  // set to non-blocking reads
   fprintf(stdout,"Connected to simbuild\n");
   fflush(stdout);
#endif
   return true;
}

void destroy_cmd_socket()
{
#ifdef __linux__
   if (sock_fd)
   {
      shutdown(sock_fd, SHUT_WR);
      close(sock_fd);
      sock_fd = 0;
   }
#elif defined WIN32
   if (sock_fd != INVALID_SOCKET)
   {
      closesocket(sock_fd);
      sock_fd = INVALID_SOCKET;
   }
#endif
}


// Later: modified to use blocking sockets. We never get here until simrun
// tells it it got the EOF packet. Closing sockets discards buffered packets.
void destroy_view_socket()
{
#ifdef __linux__
  if (sock_fdout)
  {
    close(sock_fdout);
    sock_fdout = 0;
  }
#elif defined WIN32
  if (sock_fdout != INVALID_SOCKET)
  {
    closesocket(sock_fdout);
    sock_fdout = INVALID_SOCKET;
  }
#endif
  use_socket = false;
}

void destroy_sockets()
{
  destroy_view_socket();
  destroy_cmd_socket();
#if defined WIN32
  WSACleanup();
#endif
}

static void interactive()
{
  char *ifile_name = 0;
  size_t ifname_len = 0;
  char *ofile_name = 0;
  size_t ofname_len = 0;
  char *inbuf = 0;
  size_t len = 0;
  ssize_t read;

  int really;
  printf("No input script provided, you must enter all script values manually.\n");
  really = get_yes_no("ARE YOU REALLY SURE YOU WANT TO DO THIS? (Y/N)");
  if (really != 'y')
     exit(1);

  do 
  {
    printf ("\n\n\n\n\n\n\n\n\n\n  ENTER INPUT FILENAME ...OR... <CR> TO EXIT  >> ");
    fflush (stdout);
    read = getline (&ifile_name, &ifname_len, stdin);
    delete_newline (ifile_name, &read);
    if (nograph (ifile_name, read)) 
      exit (1);
  } while ((S.ifile = fopen (ifile_name, "r")) == 0
      && printf ("\n       ***** ERROR OPENING INPUT FILE *****\n")
      && (isatty (0) || DIE)
      );
  fclose(S.ifile);
  S.input_filename = ifile_name;
  strcpy(sim_fname, ifile_name);
  read_sim();
  printf ("update parameters every how many steps? (0 for never)\n");
  fflush (stdout);
  if (getline (&inbuf, &len, stdin) == -1)
    exit (1);
  sscanf (inbuf, "%d", &S.update_interval);
  if (S.update_interval < 0)
    exit (1);

  do 
  {
    printf ("\n\n\n\n\n\n\n\n\n\n          E -- PLOT MEMBRANE POTENTIALS ON VIEWER\n");
    printf ("         <CR> -- SKIP\n");
    fflush (stdout);
    if ((read = getline (&inbuf, &len, stdin)) == -1)
      exit (1);
    if (nograph (inbuf, read))
      goto SPIKETIMES;
  } while ((sscanf (inbuf, "%c", &S.outsned) != 1 || tolower (S.outsned) != 'e')
      && (isatty (0) || DIE)
      && putchar ('a'));

  S.outsned = tolower (S.outsned);
  if (S.outsned == 'e') 
  {
    S.spawn_number = get_int ("What spawn number? ", 0, INT_MAX);

    printf ("\n\n\n\n\n\n  ENTER POPULATION ID CODE AND UNIT NUMBER AND VAR OF CELLS WHOSE STATE\n");
    printf ("  WILL BE PLOTTED:  (pop,cell,var)\n");
    while (1) 
    {
      int scan_count, pop, cell, var, n;
      printf ("                    CELL #%d >> ", S.plot_count + 1);
      fflush (stdout);
      if ((read = getline (&inbuf, &len, stdin)) == -1)
        exit (1);
      if (nograph (inbuf, read))
   break;
      scan_count = sscanf (inbuf, "%d , %d%n , %d%n , %n", &pop, &cell, &n, &var, &n, &n);
      if (scan_count == 2)
   var = 1;
      else if (scan_count != 3
               || (var > 0 && (pop > S.net.cellpop_count || pop < 1))
               || var == 0
               || var < -16) {
   printf ("\abad input data: %s", inbuf);
   printf ("scan_count = %d, pop = %d, cellpop_count = %d\n", scan_count, pop, S.net.cellpop_count);
   continue;
      }
      TREALLOC (S.plot, ++S.plot_count);
      S.plot[S.plot_count - 1].pop = pop;
      S.plot[S.plot_count - 1].cell = cell;
      S.plot[S.plot_count - 1].var = var;
      S.plot[S.plot_count - 1].type = 0;
      S.plot[S.plot_count - 1].val = 0;
      S.plot[S.plot_count - 1].spike = 0;
      {
        int e = strlen (inbuf) - 1;
        while (e > 0 && !isgraph (inbuf[e]))
          inbuf[e--] = 0;
        S.plot[S.plot_count - 1].lbl = strdup (inbuf + n);
      }
    }
    if (S.plot_count == 0)
       S.outsned = ' ';
  }

SPIKETIMES:

  S.save_spike_times = get_yes_no ("\n\n\n\n\n\n\n\n\n\n  SAVE SPIKE TIMES TO BDT/EDT FILE? (Y/N)  ");
  S.save_smr = get_yes_no ("\n\n\n\n\n\n\n\n\n\n  CREATE SMR FILE WITH BDT VALUES? (Y/N)  ");
  S.save_smr_wave = get_yes_no ("\n\n\n\n\n\n\n\n\n\n  CREATE SMR FILE WITH WAVEFORM VALUES? (Y/N)  ");
  
  if (S.save_spike_times == 'y' || S.save_smr == 'y') 
  {
    S.save_pop_total = get_yes_no ("\n  SAVE POPULATION TOTAL TO DISK? (Y/N)  ");
    if (S.save_pop_total == 'y') 
    {
      double tcint;
      S.nanlgid = get_int ("  ENTER ANALOG ID (1-3) ", 1, 15);
      S.nanlgpop = get_int ("  ENTER POP. NO. ", 1, S.net.cellpop_count);
      S.nanlgrate = get_int ("  ENTER RATE (/sec.) ", 1, 1000);
      S.nanlgrate = 1000 / S.nanlgrate;
      tcint = get_dbl ("  ENTER TIME CONSTANT ", DBL_MIN, DBL_MAX);
      S.sclfct = get_dbl ("  ENTER SCALING FACTOR ", -DBL_MAX, DBL_MAX);
      S.dcgint = exp (-S.nanlgrate / tcint);
    }

    do 
    {
      printf ("  ENTER OUTPUT FILENAME  ");
      fflush (stdout);
      read = getline (&ofile_name, &ofname_len, stdin);
      delete_newline (ofile_name, &read);
    } while ((nograph (ofile_name, read)
         || (S.ofile = fopen (ofile_name, "wb")) == 0)
        && printf ("\n       ***** ERROR OPENING OUTPUT FILE, FILE EXISTS *****\n")
        && (isatty (0) || DIE)
        );
    strncpy(outFname,ofile_name,sizeof(outFname)-1);
    if (S.save_pop_total == 'y') 
    {
#ifdef __linux__
       if (strcasestr(ofile_name,".edt"))
#else
       if (StrStrIA(ofile_name,".edt"))
#endif
          isedt=true;
      if (isedt)
      {
         fmt = edt_fmt;
         dt_step = 0.1;
         fprintf (S.ofile, fmt, 33, 3333333);
         fprintf (S.ofile, "%c",0x0a);
         fprintf (S.ofile, fmt, 33, 3333333);
         fprintf (S.ofile, "%c",0x0a);
      }
      else
      {
         fmt = bdt_fmt;
         dt_step = 0.5;
         fprintf (S.ofile, fmt, 11, 1111111);
         fprintf (S.ofile, "%c",0x0a);
         fprintf (S.ofile, fmt, 11, 1111111);
         fprintf (S.ofile, "%c",0x0a);
      }
    }
    printf ("  ENTER I.D. CODES OF CELLS AND FIBERS WHOSE SPIKE TIMES WILL BE INCLUDED\n"
       "  IN THE OUTPUT FILE.\n"
       "  FORMAT: F(iber) or C(ell), POPULATION ID, CELL/FIBER ID <CR>  (A1I5,I5)\n\n"
       "  NOTE: NO COMMA ALLOWED BETWEEN F/C AND POPULATION ID\n");
    while (1) 
    {
      int scan_count, pop, cell;
      static int channel = 1;
      char ptyp;
      printf ("          CHANNEL # %d >> ", channel);
      fflush (stdout);
      if ((read = getline (&inbuf, &len, stdin)) == -1)
        exit (1);
      if (nograph (inbuf, read))
        break;
      scan_count = sscanf (inbuf, " %c %d , %d", &ptyp, &pop, &cell);
      if (scan_count == 3 && pop == 0 && cell == 0)
        break;
      if (scan_count != 3 || strchr ("FfCc", ptyp) == 0) {
        printf ("\abad input data\n");
        continue;
      }
      if (tolower (ptyp) == 'f') 
      {
        TREALLOC (S.fwrit, ++S.fwrit_count);
        S.fwrit[S.fwrit_count - 1].pop = pop;
        S.fwrit[S.fwrit_count - 1].cell = cell;
      }
      else 
      {
        TREALLOC (S.cwrit, ++S.cwrit_count);
        memset (&S.cwrit[S.cwrit_count - 1], 0, sizeof *S.cwrit);
        S.cwrit[S.cwrit_count - 1].pop = pop;
        S.cwrit[S.cwrit_count - 1].cell = cell;
      }
      channel++;
    }
  }
}


// The script and sim file are in memory referenced by file descriptors.
// Read the lines of text without prompts.  This assumes this comes from
// simbuild/launcher and there are no errors in the text.
static void non_interactive(FILE* script)
{
  char *ifile_name = 0;
  size_t ifname_len = 0;
  char *ofile_name = 0;
  size_t ofname_len = 0;
  char *inbuf = 0;
  size_t len = 0;
  ssize_t read;
  char response;

  printf("Reading script\n");
  fflush(stdout);
  read = getline (&ifile_name, &ifname_len, script);
  delete_newline (ifile_name, &read);
  if (strlen(inPath) > 0)
  {
     char *tmp = basename(ifile_name);
     char *fname;
     asprintf(&fname,"%s%s",inPath,tmp);
     S.input_filename = fname;
     strcpy(sim_fname, fname);
  }
  else
  {
     S.input_filename = ifile_name;
     strcpy(sim_fname, ifile_name);
  }
  read_sim();
  getline (&inbuf, &len, script);
  sscanf (inbuf, "%d", &S.update_interval);
  do {
       read = getline (&inbuf, &len, script);
       if (nograph (inbuf, read))
         goto SPIKETIMES;
  } while ((sscanf (inbuf, "%c", &S.outsned) != 1 || tolower (S.outsned) != 'e'));

  S.outsned = tolower (S.outsned);
  if (S.outsned == 'e') 
  {
    getline (&inbuf, &len, script);
    sscanf (inbuf, "%d", &S.spawn_number);

    while (1) 
    {
      int scan_count, pop, cell, var, type, val, spike;
      read = getline (&inbuf, &len, script);
      if (nograph (inbuf, read)) // expect blank line at end
        break;
       scan_count = sscanf (inbuf, "%d , %d%n , %d%n , %n", &pop, &cell, &val, &var, &type, &spike);
      if (scan_count == 2)
        var = 1;
      else if (scan_count != 3
               || (var > 0 && (pop > S.net.cellpop_count || pop < 1))
               || var == 0
               || var < -23) {
         continue;
      }
      TREALLOC (S.plot, ++S.plot_count);
      S.plot[S.plot_count - 1].pop = pop;
      S.plot[S.plot_count - 1].cell = cell;
      S.plot[S.plot_count - 1].var = var;
      S.plot[S.plot_count - 1].type = type;
      S.plot[S.plot_count - 1].val = val;
      S.plot[S.plot_count - 1].spike = spike;
      {
        int e = strlen (inbuf) - 1;
        while (e > 0 && !isgraph (inbuf[e]))
          inbuf[e--] = 0;
        S.plot[S.plot_count - 1].lbl = strdup (inbuf + spike);
      }
    }
    if (S.plot_count == 0)
       S.outsned = ' ';
  }


SPIKETIMES:
  getline (&inbuf, &len, script);            // save bdt?
  sscanf (inbuf, "%c", &response);
  S.save_spike_times = tolower (response);
  getline (&inbuf, &len, script);           // save bdt info to smr?
  sscanf (inbuf, "%c", &response);
  S.save_smr = tolower (response);
  getline (&inbuf, &len, script);           // make an smr of wave info?
  sscanf (inbuf, "%c", &response);
  S.save_smr_wave = tolower (response);

  if (S.save_spike_times == 'y' || S.save_smr == 'y' || S.save_smr_wave == 'y') 
  {
    getline (&inbuf, &len, script);
    sscanf (inbuf, "%c", &response);
    S.save_pop_total = tolower (response);

    if (S.save_pop_total == 'y')             // save analog?
    {
      double tcint;
      getline (&inbuf, &len, script);
      sscanf (inbuf, "%d", &S.nanlgid);
      getline (&inbuf, &len, script);
      sscanf (inbuf, "%d", &S.nanlgpop);
      getline (&inbuf, &len, script);
      sscanf (inbuf, "%d", &S.nanlgrate);
      S.nanlgrate = 1000 / S.nanlgrate;
      getline (&inbuf, &len, script);
      sscanf (inbuf, "%lf", &tcint);
      getline (&inbuf, &len, script);
      sscanf (inbuf, "%lf", &S.sclfct);
      S.dcgint = exp (-S.nanlgrate / tcint);
    }

    read = getline (&ofile_name, &ofname_len, script);
    delete_newline (ofile_name, &read);
    sprintf(outFname,"%s%s",outPath,ofile_name);

    if (S.save_spike_times == 'y') 
    {
#ifdef __linux__
       if (strcasestr(ofile_name,".edt"))
#else
       if (StrStrIA(ofile_name,".edt"))
#endif
        isedt=true;
      S.ofile = fopen (outFname, "wb");
      if (isedt)
      {
         fmt = edt_fmt;
         dt_step = 0.1;
         fprintf (S.ofile, fmt, 33, 3333333);
         fprintf (S.ofile, "%c",0x0a);
         fprintf (S.ofile, fmt, 33, 3333333);
         fprintf (S.ofile, "%c",0x0a);
      }
      else
      {
         fmt = bdt_fmt;
         dt_step = 0.5;
         fprintf (S.ofile, fmt, 11, 1111111);
         fprintf (S.ofile, "%c",0x0a);
         fprintf (S.ofile, fmt, 11, 1111111);
         fprintf (S.ofile, "%c",0x0a);
      }
    }

    while (1) 
    {
      int scan_count, pop, cell;
      static int channel = 1;
      char ptyp;


      read = getline (&inbuf, &len, script);
      if (nograph (inbuf, read))
        break;
      scan_count = sscanf (inbuf, " %c %d , %d", &ptyp, &pop, &cell);
      if (scan_count == 3 && pop == 0 && cell == 0)
        break;
      if (scan_count != 3 || strchr ("FfCc", ptyp) == 0) {
        continue;
      }
      if (tolower (ptyp) == 'f') 
      {
        TREALLOC (S.fwrit, ++S.fwrit_count);
        S.fwrit[S.fwrit_count - 1].pop = pop;
        S.fwrit[S.fwrit_count - 1].cell = cell;
      }
      else 
      {
        TREALLOC (S.cwrit, ++S.cwrit_count);
        memset (&S.cwrit[S.cwrit_count - 1], 0, sizeof *S.cwrit);
        S.cwrit[S.cwrit_count - 1].pop = pop;
        S.cwrit[S.cwrit_count - 1].cell = cell;
      }
      channel++;
    }
  }
  printf("Got script\n");
}

void usage(char* name)
{
   printf("usage %s [--script [optional path]script_name] [--condi] [--file | --socket --port port number] [--bdt] [--smr] [--wave] [--output optional output path]\n"
         "where:\n"
         "--condi create condi csv files\n"
         "--file reads .sim and .snd files\n"
         "-- socket --port number uses network connection at port number\n"
         "--bdt creates a bdt/edt file\n"
         "--smr creates a Spike2 file that contains bdt information\n"
         "--wave creates a Spike2 file that contains waveforminformation\n"
         "--output saves the files in the output path\n"
         ,name);

}

const static struct option long_options[] =
{
   {"condi",no_argument,&condi_flag,1},
   {"debug",no_argument,&Debug,1},
   {"nonoise",no_argument,&S.nonoise,1},  // this seems to be obsolete or for testing
   {"port",required_argument,0,'p'},
   {"host",required_argument,0,'r'},
   {"script",required_argument,0,'s'},
   {"output",required_argument,0,'o'},
   {"file",no_argument,&write_waves,1},
   {"socket",no_argument,&use_socket,1},
   {"bdt",no_argument,&write_bdt,1},
   {"smr",no_argument,&write_smr,1},
   {"wave",no_argument,&write_smr_wave,1},
   {"help",no_argument,0,'h'},
   {"h",no_argument,0,'h'},
   {0,0,0,0}
};

int main (int argc, char **argv)
{
  int c;
  bool have_script=false;
  char scriptname[1024]={0};
  char *tmp_path;
  FILE *script;

#if defined WIN32
  freopen("siminfo.txt","w",stdout);
#endif

  fprintf(stdout,"SIMRUN version %s\n",VERSION);
  fflush(stdout);

  while (1)
  {
     int option_index = 0;
     c = getopt_long(argc, argv, "hsocp", long_options, &option_index);
     if (c == -1)
        break;
     switch(c)
     {
        case 'p':
           if (optarg)
           {
              sscanf(optarg, "%hd",&simbuild_port);
              fprintf(stdout,"SIMRUN: Got simbuild port: %d\n",simbuild_port);
           }
           break;
        case 's':
           if (optarg)
           {
              strcpy(scriptname,optarg);
              tmp_path = dirname(optarg);
              strcpy(inPath, tmp_path);
              strcat(inPath,"/");
              have_script=true;
           }
           break;
        case 'o':
           if (optarg)
           {
              strcpy(outPath,optarg);
              if (outPath[strlen(outPath)-1] != '/')
                 strcat(outPath,"/");
           }
           break;
        case 'r':
           // experimental, users don't see this yet
           if (optarg)
           {
             memset((void*) &host_name,0,sizeof(host_name));
             strncpy(host_name,optarg,sizeof(host_name)-1);
             fprintf(stdout,"SIMRUN: Got host name %s\n",host_name);
           }
           break;
        case 'h':
           usage(argv[0]);
           exit(1);
           break;

        default:
           break;
     }
  }

  if (strlen(inPath) > 0 && strlen(outPath) == 0)
     strcpy(outPath,inPath);

#if defined WIN32
  // Windows does not have standard locations that are always on the PATH.
  // Many users have no idea how to modify their PATH. Find out dir we 
  // are running from in case we have to run the simmsg app.
  SONStart();  // using static link for windows, so have to do this
  char path[MAX_PATH];
  GetModuleFileNameA(NULL,path,sizeof(path)-1);
  for (int len = strlen(path); len > 0; --len)
     if (path[len] != '\\'  && path[len] != '/')
        path[len] = 0;
     else
        break;
   sprintf(simmsg,"start \"\" \"%ssimmsg.exe \"",path);
   printf("simmsg cmd is %s\n",simmsg);
   fflush(stdout);
#else
   strcpy(simmsg,"simmsg");
#endif

   // write wave files or send directly to simview?
  if (write_waves && use_socket) // only one of these can be set
  {
     fprintf(stdout,"WARNING: Only one of --file or --socket can be used.\nSetting file as the default.");
#if defined WIN32
     char *msg, *cmd;
     asprintf(&msg,"WARNING: Only one of --file or --socket can be used\nSetting file as the default.");
     asprintf(&cmd, "%s %s %s", simmsg, "\"SIMRUN ERROR\"", msg);
     system (cmd);
     free(msg);
     free(cmd);
#endif
     use_socket = false;
  }

  if (simbuild_port != 0)
    create_socket();

  if (have_cmd_socket() && get_essentials(use_socket,true,true,true)) // Will get files via socket
  {
#if defined __linux__
    script = fmemopen(script_ptr,script_size,"r");
    non_interactive(script);
#else
    // Windows has no easy way (if any) to turn a buffer in memory
    // into a FILE *. The best we can do is create a temp file that
    // will live in memory (unless it is too big) and goes away when we close it.
    char namebuff[MAX_PATH];
    FILE *fp;
    unsigned int randno;
    HANDLE hnd1, hnd2;

    rand_s(&randno);
    snprintf(&namebuff,sizeof(namebuff),"simrun%x.tmp",randno);
    printf("Temp name is %s\n",namebuff);
    hnd1 = CreateFile(namebuff, GENERIC_READ | GENERIC_WRITE, 0, NULL, 
      CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    if (hnd1 == -1)
      printf("create file failed\n");
    fflush(stdout);

    hnd2 = _open_osfhandle((intptr_t)hnd1,_O_RDONLY);
    fp = _fdopen(hnd2,"r+");
    if (!fp)
      printf("temp file failed\n");
    else
      printf("temp file okay\n");
    fwrite(script_ptr,1, script_size, fp);
    rewind(fp);
    non_interactive(fp);
    fclose(fp);
    close(hnd1);
    close(hnd2);
#endif
    fclose(script);
    free(script_ptr);
    script_ptr = 0;
    script_size = 0;
  }
  else if (have_script) // Direct connection or read from file? Socket over-rides script
  {
    printf("Using script file\n");
    fflush(stdout);
    script = fopen(scriptname,"r");
    if (script)
    {
       non_interactive(script);
       fclose(script);
    }
    else
    {
       printf("Cannot open script file %s, exiting program. . .\n",scriptname);
       exit(1);
    }
    // JAH: if simrun is run with a script it no longer produces the wave* files
    noWaveFiles = true;
  }
  else
     interactive();
  signal (SIGTERM, sigterm_handler);
  if (S.save_smr == 'y')
  {
     write_smr = true;
     openSpike();
  }
  if (S.save_smr_wave == 'y')
  {
     write_smr_wave = true;
     openSpikeWave();
  }
  if (S.save_spike_times == 'y')
     write_bdt = true;
  if (S.save_pop_total == 'y')
     write_analog = true;

  simloop(); // run the simulation

  if (simbuild_port != 0)
    destroy_sockets();
  closeSpike();
#if defined WIN32
  SONStop();
#endif
  return 0;
}
