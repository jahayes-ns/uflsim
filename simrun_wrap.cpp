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


// C to C++ wrapper functions mostly for the CED son64 classes.

#include <stdio.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <map>
#include <sstream>
#include <ctime>
#include <memory>

#include "simulator.h"
#include "s64.h"
#include "s3264.h"
#include "s32priv.h"
#include "s64priv.h"
#include "simrun_wrap.h"
//#include "launch_model.h"

using namespace ceds64;
using namespace std;

extern int Debug;

enum class SampFreq {FASTER, SLOWER, SAME};
SampFreq currFreq;

// We want time in microseconds so we can use integers, not floats.
// Instatiate this class with that in mind.
class Interval
{
   public:
      Interval(TSTime64 l, TSTime64 s):left(l),step(s) {right = left+step;}
      Interval& operator++() { left = right; right += step; return *this;}
      Interval operator++(int) { Interval tmp(*this);operator++();return tmp;}
      TSTime64 left;
      TSTime64 step;
      TSTime64 right;
   private:
      Interval();
};

// For managing afferent data source
class affData {
   public:
      affData() {sFileIn=nullptr;}
      ~affData(){if (sFileIn) sFileIn->Close();}

      CSon64File *sFileIn;
      SampFreq currFreq;
      int inputChan;
      double timeBase;
      double bpHz;
      double simTicks;      // clock freq
      TSTime64 sampIntv;    // how many ticks per sample
      TSTime64 startTime;
      TSTime64 simStep;
      float    carry;
      unique_ptr<Interval> srcInterval;
      unique_ptr<Interval> simInterval;
};


extern char outFname[];

