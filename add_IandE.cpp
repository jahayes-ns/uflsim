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

/*
  Function to put in I and E respiratory pulses into a bdt or edt file
  using the analog channel as the source of phrenic pulses. 
  Creates a new bdt or edt file.
  I pulse is put in as unit 97
  E pulse as unit 98
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <cstring>
#include <cmath>
#include <fstream>
#include <limits>
#include <vector>
#include <algorithm>
#include <array>
#include <filesystem>
#include <system_error>
#include <ctime>

#ifdef __cplusplus
extern "C" {
#endif
#include "simulator.h"

extern void sendMsg(char*);
extern char outFname[];

using namespace std;

const string merge_ie = "merge_ie.tmp";

const float PI = 3.1416;
const int CHAN_VAL = 0; // array indicies
const int TIME = 1;
const int BDT = 0;
const int MRG = 1;

using Smoother = struct {
   int isamp;
   int ism;
   double up;
   double down;
   int ihz;
};

using oneLine = struct {
   int code;
   int time;
};

using toMerge = vector <oneLine>;

// Default params for two common Hz
Smoother Forty = {400, 51, 0.01, -0.01, 40};
Smoother TwoHundred = {2000, 101, 0.05, -0.05, 200};

string fNami, fNamo;


static bool openFiles(ifstream& infile, fstream& resfile, ofstream& mergefile)
{
   string base, ext, tmpnum;
   time_t now;
   struct tm tnd;
   char tbuf[80];
   size_t dot;

     // To avoid tedious having-to-remember workflow, make the 
     // output name conform to what cth_cluster (and maybe other progs)
     // expect. Use current date to create filename such as
     // 2020-12-17_001_simIE.bdt.
   fNami = outFname;
   dot = fNami.find_last_of(".");
   base = fNami.substr(0,dot);
   ext = fNami.substr(dot,fNami.size()-dot);
   now = time(0);
   tnd = *localtime(&now);
   strftime(tbuf,sizeof(tbuf),"%Y-%m-%d",&tnd);
   fNamo = tbuf;
   fNamo += "_001";
   fNamo += "_simIE";
   fNamo += to_string(S.spawn_number);
   fNamo += ext;

   infile.open(fNami);
   if (!infile.is_open())
   {
      cout << "Could not open input file " << fNami << " cannot create I and E puls file." << endl;
      return false;
   }
   resfile.open(merge_ie, ios_base::in | ios_base::out | ios_base::trunc);
   if (!resfile.is_open())
   {
      cout << "Could not open temporary file " << merge_ie << " cannot create I and E pulse file." << endl;
      infile.close();
      return false;
   }
   mergefile.open(fNamo);
   if (!mergefile.is_open())
   {
      cout << "Could not open merge file " << fNamo << " cannot create I and E pulse file." << endl;
      return false;
   }
   return true;
}

// initialize the smoothing array
static void initSmooth(vector<float>& smooth, Smoother&  smoo)
{
   float accum = 0, rk;
   for (int n = 0; n < smoo.ism; ++n)
   {
     rk =n*2*PI/(smoo.ism-1);
     if ((rk >= 0.0) && (rk <= PI/2.0))
         smooth[n] = (2.0*rk/PI);
     else if ((rk > PI/2.0) && (rk <= 3.0*PI/2.0))
         smooth[n] = (2.0-2.0*rk /PI);
     else if ((rk > 3.0*PI/2.0) &&(rk <= 2.0*PI))
         smooth[n] = (2.0*rk /PI-4);
      accum = accum+abs(smooth[n]);
   }
   for (int n = 0; n < smoo.ism; ++n)
   {
      smooth[n] = smooth[n] / accum;
      // cout << setprecision(6) << fixed << sm[n] << endl;
   }
}

static void readFile(ifstream& infile, vector<vector<int>>& ix)
{
   string line;
   int code, time, val;
   int ichno = S.nanlgid * 4096;

   while (true)  // read in all chan recs
   {
      getline(infile,line);
      if (line.length())
      {
         code = stoi(line.substr(0,5));
         time = stoi(line.substr(5,string::npos));
         if (S.nanlgid == code/4096) // our analog chan
         {
            val = code - ichno;
            if (val > 2047)
               val -= 4096;
            ix[CHAN_VAL].push_back(val);
            ix[TIME].push_back(time);
          }
       }
       else
          break; // eof
   }
}

// for sorting merge list
static bool compareTimes(const oneLine& one, const oneLine& two)
{
   return (one.time < two.time);
}

void add_IandE()
{
   int itics;
   float y;
   int imaxpt, iminpt, iflg;
   int code,time;
   ifstream infile;
   fstream resfile; 
   ofstream mergefile;
   vector<vector<int>> ix;
   vector<float> smooth;
   array<array<int, 2>, 2> look;
   double ticks_in_sec = ceil(1000.0/S.step);
   int curr_start_idx;
   error_code ec;
   int num_lines;
   size_t minSize = 0;
   bool init_vars = true;
   bool done = false;
   string line;
   Smoother  smoo;
   toMerge  mergeMe;
   char msg[2048] = {0};

      // if an older version of .sim file or no params entered
   if (!S.ie_freq || !S.ie_sample || !S.ie_smooth)
   {
      cout << "I and E detection not performed because parameter(s) are zero." << endl;
      snprintf(msg,sizeof(msg)-1, "MSG\nI and E detection not performed because parameter(s) are zero\n");
      sendMsg(&msg[0]);
      return;
   }

   smoo.isamp = S.ie_sample;
   smoo.ism   = S.ie_smooth;
   smoo.up    = S.ie_plus;
   smoo.down  = S.ie_minus;
   smoo.ihz   = S.ie_freq;

   smooth.resize(smoo.ism);
   ix.resize(2);
   itics = ticks_in_sec/smoo.ihz; // # of tics between adc measurements?

      // do set-ups
   if (!openFiles(infile, resfile, mergefile))
         return;
   initSmooth(smooth, smoo);
   readFile(infile,ix);

    // sanity check
   if (int(ix[TIME].size()) < smoo.isamp)
   {
      cout << "Warning: The number of analog samples " << smoo.isamp 
           << " is less that the sample size. I and E detection not performed. " << ix[TIME].size() << endl;
      snprintf(msg,sizeof(msg)-1, "MSG\nWarning: The number of analog samples %ld is less that the sample size %d, I and E detection not performed.\n", ix[TIME].size(), smoo.isamp);
      sendMsg(msg);
      return;
   }
   if (int(ix[TIME].size()) < smoo.ism)
   {
      cout << "Warning: The analog smoothing array size " << smoo.ism 
           << " is less that the sample size, I and E detection not performed " << ix[TIME].size() << endl;
      snprintf(msg,sizeof(msg)-1, "MSG\nWarning: The number of analog samples %ld is less that the smoothing array size %d, and E detection not performed.", ix[TIME].size(), smoo.ism);
      sendMsg(msg);
      return;
   }

   curr_start_idx = 0;
   num_lines = ix[CHAN_VAL].size();
     // if we have codes, add to merge list (will sort it later)
   if (S.p1code)
   {   // convert ms to ticks
      mergeMe.push_back({S.p1code,int(S.p1start/S.step)});
      mergeMe.push_back({S.p1code,int(S.p1stop/S.step)});
      minSize += 2;
   }
   if (S.p2code)
   {
      mergeMe.push_back({S.p2code,int(S.p2start/S.step)});
      mergeMe.push_back({S.p2code,int(S.p2stop/S.step)});
      minSize += 2;
   }

   while (!done)
   {
      if (init_vars)
      {
         imaxpt = iminpt = curr_start_idx-1;
         y = 0;
         iflg = 0;
         init_vars = false;
      }

      if (imaxpt < curr_start_idx)
      {
         imaxpt = curr_start_idx;
         for (int k = curr_start_idx + 1; k < smoo.isamp + curr_start_idx; ++k)
         {
            if (ix[CHAN_VAL][imaxpt] < ix[CHAN_VAL][k])
               imaxpt = k;
         }
      }
      if (iminpt < curr_start_idx)
      {
         iminpt = curr_start_idx;
         for (int k = curr_start_idx + 1; k < smoo.isamp + curr_start_idx; ++k)
         {
            if (ix[CHAN_VAL][iminpt] > ix[CHAN_VAL][k])
               iminpt = k;
         }
      }

       // test for gaps, if so, move along the stream until we're gapless
      if (ix[TIME][smoo.isamp-1] - ix[TIME][smoo.isamp-2] > itics*2)
      {
         cout << "gap detected." << endl;
         init_vars = true;
         ++curr_start_idx;
         continue;
      }

      for (int i = 0; i < smoo.ism; ++i)
      {
         if ((ix[CHAN_VAL][imaxpt] - ix[CHAN_VAL][iminpt])/2.0 != 0)
         {
            y += (float(ix[CHAN_VAL][(curr_start_idx + smoo.isamp-1)-i] /
                 float(ix[CHAN_VAL][imaxpt] - ix[CHAN_VAL][iminpt])/(float)2.0)) * smooth[i];
         }
      }
      if ((y > smoo.up) && (!iflg))
      {
         iflg = 1;
         mergeMe.push_back({97,ix[TIME][(curr_start_idx + smoo.isamp-1)-smoo.ism/2-40]});
      }
      if ((y < smoo.down) && (iflg))
      {
         iflg = 0;
         mergeMe.push_back({98,ix[TIME][(curr_start_idx + smoo.isamp-1)-smoo.ism/2+10]});
      }
      ++curr_start_idx; // move right in array 
      y = 0.0;
   
         // quit when not enough of array left to process
      if (num_lines - curr_start_idx < smoo.isamp) 
         done = true;
   }

   // done processing the files, merge original bdt file and markers 
   if (mergeMe.size() <= minSize)  // Did we get any results?
   {
      cout << "Did not find pulses in the analog channel, no output file created." << endl;
      infile.close();
      resfile.close();
      mergefile.close();
      filesystem::remove(fNamo.c_str(),ec);
      filesystem::remove(merge_ie.c_str(),ec);
      return;
   }

   // sort by time
   std::sort(mergeMe.begin(),mergeMe.end(),compareTimes);
   for (auto iter = mergeMe.begin(); iter != mergeMe.end(); ++iter)
      resfile << right << setw(5) << iter->code << setw(8) << iter->time << endl;

   infile.clear();
   resfile.clear();
   infile.seekg(0,infile.beg);
   resfile.seekp(0,resfile.beg);
    // copy header 
   getline(infile,line);
   code = stoi(line.substr(0,5));
   time = stoi(line.substr(5,string::npos));
   if (code == 0)
      getline(infile,line);
   getline(infile,line);
   code = stoi(line.substr(0,5));
   time = stoi(line.substr(5,string::npos));

   mergefile << right << setw(5) << code << setw(8) << time << endl;
   mergefile << right << setw(5) << code << setw(8) << time << endl;

   getline(infile,line);
   look[BDT][CHAN_VAL] = stoi(line.substr(0,5));
   look[BDT][TIME] = stoi(line.substr(5,string::npos));
   getline(resfile,line);
   look[MRG][CHAN_VAL] = stoi(line.substr(0,5));
   look[MRG][TIME] = stoi(line.substr(5,string::npos));
   while(!infile.eof() || !resfile.eof())
   {
      if (look[BDT][TIME] < look[MRG][TIME])
         iflg = BDT;
      else
         iflg = MRG;
      mergefile << right << setw(5) << look[iflg][CHAN_VAL] << setw(8) << look[iflg][TIME] << endl;
      if (!iflg && !infile.eof())
      {
         getline(infile,line);
         if (line.length())
         {
            look[BDT][CHAN_VAL] = stoi(line.substr(0,5));
            look[BDT][TIME] = stoi(line.substr(5,string::npos));
         }
         else
            look[BDT][TIME] = numeric_limits<int>::max();
      }
      else if (iflg && !resfile.eof())
      {
         getline(resfile,line);
         if (line.length())
         {
            look[MRG][CHAN_VAL] = stoi(line.substr(0,5));
            look[MRG][TIME] = stoi(line.substr(5,string::npos));
         }
         else
            look[MRG][TIME] = numeric_limits<int>::max();
      }
   }

    infile.close();
    resfile.close();
    mergefile.close();
   // filesystem::remove(merge_ie.c_str(),ec); // keep around during testing
}

#ifdef __cplusplus
}
#endif
