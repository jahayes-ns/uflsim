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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <getopt.h>
#include <iostream>
#include <math.h>
#include <string>
#include <sstream>
#include <memory>
#include <chrono>
#include <ctime>
#include "s64.h"
#include "s3264.h"
#include "s32priv.h"

using namespace std;
using namespace ceds64;

// Globals

double sampRate = 2000.0; // hz
double plotTime = 60.0;   // sec
double plotFreq = 3.0;  // 3 times / sec

static void parse_args(int argc, char *argv[])
{
   int cmd;
   static struct option opts[] = 
   {
      {"n", required_argument, NULL, 'r'},
      {"t", required_argument, NULL, 't'},
      {"f", required_argument, NULL, 'f'},
      { 0,0,0,0} 
   };
   while ((cmd = getopt_long(argc, argv, "r:t:f:", opts, NULL )) != -1)
   {
      switch (cmd)
      {
         case 'r':
               sscanf(optarg, "%lf", &sampRate);
               break;
         case 't':
               sscanf(optarg, "%lf", &plotTime);
               break;
         case 'f':
               sscanf(optarg, "%lf", &plotFreq);
               break;
         case '?':
         default:
            cout << "Usage: " << argv[0] << " [-r sample rate in HZ]  [-t length of plot in seconds]  [-f frequency in times per second]" << endl;
           break;
      }
   }
}

int main(int argc, char*argv[])
{
   int res;
   int chan = 0;
#ifdef WIN64
   SONInitFiles();
#endif
   parse_args(argc,argv);

   printf("Sample rate is %lf Hz\n",sampRate);
   printf("Plot time is %lf seconds\n",plotTime);
   printf("Plot frequency is %lf per second\n",plotFreq);

   stringstream file;
   file << "sine_at_f" << plotFreq << "_s" << sampRate << ".smr";
   string outFile = file.str();

   TSon32File sFile(1);
   res = sFile.Create(outFile.c_str(),32);
   if (res == S64_OK)
   {
      sFile.SetTimeBase(1/sampRate);
      res = sFile.SetWaveChan(chan,1,ceds64::TDataKind::RealWave);
      sFile.SetBuffering(chan,0x8000,0);
      if (res != S64_OK)
         cout << "error creating chan " << res << endl;
   }
   else
   {
      cout << "error, res is " << res << endl;
      exit(1);
   }

   float time = sampRate * plotTime;
   float plot;
   TSTime64 currtime = 0;

   // a 3hz wave is sampRate/3 samples 0-2pi radians
   double step = 2*M_PI / (sampRate / plotFreq);
   double start=0.0;
   for (currtime = 0; currtime < time;  ++currtime)
   {
      plot = (sin(start) + 1.0) * 50.0; // translate so values are 0-100
      start += step;
      sFile.WriteWave(chan, &plot, 1, currtime);
   }
   sFile.Close();
#ifdef WIN64
      SONCleanUp();
#endif
}