#ifdef __cplusplus
extern "C" {
#endif

extern char simmsg[];
using lookup = struct { int real; int sib;};
using sibMap = map <int, lookup>;
using sibIter = sibMap::iterator;

static sibMap Sib;
static TSon32File *sFile;      // we write these
static TSon32File *sFileWave;

static bool init_done = false;

void SONStart()
{
   cout << "Init son64 " << endl;
   if (!init_done)
   {
      SONInitFiles();
      init_done = true;
   }
}

void SONStop()
{
   cout << "clean up son64 " << endl;
   if (init_done)
      SONCleanUp();
}

// set up the chans for the smr file
void openSpike()
{
   int num_chans, pop, res;
   TTimeDate td;
   TChanNum chan, ourChan;
   char text[128];
   TSTime64 sampIntv;

   num_chans = S.cwrit_count + S.fwrit_count;
   if (S.save_pop_total == 'y')
      ++num_chans;
   if (num_chans < MINCHANS)  // must be at least this many chans in smr file.
      num_chans = MINCHANS;

   string smrName(outFname);
   const size_t period_idx = smrName.rfind('.');
   if (string::npos != period_idx)
      smrName.erase(period_idx);
   smrName += ".smr";

   sFile = new TSon32File(1);
   res = sFile->Create(smrName.c_str(),num_chans);

   if (res != S64_OK)
   {
      char *msg, *cmd;

      if (res == NO_ACCESS)
         asprintf(&msg,"%s %s %s%s", "\"The file ",smrName.c_str(), " is in use by another program. Close that program and re-run the simulation.","\"");
      else
         asprintf(&msg,"%s %s %s %d%s", "\"Cannot open ", smrName.c_str(), " Error is: ",res,"\"");
      asprintf(&cmd, "%s %s %s", simmsg, "\"SIMRUN ERROR\"", msg);
      cout << msg << endl;
      system (cmd);
      free(msg);
      free(cmd);

      delete sFile;
      sFile = nullptr;
      return;
   }
   double tickSize = S.step/1000.0;
   sFile->SetTimeBase(tickSize);
   time_t curr_time = time(0);
   tm* now = localtime(&curr_time);
   td.ucHun = 0;
   td.ucSec = now->tm_sec;
   td.ucMin = now->tm_min;
   td.ucHour = now->tm_hour;
   td.ucDay = now->tm_mday;
   td.ucMon = now->tm_mon;
   td.wYear = now->tm_year+1900;
   sFile->TimeDate(nullptr,&td);

    // add some comments
   stringstream strm;

   strm << "Simulator run, model file is ";
   strm << S.snd_file_name;
   res = sFile->SetFileComment(0,strm.str().c_str());
   ourChan = 101; // scope starts chans here, we will too
   chan = 0;      // smr files wants them to start a 0
    // create chans
   for (pop = 0; pop < S.cwrit_count; pop++, chan++, ++ourChan)
   {
      res = sFile->SetEventChan(chan,100,ceds64::TDataKind::EventRise,ourChan);
      if (res != S64_OK)
         cout << "event cell chan create res: " << res << " chan" << chan << endl;
      else
      {
         sFile->SetChanUnits(chan,"");
         sprintf(text,"%3d C%3d",ourChan,S.cwrit[pop].pop); // 9 chars or less
         sFile->SetChanTitle(chan,text);
         sFile->SetBuffering(chan,0x4000);
      }
   }
   for (pop = 0; pop < S.fwrit_count; pop++, chan++, ++ourChan)
   {
      res = sFile->SetEventChan(chan,100,ceds64::TDataKind::EventRise,ourChan);
      if (res != S64_OK)
         cout << "event fiber chan create res: " << res << " chan" << chan << endl;
      {
         sFile->SetChanUnits(chan,"");
         sprintf(text,"%3d F%3d",ourChan,S.fwrit[pop].pop); // 9 chars or less
         sFile->SetChanTitle(chan,text);
         sFile->SetBuffering(chan,0x4000);
      }
   }
   sampIntv = 1 / S.step * S.nanlgrate;
   if (S.save_pop_total == 'y')
   {
      res = sFile->SetWaveChan(chan,sampIntv,ceds64::TDataKind::Adc,tickSize,ourChan);
      if (res != S64_OK)
         cout << "wave chan create res: " << res << endl;
      else
      {
         sFile->SetChanUnits(chan,"");
         sprintf(text,"An %2d",ourChan); // 9 chars or less
         sFile->SetChanTitle(chan,text);
         sFile->SetBuffering(chan,0x1000);
      }
   }
}

// set up the waveform chans for smr file
// We use two chans for action potentials, one
// for spike events, one for waveforms
void openSpikeWave()
{
   int num_chans = 0;
   int res;
   TTimeDate td;
   //TChanNum chan;
   int chan;
   char text[128];
   TSTime64 sampIntv;
   int real_chan = 0;
   lookup next;

   for (chan = 0; chan < S.plot_count; ++chan,++num_chans)
      if (S.plot[chan].var == 1) // need 2 chans for action potentials
         ++num_chans;
   if (num_chans < MINCHANS)  // must be at least this many chans in smr file.
      num_chans = MINCHANS;

   string smrName(outFname);
   const size_t period_idx = smrName.rfind('.');
   if (string::npos != period_idx)
      smrName.erase(period_idx);
   smrName += "_wave.smr";
   sFileWave = new TSon32File(1);
   res = sFileWave->Create(smrName.c_str(),num_chans);

   if (res != S64_OK)
   {
      char *msg, *cmd;

      if (res == NO_ACCESS)
         asprintf(&msg,"%s %s %s%s", "\"The file ",smrName.c_str(), " is in use by another program. Close that program and re-run the simulation.","\"");
      else
         asprintf(&msg,"%s %s %s %d%s", "\"Cannot open ", smrName.c_str(), " Error is: ",res,"\"");
      asprintf(&cmd, "%s %s %s", simmsg, "\"SIMRUN ERROR\"", msg);
      cout << msg << endl;
      system (cmd);
      free(msg);
      free(cmd);

      delete sFileWave;
      sFileWave = nullptr;
      return;
   }
   double tickSize = S.step/1000.0;
   sFileWave->SetTimeBase(tickSize);
   time_t curr_time = time(0);
   tm* now = localtime(&curr_time);
   td.ucHun = 0;
   td.ucSec = now->tm_sec;
   td.ucMin = now->tm_min;
   td.ucHour = now->tm_hour;
   td.ucDay = now->tm_mday;
   td.ucMon = now->tm_mon;
   td.wYear = now->tm_year+1900;
   sFileWave->TimeDate(nullptr,&td);

    // add some comments
   stringstream strm;

   strm << "Simulator run, waveform data.";
   res = sFileWave->SetFileComment(0,strm.str().c_str());
   strm.str("");
   strm << "Model file is ";
   strm << S.snd_file_name;
   res = sFileWave->SetFileComment(1,strm.str().c_str());
    // create chans
   sampIntv = 1;
   int plot_type = 0;
   const char *varnames[] = {"", "Vm", "gK", "Thr", "Vm", "h", "Thr"};
   const char *units[] = {"%VC", "%VC/s", "cmH2O", "frac", "frac", "frac", "L", "L", "L/s", "L/s", "cmH2O", "cmH2O", "cmH2O", "0->1", "0->1", "-1->1"};
   for (chan = 0; chan < S.plot_count; ++chan)
   {
       // this code from simviewer_launch_impl.cpp
      char *val = nullptr;
      plot_type = S.plot[chan].var;
      switch (plot_type)
      {
         case -1: // lung model
         case -2:
         case -3:
         case -4:
         case -5:
         case -6:
         case -7:
         case -8:
         case -9:
         case -10:
         case -11:
         case -12:
         case -13:
         case -14:
         case -15:
         case -16:
            if (asprintf (&val, "%.5s", units[abs(plot_type) - 1]) == -1) exit (1);
            break;
         case 0: 
            if (asprintf (&val, "mV") == -1) exit (1);
            break;
         case 1: 
         case 3: 
            if (asprintf (&val, "mV") == -1) exit (1);
            break;
         case 2: 
            if (asprintf (&val, "%.5s", varnames[S.plot[chan].type * 3 + plot_type]) == -1) exit (1);
            break;
         case 4:
            if (asprintf (&val, "%.2f ms",S.plot[chan].val) == -1) exit (1);
            break;
         case STD_FIBER:
         case AFFERENT_EVENT:
            if (asprintf (&val, "Events") == -1) exit (1);
            break;
         case AFFERENT_SIGNAL:
            if (asprintf (&val, "Signal") == -1) exit (1);
            break;
         case AFFERENT_BOTH:
            break;  // not saved to file
         case AFFERENT_INST:
            if (asprintf (&val, "Firing Rate Per Tick") == -1) exit (1);
            break;
         case AFFERENT_BIN:
            if (asprintf (&val, "Average Firing Rate") == -1) exit (1);
            break;
         default:
            if (plot_type < 1)
               break;
            if (asprintf (&val, "%d ms", S.plot[chan].var) == -1) exit (1);
            break;
      }
      sprintf(text,"%.8s", S.plot[chan].lbl); // 9 chars or less
      if (plot_type == 1) // action potentials and waveform
      {
          // use two chans, 1 event 1 waveform
         res = sFileWave->SetEventChan(real_chan,sampIntv,ceds64::TDataKind::EventRise,chan);
         if (res != S64_OK)
            cout << "wave event chan create res: " << res << endl;
         sFileWave->SetChanComment(real_chan,"Spikes");
         sFileWave->SetChanTitle(real_chan,text);
         if (val)
            sFileWave->SetChanUnits(real_chan,val); // units vary
         next.sib = real_chan;
         ++real_chan;
         next.real = real_chan;

         res = sFileWave->SetWaveChan(real_chan,sampIntv,ceds64::TDataKind::RealWave,tickSize,chan);
         if (res != S64_OK)
            cout << "wave chan create res: " << res << endl;
         if (val)
            sFileWave->SetChanUnits(real_chan,val); // units vary
         sFileWave->SetChanTitle(real_chan,text);
         sFileWave->SetChanScale(real_chan,S.step);
         Sib.insert(pair(chan,next));
         ++real_chan;
      }
      else if (plot_type == -1 || plot_type == -2) // JAH: lung mechanics
       {
          res = sFileWave->SetWaveChan(real_chan,sampIntv,ceds64::TDataKind::RealWave,tickSize,chan);
          if (res != S64_OK)
             cout << "wave chan create res: " << res << endl;
          if (val)
             sFileWave->SetChanUnits(real_chan,val); // units vary
          sFileWave->SetChanTitle(real_chan,text);
          sFileWave->SetChanScale(real_chan,S.step);
          next.real = real_chan;
          next.sib = real_chan; // we are our sibling
          Sib.insert(pair(chan,next));
          ++real_chan;
       }
       
      else if (plot_type == STD_FIBER || plot_type == AFFERENT_EVENT) // just events
      {
         res = sFileWave->SetEventChan(real_chan,sampIntv,ceds64::TDataKind::EventRise,chan);
         if (res != S64_OK)
            cout << "wave event chan create res: " << res << endl;
         sFileWave->SetChanComment(real_chan,"Events");
         sFileWave->SetChanTitle(real_chan,text);
         if (val)
            sFileWave->SetChanUnits(real_chan,val); // units vary
         next.real = real_chan;
         next.sib = real_chan; // we are our sibling
         Sib.insert(pair(chan,next));
         ++real_chan;
      }
      else if (plot_type >= 4 || plot_type == AFFERENT_SIGNAL || plot_type == AFFERENT_INST || plot_type == AFFERENT_BIN)
      {
         res = sFileWave->SetWaveChan(real_chan,sampIntv,ceds64::TDataKind::RealWave,tickSize,chan);
         if (res != S64_OK)
            cout << "wave chan create res: " << res << endl;
         if (val)
            sFileWave->SetChanUnits(real_chan,val); // units vary
         sFileWave->SetChanTitle(real_chan,text);
         sFileWave->SetChanScale(real_chan,S.step);
         next.real = real_chan;
         next.sib = -1;
         Sib.insert(pair(chan,next));
         ++real_chan;
      }
   }
   sFileWave->SetBuffering(-1,0x8000);
}


// Close any open smr file.
void closeSpike()
{
   if (sFile)
   {
      sFile->Close();
      delete sFile;
      sFile == nullptr;
   }
   if (sFileWave)
   {
      sFileWave->Close();
      delete sFileWave;
      sFileWave == nullptr;
   }
}


// write bdt/edt type event
void writeSpike(int chan, int time)
{
   int res;
   TChanNum fileChan;
   TSTime64 fileTime = time;

   if (sFile)
   {
      fileChan = chan - 101;
      res = sFile->WriteEvents(fileChan,&fileTime,1);
      if (res != S64_OK)
         cout << "Event chan write error: " << res << endl;
   }
}

// write analog wave data 
void writeWave(int chan, int time)
{
   int res;
   TAdc a_val;
   TChanNum fileChan;
   TSTime64 fileTime = time;

   fileChan = S.cwrit_count + S.fwrit_count;
   a_val = chan % 4096 - (chan % 4096 > 2047) * 4096;
 
   if (sFile)
   {
      res = sFile->WriteWave(fileChan,&a_val,1,fileTime);
      if (res < 0)
        cout << "Wave chan write error1: " << res << endl;
   }
}

// This is analog the waveform data, same stuff
// you see in simviewer. The chan is a plot row number.
// We hide the real chan from the caller, it does not know
// we have a sibling chan
void writeWaveForm(int chan, int time, float val)
{
   sibIter iter = Sib.find(chan);
   int res;
   TChanNum fileChan = iter->second.real;

   //printf("chan: %d\n", chan);
   TSTime64 fileTime = time;
   if (sFileWave)
   {
     /* if (chan == 4) {
         printf("volume: %f\n", val);
      }*/
      res = sFileWave->WriteWave(fileChan,&val,1,fileTime);
      if (res < 0)
        cout << "Wave chan write error2: " << res << endl;
   }
}

// write bdt/edt type event.
// See above for chan info.
void writeWaveSpike(int chan, int time)
{
   int res;
   sibIter iter = Sib.find(chan);
   TChanNum fileChan = iter->second.sib;
   TSTime64 fileTime = time;

   if (sFileWave)
   {
      res = sFileWave->WriteEvents(fileChan,&fileTime,1);
      if (res != S64_OK)
         cout << "Wave Spike Event chan write error: " << res << endl;
   }
}



/* Open the afferent input file and setup vars from it.

  Future notes: 
  How to connnect to external program (EP)?
  simbuild chould start the EP for better UI for failures.

  How do we tell EP our simrun listen port?
  Does EP have to be listening? Then we connect with it? How do
  we know its listen port?

  May be easier for simrun to start and pass its listen port to EP. 
  simbuild could at least make sure program file exists. That way,
  we don't need simbuild at all. Just text in the .sim or script file.
  may also pass params to EP, such as file name or some flags that 
  control behavior.

  Instead of a son file, define an interface:
  For each fiber pop with a signal source:
     Run its EP with port #, like we do with simrun - where simrun is listening 
     All msgs are text  to avoid 0x0d 0x0a stuff, us $ as terminator
      once connected, first block is basic info we need, like in son header, so:
      send rqst for params  "R$"
      get back: sample_rate in samples per second
      when read, send request for as many samples we need for one sim tick.
      Can send ints, be we expect and will convert to float
      E.g: 
      "R$" -->
      "25000$" <--
      "S$" ->-
      "0.125$" <-- 
      if we need more, stay local until we get all of them.
      if we do not need more, keep returning this value until we need the next value.
      Finally, terminate command shuts things down, we don't wait around for a reply
      "T$" -->
      Program can also decide to disconnect
      "D$" <--

   Complications:
   If you want different sources feeding different fiber pops, that is one 
   socket per pop. Could be same program with perhaps optional parameters,
   different sample rates, etc.

*/
void openExternalSource(FiberPop *fiber)
{
   int num_chans, res, vers;
   TChanNum chan;
   char text[128];
   double scale, offset;
   double lo, hi;
   double sim_tick = ceil(1000.0/S.step);
   bool found_one = false;

   if (!strlen(fiber->afferent_file_name)) // no name, call it quits
   {
      fiber->affStruct = nullptr;
      return;
   }
   fiber->affStruct = static_cast<affData*>(new affData());
   affData* aff = static_cast<affData*>(fiber->affStruct);

   aff->sFileIn = new TSon32File;        // older format
   res = aff->sFileIn->Open(fiber->afferent_file_name,1);
   if (res == WRONG_FILE)  // newer format?
   {
      cout << "Not a son32 file (.smr), trying son64 file (.smrx)" << endl;
      delete aff->sFileIn ;
      aff->sFileIn = new TSon64File();
      res = aff->sFileIn->Open(fiber->afferent_file_name,1);
   }
   if (res != S64_OK)
   {
      cout << "Error " << res << " opening " << fiber->afferent_file_name << " Is this a CED format file?" << endl;
      delete aff->sFileIn;
      aff->sFileIn = nullptr;
      delete static_cast<affData*>(fiber->affStruct);
      fiber->affStruct = nullptr;
      return;
   }

   cout << "Reading " <<  fiber->afferent_file_name << endl;
   num_chans = aff->sFileIn->MaxChans();
   aff->timeBase = aff->sFileIn->GetTimeBase();
   vers = aff->sFileIn->GetVersion();
   aff->bpHz = 1.0/aff->timeBase;
   cout << "Time base in seconds: " <<fixed << setprecision(10) << aff->timeBase << endl;
   cout << "Time base: " << fixed << setprecision(4) << round(aff->timeBase*1000*1000) << " usec  " << aff->bpHz << " Hz" << endl;
   cout << "Version: " << vers/256 << "." << vers % 256 << endl;

    // Assume 1st adc chan is our input source, no way right now to select a chan in the GUI
   for (chan = 0 ; chan < num_chans && !found_one; ++chan)
   {
      if (aff->sFileIn->ChanKind(chan) == ceds64::TDataKind::Adc || aff->sFileIn->ChanKind(chan) == ceds64::TDataKind::RealWave)
      {
         found_one = true;
         aff->inputChan = chan;
         cout << "Using channel " << chan  << endl;
         aff->sFileIn->GetChanScale(chan,scale);
         aff->sFileIn->GetChanOffset(chan,offset);
         aff->sFileIn->GetChanUnits(chan,sizeof(text),text);
         aff->sFileIn->GetChanYRange(chan,lo,hi);
         aff->sampIntv = aff->sFileIn->ChanDivide(chan);
         cout << "Scale: " << scale << " Offset: " << offset << endl;
         cout << "Sample interval for chan: " << aff->sampIntv << " timebase ticks"  << endl;
         cout << "Units: " << text << endl;
          // how many samples in a simulator tick?
         aff->simTicks = (aff->bpHz/aff->sampIntv) / sim_tick;
         aff->simStep = S.step * 1000.0; 
         aff->startTime = 0;
         aff->carry = NAN;
         TSTime insec = round((aff->timeBase*1000.0*1000.0) * aff->sampIntv);
         cout << "src time step in usec: " << insec << endl;
         aff->srcInterval = make_unique<Interval>(aff->startTime,insec);
         aff->simInterval = make_unique<Interval>(0,aff->simStep);
         if (Debug){cout << "interval [" << aff->srcInterval->left << " , " 
              << aff->srcInterval->right << ")" << endl;}
         double relate = (aff->bpHz / aff->sampIntv) / 2000;
         if (aff->srcInterval->step > aff->simStep) // slower src frequency
         {  // read 1st outside of later function to avoid constant check for 1st read
            float samp = 0.0;
            TSTime64 read;
            aff->sFileIn->ReadWave(aff->inputChan, &samp, 1, aff->startTime,
                  aff->startTime + aff->srcInterval->step, read);
            aff->carry = samp;
            cout << 1/relate << " simulator ticks per source tick" << endl;
         }
         else
            cout << relate << " src ticks per simulator tick" << endl;
        if (aff->srcInterval->step < aff->simStep)
           aff->currFreq = SampFreq::FASTER;
        else if (aff->srcInterval->step > aff->simStep)
           aff->currFreq = SampFreq::SLOWER;
        else
           aff->currFreq = SampFreq::SAME;

      }
   }
   if (!found_one)
   {
      delete static_cast<affData*>(fiber->affStruct);
      fiber->affStruct = nullptr;
   }
}

/* Read next block of up/downsample and return probability.

   SON expects a start and stop time, more or less a random access method,

   NOTE: We are reading from a file in this implementation. The general case is
   an external program is providing this. So we read from the file one tick at
   a time until we decide we have read one simulator tick. We keep the last
   value since we will always read one sample past the simulators tick.  We'll
   keep that around for the next simulator tick.

   Since we treat time as an interval open on the right, i.e. [tn,tn+1), even
   if the current sample ends exactly on the simulator interval, we carry it
   into the next simulator tick.

*/
bool nextExternalVal(FiberPop *fiber, double* val)
{
   affData* aff = static_cast<affData*>(fiber->affStruct);
   int readvals;
   TSTime64 read;
   float avg = 0.0;
   float samp;
   bool done;
   int have_carry;
   int tick = 1;
   vector <float> samples;

   if (!fiber->affStruct)
   {
      *val = 0.0;
      return false;
   }
   if(Debug){cout << "sim interval [" << aff->simInterval->left <<" , " << aff->simInterval->right << ") usec"<< endl;}
   switch (aff->currFreq)
   {
      case SampFreq::FASTER:
         done = false;
         tick = 1;
         while (!done)
         {
            readvals = aff->sFileIn->ReadWave(aff->inputChan,
                                              &samp, 
                                              1,
                                              aff->startTime,
                                              aff->startTime + aff->srcInterval->step,
                                              read);
            if (Debug){
            cout << "tick: " << tick << " read " << readvals << " samples. " << samp << " time " << read * aff->srcInterval->step;}
            if (readvals == 0) // Do not get an EOF condition, just no data
            {
               *val = 0;
               return false;
            }
            if (read*aff->srcInterval->step  >= aff->simInterval->right)
            {
               if(Debug){cout << " tick: " << tick << " carried to next "<< endl;}
               done = true;
            }
            else
            {
              if(Debug){cout << endl;}
              samples.push_back(samp);
            }
            aff->startTime += aff->sampIntv;
            ++tick;
         }
         have_carry = 0;
         if (!isnan(aff->carry))
         {
            avg = aff->carry;
            have_carry = 1;
         }
         for (auto iter = samples.begin(); iter != samples.end(); ++iter)
            avg += *iter;
         avg /= samples.size() + have_carry;
         if(Debug){cout << "   ****  Avg: " << avg << endl;}
         aff->carry = samp;
         break;

      case SampFreq::SLOWER:
         if(Debug){
         cout << "slower freq  src Interval.right " << aff->srcInterval->right << " current sim " <<  aff->startTime + aff->simInterval->left << endl;}
         if (aff->srcInterval->right <= aff->startTime + aff->simInterval->left) // need new val?
         {
            aff->startTime += aff->sampIntv;
            ++(*aff->srcInterval);
            readvals = aff->sFileIn->ReadWave(aff->inputChan,
                                              &samp, 
                                              1,
                                              aff->startTime,
                                              aff->startTime + aff->sampIntv,
                                              read);
            aff->carry = samp;
            if(Debug){
            cout << " slower freq: read " << readvals << " samples. " << samp << " time " << read;}
         }
         avg = aff->carry;
         if(Debug){cout << "   ****  Avg: " << avg << endl;}
         break;

      case SampFreq::SAME:
      default:
         readvals = aff->sFileIn->ReadWave(aff->inputChan,
                                              &avg,
                                              1,
                                              aff->startTime,
                                              aff->startTime + aff->sampIntv,
                                              read);
         if(Debug){cout << " same freq: read " << readvals << " samples. " << samp << " time " << read;}
         if (readvals == 0)
         {
            *val = 0;
            return false;
         }
         aff->startTime++;
         break;
   }
   ++(*aff->simInterval); // next sim tick

   *val = avg;
   return true;
}

#ifdef __cplusplus
}
#endif

