/*
       Program to put in I and E respiratory pulses
       I pulse is put in as unit 97
       E pulse as unit 98
       This is a c++ port of the original fortran rplssim.f
       It uses a dynamic vector of vectors instead of fixed arrays 
       and reads all the analog chan we are interested in a one time, 
       then processes the in-memory array.
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <getopt.h>
#include <string>
#include <cstring>
#include <fstream>
#include <limits>
#include <vector>
#include <array>

using namespace std;

const string tmpname = "eraseme_p.tmp";

// Default parameters for 200 Hz
int isamp = 2000;
int ism = 101; 
int ihz = 200;
float up = .05; 
float dn = -.05;
const float PI = 3.1416;
const int CHAN_VAL = 0; // array indicies
const int TIME = 1;
const int INP = 0;
const int MRG = 1;
#if 0
isamp = 400;
ism = 51; 
ihz = 40;
up = .01; 
dn = -.01;
#endif
using twoD = vector<vector<int>>;

void add_markers()
{
   string fNami, fNamo, tmpnum;
   int ichn, ichno, itics;
   float y;
   int imaxpt, iminpt, iflg;
   int val;
   int i, j, k;
   ifstream infile;
   fstream resfile; 
   ofstream mergefile;
   vector<vector<int>> ix;
   vector<float> smooth;
   array<array<int, 2>, 2> look;
   int curr_start_idx;
   int num_lines;

   smooth.resize(ism);
   ix.resize(2);

   // we need the size of the smoothing array, the critical
   // slopes and sampling rate as parameters
   // how many tics of the clock between adc measurements?
  itics = 2000/ihz;

   // initialize the smoothing array
   float accum = 0, rk;
   for (int n = 0; n < ism; ++n)
   {
    rk =n*2*PI/(ism-1);     // check the - 1 if it is an index thing
    if ((rk >= 0.0) && (rk <= PI/2.0))
         smooth[n] = (2.0*rk/PI);
    else if ((rk > PI/2.0) && (rk <= 3.0*PI/2.0))
         smooth[n] = (2.0-2.0*rk /PI);
    else if ((rk > 3.0*PI/2.0) &&(rk <= 2.0*PI))
         smooth[n] = (2.0*rk /PI-4);
      accum = accum+abs(smooth[n]);
   }
   for (int n = 0; n < ism; ++n)
      smooth[n] = smooth[n] / accum;
   
   cout << "Enter input file name, <CR> to exit: ";
   getline(cin,fNami);
   if (!fNami.length())
      exit(0);
   cout <<  "Which analog channel #? ";
   getline(cin,tmpnum);
   ichn = stoi(tmpnum);
   ichno = ichn * 4096;
   cout << endl;

   infile.open(fNami);
   if (!infile.is_open())
   {
      cout << "Could not open " << fNami << " exiting. . ." << endl;
      exit(1);
   }
   resfile.open(tmpname, ios_base::in | ios_base::out | ios_base::trunc);
   if (!resfile.is_open())
   {
      cout << "Could not open temporary file " << tmpname << " " << resfile.rdstate() << endl;
      infile.close();
      exit(1);
   }
   bool startover = true;
   bool done = false;
   string line;
   while (true)  // read in all chan recs
   {
      getline(infile,line);
      if (line.length())
      {
         i = stoi(line.substr(0,5));
         j = stoi(line.substr(5,string::npos));
         if (ichn == i/4096) // our analog chan
         {
            val = i - ichno;
            if (val > 2047)
               val -= 4096;
            ix[CHAN_VAL].push_back(val);
            ix[TIME].push_back(j);
         }
       }
         else
            break;
   }
   curr_start_idx = 0;
   num_lines = ix[CHAN_VAL].size();
   while (!done )
   {
      if (startover)
      {
         imaxpt = iminpt = curr_start_idx-1;
         y = 0;
         iflg = 0;
         startover = false;
      }

      if (imaxpt < curr_start_idx)
      {
         imaxpt = curr_start_idx;
         for (k = curr_start_idx + 1; k < isamp + curr_start_idx; ++k)
         {
          // cout << "compare max " << ix[CHAN_VAL][imaxpt] << " " << ix[CHAN_VAL][k] << endl;
          if (ix[CHAN_VAL][imaxpt] < ix[CHAN_VAL][k])
               imaxpt = k;
         }
      }
      if (iminpt < curr_start_idx)
      {
         iminpt = curr_start_idx;
         for (k = curr_start_idx + 1; k < isamp + curr_start_idx; ++k)
         {
          // cout << "compare min " << ix[CHAN_VAL][iminpt] << " " << ix[CHAN_VAL][k] << endl;
          if (ix[CHAN_VAL][iminpt] > ix[CHAN_VAL][k])
               iminpt = k;
         }
       }
       //cout << "curr_start_idx " << curr_start_idx << endl;
       //cout << "max idx: " << imaxpt << endl;
       //cout << "min idx: " << iminpt << endl;

      // test for gaps
      if (ix[TIME][isamp-1] - ix[TIME][isamp-2] > itics*2)
      {
         cout << "gap detected." << endl;
         startover = true;
         continue;
      }

      for (i = 0; i < ism; ++i)
      {
         if ((ix[CHAN_VAL][imaxpt] - ix[CHAN_VAL][iminpt])/2.0 != 0)
         {
            // cout << ix[0][(curr_start_idx + isamp-1)-i] << " " << ix[0][imaxpt] << " " <<  ix[0][iminpt] << endl;

            y += (float(ix[CHAN_VAL][(curr_start_idx + isamp-1)-i] /
                 float(ix[CHAN_VAL][imaxpt] - ix[CHAN_VAL][iminpt])/(float)2.0)) * smooth[i];
            // cout << "y: " << setprecision(8) << fixed << y << endl;
         }
      }

    if ((y > up) && (!iflg))
    {
       iflg=1;
         resfile << right << setw(5) << 97 << setw(8) << ix[TIME][(curr_start_idx + isamp-1)-ism/2-40] << endl;
      }
      if ((y < dn) && (iflg))
      {
        iflg = 0;
          resfile << right << setw(5) << 98 << setw(8) << ix[TIME][(curr_start_idx + isamp-1)-ism/2+10] << endl;
      }
      ++curr_start_idx; // just move index, no shifting
      y = 0.0;
   
      if (num_lines - curr_start_idx < isamp) // not enough of array left to process
         done = true;
   }


// done processing the files, so lets merge them
// Did we actually get any results?
   if (!resfile.tellp())
   {
       cout << "No results, exiting. . ." << endl;
       infile.close();
       resfile.close();
       exit(1);
   }

   cout << "Enter output filename, <CR> to exit: ";
   getline(cin,fNamo);
   if (!fNamo.length())
      exit(0);
   mergefile.open(fNamo);
   if (!mergefile.is_open())
   {
      cout << "Could not open merge file, exiting. . . " << fNamo << endl;
      exit(1);
   }
   infile.clear();
   resfile.clear();
   infile.seekg(0,infile.beg);
   resfile.seekp(0,resfile.beg);
   // copy header 
   getline(infile,line);
   i = stoi(line.substr(0,5));
   j = stoi(line.substr(5,string::npos));
  if (i == 0)
  {
     getline(infile,line);
  }
   getline(infile,line);
   i = stoi(line.substr(0,5));
   j = stoi(line.substr(5,string::npos));

   mergefile << right << setw(5) << i << setw(8) << j << endl;
   mergefile << right << setw(5) << i << setw(8) << j << endl;

   getline(infile,line);
   look[INP][CHAN_VAL] = stoi(line.substr(0,5));
   look[INP][TIME] = stoi(line.substr(5,string::npos));
   getline(resfile,line);
   look[MRG][CHAN_VAL] = stoi(line.substr(0,5));
   look[MRG][TIME] = stoi(line.substr(5,string::npos));
    while(!infile.eof() || !resfile.eof())
    {
       if (look[INP][TIME] < look[MRG][TIME])
          iflg = 0;
       else
          iflg = 1;
      mergefile << right << setw(5) << look[iflg][CHAN_VAL] << setw(8) << look[iflg][TIME] << endl;
      if (!iflg && !infile.eof())
      {
         getline(infile,line);
         if (line.length())
         {
            look[INP][CHAN_VAL] = stoi(line.substr(0,5));
            look[INP][TIME] = stoi(line.substr(5,string::npos));
         }
         else
            look[INP][TIME] = numeric_limits<int>::max();
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
}

int main(int /*argc*/, char ** /*argv*/)
{
   add_markers();
   return 0;
}

