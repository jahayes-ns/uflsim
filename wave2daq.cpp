/*
Copyright 2005-2020 Kendall F. Morris

This file is part of a collection of recording processing software.

    The is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    The suite is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with the suite.  If not, see <https://www.gnu.org/licenses/>.
*/



/*
   Read all the wave files in the current dir and make .daq file(s).
*/


#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <unistd.h>
#ifdef __linux__
#include <linux/limits.h>
#endif
#include <getopt.h>

#ifdef WIN32
#include <windows.h>
#endif

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <array>

using namespace std;

const int CHANS_PER_FILE = 64;
const int DAQ_BUFF_SIZ = 66;
const int DAQ_DATA_SIZ = 64;
const int wordsPerSamp = 66;
const int bytesPerSamp = wordsPerSamp * sizeof(short); // 2 words header, 64 words data
const int dataPerSamp = 64 * 2;  // 64 words data
using oneSample = array <short, wordsPerSamp>;



string DAQ_EXT(".daq");


// globals
int launchNum = 0;
int numChans = 0;
int numSteps = 0;
float stepSize = 0;
int currSeq = 0;
bool Debug = false;
string outName("waves.daq");


static void usage(char * name)
{
   printf (
"\nUsage: %s "\
"[-v launch number] -o outname\n"\
"Read all the wave.launch_number.???? files and make .daq file(s).\n"\
,name);
}

static void parse_args(int argc, char *argv[])
{
   static struct option opts[] = { 
                                   {"l", required_argument, NULL, 'l'},
                                   {"o", required_argument, NULL, 'o'},
                                   {"h", required_argument, NULL, 'h'},
                                   { 0,0,0,0} };
   int cmd;
   opterr = 0;
   string arg;
   while ((cmd = getopt_long_only(argc, argv, "", opts, NULL )) != -1)
   {
      switch (cmd)
      {
         case 'v':
               arg = optarg;
               launchNum = stoi(arg);
               break;
         case 'o':
               outName= optarg;
               break;
         case 'h':
         case '?':
         default:
            usage(argv[0]); 
            exit(1);
            break;
      }
   }
}


static bool findFirst()
{
   size_t maxname;
#if defined __linux__
   maxname = NAME_MAX;
#elif defined WIN32
   maxname = _MAX_FNAME;
#endif
   char nextname[maxname];
   stringstream waveStrm;
   char inbuf[80] = {0};

   for (int seq = 0; seq <= 9999; ++seq)
   {
      sprintf(nextname,"wave.%02d.%04d",launchNum, seq);
      if (access (nextname, R_OK) == 0)
      {
         currSeq = seq;
//         cout << "Starting with file " << nextname << endl;

         ifstream fStream(nextname);
         if (!fStream)
            cerr << "***********Error opening " << nextname << endl;
         if (!fStream.good())
         {
            cerr << "***********Problem with " << nextname << endl;
            cerr << "bad bit " << fStream.bad() << "eof bit " << fStream.eof()
                 << "fail bit: " << fStream.fail() << endl;
            fStream.close();
            return false;
         }

         waveStrm << fStream.rdbuf();
         if (!waveStrm.getline(inbuf, sizeof(inbuf))) 
         {
            cerr << "Error reading first line of " << nextname << endl;
            return false;
         }
         if (sscanf (inbuf, "%d %f", &numSteps, &stepSize) != 2)
         {
            cerr << "Error reading first line of " << nextname << endl;
            return false;
         }
         if (!waveStrm.getline (inbuf, sizeof(inbuf)))
         {
            cerr << "Error reading second line of " << nextname << endl;
            return false;
         }
         if (sscanf(inbuf,"%d",&numChans) != 1)
         {
            cerr << "Error reading second line of " << nextname << endl;
            return false;
         }
//         cout << "Found " << numChans << " channels" << endl;

         fStream.close();
         return true;
      }
   }
   return false;
}

/*
   The values in the wave file are millivolts. The values in the
   daq file are 2's complement words where 0xFFFF is 2.5 v, 0x8000 is 0 v 
   and 0x0000 is -2.5v.  
*/
static void makeDaq()
{
   size_t maxname;
#if defined __linux__
   maxname = NAME_MAX;
#elif defined WIN32
   maxname = _MAX_FNAME;
#endif
   char nextname[maxname];
   int skip, sample, block;
   float mv;
   char inbuf[80] = {0};
   int idx;
   int val;
   oneSample daq;
   ofstream daqStream(outName);

   for (int seq = currSeq; seq <= 9999; ++seq)
   {
      sprintf(nextname,"wave.%02d.%04d",launchNum, seq);
      ifstream fStream(nextname);
      stringstream waveStrm;
      if (!fStream) // last file
      {
         cout << "Found " << seq+1 << " wave files." << endl;
         return;
      }
      waveStrm << fStream.rdbuf();
        // skip to first data
      for (skip = 0; skip < 2 + numChans; ++skip)
         waveStrm.getline(inbuf, sizeof(inbuf));

      for (block = 0; block < numSteps; ++block)
      {
         daq.fill(0x8000); 
         idx = 0;
         daq[idx++] = 0;
         daq[idx++] = 0;
         for (sample = 0; sample < numChans; ++sample)
         {
            waveStrm.getline(inbuf, sizeof(inbuf));
            if (sscanf(inbuf,"%f",&mv) != 1)
               cout << "failed to read " << inbuf << endl;
            val = mv * 1000 + 0x8000;
            if (val > 0xffff)
            {
               val = 0xffff;
               cout << " ** POS CLIP" << endl;
            }
            else if (val < 0)
            {
               val = 0;
               cout << " ** NEG CLIP" << endl;
            }
            daq[idx++] = short(val);
         }
         daqStream.write((const char*)daq.data(),sizeof(daq));
      }
      fStream.close();
   }
}
/*
   arg is launch number, defaults to 0
   Read first wave file to get number of chans. 
   If < 64, only need to write the .64 file.
   Skip labels, can't use them.

*/
int main (int argc, char **argv)
{
   string tmp_num;

   cout << argv[0] << "Version: " << VERSION << endl;
   parse_args(argc, argv);
   if (!findFirst())
      exit(1);
   makeDaq();
   return 0;
}
