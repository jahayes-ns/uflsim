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

#include <iostream>
#include <QMessageBox>
#include <QString>
#include <QTextStream>
#include <QFile>
#include <QDir>
#include <QDateTime>

#ifdef __cplusplus
extern "C" {
#endif
#include "inode.h"
#include "fileio.h"
#include "simulator.h"
#include "simulator_hash.h"
#include "swap.h"
#include "fileio.h"
#include "inode_hash.h"
#include "inode_2.h"
#include "c_globals.h"
#ifdef __cplusplus
}
#endif

#include "chglog.h"
#include "lin2ms.h"

using namespace std;

ChgLog::ChgLog(QString name) : logName(name)
{
   Dref = &D;
   OnDisk = make_unique<inode_global>();
   logstrm.setString(&logSoFar,QIODevice::WriteOnly);
}

ChgLog::~ChgLog()
{
}


static void popToStr(int type, QString& str)
{

  switch (type)
  {
     case FIBER:
        str="Normal Fiber";
        break;
     case ELECTRIC_STIM:
        str="Electric stimumulation";
        break;
     case AFFERENT:
        str="Afferent";
        break;
     case CELL:
        str="Standard Cell";
        break;
     case BURSTER_POP:
        str="Burster";
        break;
     case PSR_POP:
        str="PSR";
        break;
     case PHRENIC_POP:
        str="Phrenic";
        break;
     case LUMBAR_POP:
        str="Lumbar";
        break;
     case INSP_LAR_POP:
        str="Inspiratory Laryngeal";
        break;
     case EXP_LAR_POP:
        str="Expiritory Laryngeal";
        break;
     default:
        str="Subtype";
        break;
   }
}

static void freqToStr(int type, QString& str)
{
   switch (type)
   {
      case STIM_FIXED:
         str = "Deterministic frequency";
         break;
      case STIM_FUZZY:
         str = "Fuzzy Frequency";
         break;
        default:
           str="Frequency";
           break;
   }
}


bool ChgLog::newChangeLog(const QString& fname)
{
   QFile log(logName);
   if (!log.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
   {
      QString why = log.errorString();
      int why2 = log.error();
      cout << why.toLatin1().data() << " " << why2 << endl;
      QMessageBox msgBox;
      msgBox.setIcon(QMessageBox::Warning);
      msgBox.setWindowTitle("COULD NOT CREATE LOG FILE");
      msgBox.setStandardButtons(QMessageBox::Ok);
      msgBox.setText("Could not create " + logName + ". Are there permission problems?");
      msgBox.exec();
      return false;
   }
   QTextStream str(&log);
    // header
   QDateTime dt = QDateTime::currentDateTime();
   QString dt_str = dt.toString("ddd MMMM dd yyyy hh:mm:ss ap");
   str << endl << "***************************************************"  << endl
           << dt_str << endl;
   str << "Change log file created." << endl;
   str << "File: " << fname << endl;
   log.close();
   return true;
}

// This is called from a menu choice or as part of the save-file control flow.
bool ChgLog::updateChangeLog(const QString& fname)
{
   if (decide(fname))
      chkForChanges(fname);
   return true;
}

bool ChgLog::decide(const QString& fname)
{
   FILE *fd;
   const int sz = 4;
   char buf[sz];
   int ver = 0;
   QString msg;
   QTextStream strm(&msg);
   QMessageBox msgBox;

   if ((fd = fopen (fname.toLatin1().data(), "rb")) == 0 || fread(buf,1,sz,fd) != sz)
   {
      strm << "Could not open " << fname << ". If this is a new file, there are no changes to log.";
      msgBox.setIcon(QMessageBox::Warning);
      msgBox.setWindowTitle("Could Not Open File");
      msgBox.setStandardButtons(QMessageBox::Ok);
      msgBox.setText(msg);
      msgBox.exec();
      return false;
   }

   if (strncmp (buf, "file", 4) == 0) // V5 or later has a header as first line
   {
       char *line = nullptr;
       size_t len=0;
       ssize_t res = getline(&line,&len,fd);
       fclose (fd);
       if (res && line)
       {
          int gotit = sscanf(line,"%*s %d\n",&ver);
          free(line);
          if (!gotit)
          {
             fprintf(stderr,"Warning: the .snd file does have have a valid header.\n");
             return false;
          }
       }
   }
   if (ver < FILEIO_FORMAT_VERSION6)
   {
      QMessageBox msgBox;
      msgBox.setIcon(QMessageBox::Warning);
      msgBox.setWindowTitle("Unsupported Format");
      msgBox.setStandardButtons(QMessageBox::Ok);
      msgBox.setText("The file on the disk is an earlier version.\nChange Logs are only supported for newer versions.\nSave the current in-memory information to a file before updating the change log.");
      return false;
   }
   return true;
}

bool ChgLog::chkForChanges(const QString& fname)
{
   FILE *fd;
   QString from, to;

   ::struct_info_fn = inode_struct_info;
   ::struct_members_fn = inode_struct_members;
   if ((fd = load_struct_open_simbuild(fname.toLatin1().data())) != 0)
   {
      load_struct (fd, (char*)"inode_global", static_cast<void*>(OnDisk.get()), 1);
      fclose (fd);
   }
   QFile log(logName);
   if (!log.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
   {
      QString why = log.errorString();
      int why2 = log.error();
      cout << why.toLatin1().data() << " " << why2 << endl;
      QMessageBox msgBox;
      msgBox.setIcon(QMessageBox::Warning);
      msgBox.setWindowTitle("COULD NOT OPEN FILE");
      msgBox.setStandardButtons(QMessageBox::Ok);
      msgBox.setText("Could not open " + logName + ". Are there permission problems?");
      msgBox.exec();
      return false;
   }

   compareGlobals();
   compareSynapses();
   compareNodes();

   QTextStream str(&log);
    // header
   QDateTime dt = QDateTime::currentDateTime();
   QString dt_str = dt.toString("ddd MMMM dd yyyy hh:mm:ss ap");
   str << endl << "***************************************************"  << endl
           << dt_str << endl;
   str << "File: " << fname << endl;

   if (logSoFar.length() == 0)  // no changes, say that
   {
      str << "No changes detected." << endl;
      str << "Note: Changes to Launcher window are not currently checked for." << endl;
   }
   else
   {
      str << "Current Launch Number: " << currModel << endl;
      str << logSoFar; 
   }
   log.close();
   return true;
}

void ChgLog::compareGlobals()
{
   const GLOBAL_NETWORK& new_g = Dref->inode[GLOBAL_INODE].unode.global_node;
   const GLOBAL_NETWORK& old_g = OnDisk->inode[GLOBAL_INODE].unode.global_node;
   QString old_comment = old_g.global_comment;
   QString new_comment = new_g.global_comment;
   if (old_comment != new_comment)
      logstrm << "Global comment changed from" << endl << "\"" << old_comment << "\"" <<endl << "to" <<endl << "\"" << new_comment << "\"" << endl;
   if (new_g.sim_length_seconds - old_g.sim_length_seconds > .0000001)
      logstrm << "Simulation length changed from " << old_g.sim_length_seconds << " seconds to " << new_g.sim_length_seconds << " seconds." << endl;
   if (new_g.k_equilibrium != old_g.k_equilibrium)
      logstrm << "K equilibrium value changed from "
              << old_g.k_equilibrium << " to " << new_g.k_equilibrium << "." << endl;
   if (new_g.step_size != old_g.step_size)
      logstrm << "Step tick time value changed from " << old_g.step_size/1000  
              << " to " << new_g.step_size/1000 << "." << endl;
   if (new_g.ilm_elm_fr != old_g.ilm_elm_fr)
      logstrm << "ILM/ELM firing rate value changed from " << old_g.ilm_elm_fr 
              << " to " << new_g.ilm_elm_fr << "." << endl;
   if (strcmp(new_g.phrenic_equation,old_g.phrenic_equation) != 0)
      logstrm << "Phrenic equation changed from " << old_g.phrenic_equation << 
              " to " << new_g.phrenic_equation << "." << endl;
   if (strcmp(new_g.lumbar_equation,old_g.lumbar_equation) != 0)
      logstrm << "Lumbar equation changed from " << old_g.lumbar_equation  <<
              "to  " << new_g.lumbar_equation << "." << endl;
}


// cell/fiber pops
void ChgLog::compareNodes()
{
   int old_node_type, new_node_type;
   QString from, to;

  for (int slot = FIRST_INODE ; slot <= LAST_INODE; ++slot)
  {
     const I_NODE& i_new = Dref->inode[slot];
     const I_NODE& i_old = OnDisk->inode[slot];

     new_node_type = i_new.node_type;
     old_node_type = i_old.node_type;

     if (old_node_type == UNUSED && new_node_type == UNUSED)
        continue;
     else if (old_node_type == UNUSED && new_node_type != UNUSED)
     {
        logstrm << "Added " ;
        if (new_node_type == CELL)
           logstrm << "Cell";
        else
           logstrm << "Fiber";
        logstrm << " Population " << i_new.node_number 
             << ", Comment is: " << i_new.comment1 << "." << endl;
     }
     else if (old_node_type != UNUSED && new_node_type == UNUSED)
     {
        logstrm << "Deleted ";
        if (old_node_type == CELL)
           logstrm << " Cell";
        else
           logstrm << " Fiber";
        logstrm << " Population " << i_new.node_number << "." << endl;
     }
     else if (old_node_type != new_node_type)
     {
        popToStr(old_node_type,from);
        popToStr(new_node_type,to);
        logstrm << "Changed " << from << " to " << to << "." << endl;
     }
     else if (old_node_type == CELL && new_node_type == CELL)
         compareCells(i_old,i_new);
     else if (old_node_type == FIBER && new_node_type == FIBER)
         compareFibers(i_old,i_new);
     else
        logstrm << "missed case" << endl;
   }
}

void ChgLog::compareCells(const I_NODE& old_i, const I_NODE& new_i)
{
   const C_NODE &old_c = old_i.unode.cell_node;
   const C_NODE &new_c = new_i.unode.cell_node;
   int old_pop = old_i.node_number;
   int old_sub = old_c.pop_subtype;
   int new_sub = new_c.pop_subtype;
   int new_pop = new_i.node_number;
   QString from, to, popstr;
   QString old_txt, new_txt;
   int node;

   if (old_pop != new_pop) // adding and deleting can result in this
   {
      popstr = QString("Changed cell population %1 to cell population %2").arg(old_pop).arg(new_pop);
      logstrm << popstr << "." << endl;
      return;
   }

   popstr = QString("Changed cell population %1").arg(old_pop);
    // Earlier v6 files set the pop_subtype to UNUSED if it was the standard
    // cell. It simplfies things elsewhere if we set the subtype to the node
    // type.
   if (old_sub == UNUSED)
      old_sub = CELL;
   if (new_sub == UNUSED)
      new_sub = CELL;
   if (old_sub != new_sub) // if changed type, can't really compare params
   {
      popToStr(old_sub,from);
      popToStr(new_sub,to);
      logstrm << popstr << " from " << from
              << " to " << to << "." << endl;
   }
   else // Chk for param changes.  Some vars are overloaded and the value
   {    // of a var represents different things for different kinds of cells.
      if (old_sub == BURSTER_POP)
      {
         if (old_c.c_accomodation != new_c.c_accomodation)
            logstrm << popstr << " time constant for h from " << old_c.c_accomodation 
                   << " to " << new_c.c_accomodation  << "." << endl;
         if (old_c.c_thresh_remove_ika != new_c.c_thresh_remove_ika)
            logstrm << popstr << " slope for h from " << old_c.c_thresh_remove_ika 
                   << " to " << new_c.c_thresh_remove_ika  << "." << endl;
         if (old_c.c_rebound_time_k != new_c.c_rebound_time_k)
            logstrm << popstr << " half voltage for activation from " << old_c.c_rebound_time_k 
                   << " to " << new_c.c_rebound_time_k  << "." << endl;
         if (old_c.c_max_conductance_ika != new_c.c_max_conductance_ika)
            logstrm << popstr << " slope for activation from " << old_c.c_max_conductance_ika 
                   << " to " << new_c.c_max_conductance_ika  << "." << endl;
         if (old_c.c_pro_deactivate_ika != new_c.c_pro_deactivate_ika)
            logstrm << popstr << " reset voltage @h=0 from " << old_c.c_pro_deactivate_ika 
                   << " to " << new_c.c_pro_deactivate_ika  << "." << endl;
         if (old_c.c_pro_activate_ika != new_c.c_pro_activate_ika)
            logstrm << popstr << " threshold voltage from " << old_c.c_pro_activate_ika 
                   << " to " << new_c.c_pro_activate_ika  << "." << endl;
         if (old_c.c_rebound_time_k != new_c.c_rebound_time_k)
            logstrm << popstr << " half voltage for h from " << old_c.c_rebound_time_k 
                   << " to " << new_c.c_rebound_time_k  << "." << endl;
         if (old_c.c_ap_k_delta != new_c.c_ap_k_delta)
            logstrm << popstr << " NaP conductance from " << old_c.c_ap_k_delta 
                   << " to " << new_c.c_ap_k_delta  << "." << endl;
         if (old_c.c_dc_injected != new_c.c_dc_injected)
            logstrm << popstr << " applied current (Iapp) from " << old_c.c_dc_injected 
                   << " to " << new_c.c_dc_injected  << "." << endl;
      }
      else if (old_sub == PSR_POP)
      {
         if (old_c.c_accomodation != new_c.c_accomodation)
            logstrm << popstr << " rise time from " << old_c.c_accomodation 
                   << " to " << new_c.c_accomodation  << "." << endl;
         if (old_c.c_k_conductance != new_c.c_k_conductance)
            logstrm << popstr << " fall time from " << old_c.c_k_conductance 
                   << " to " << new_c.c_k_conductance  << "." << endl;
         if (old_c.c_resting_thresh != new_c.c_resting_thresh)
            logstrm << popstr << " output threshold from " << old_c.c_resting_thresh 
                   << " to " << new_c.c_resting_thresh  << "." << endl;
         if (old_c.c_resting_thresh_sd != new_c.c_resting_thresh_sd)
            logstrm << popstr << " SD of threshold if NZ from " << old_c.c_resting_thresh_sd 
                   << " to " << new_c.c_resting_thresh_sd  << "." << endl;
         if (old_c.c_pop != new_c.c_pop)
            logstrm << popstr << " population size from " << old_c.c_pop 
                   << " to " << new_c.c_pop  << "." << endl;
      }
      else
      {
         if (old_c.c_accomodation != new_c.c_accomodation)
            logstrm << popstr << " accommodation time from " << old_c.c_accomodation 
                   << " to " << new_c.c_accomodation  << "." << endl;
         if (old_c.c_k_conductance != new_c.c_k_conductance)
            logstrm << popstr << " potassium conductance time from " << old_c.c_k_conductance 
                   << " to " << new_c.c_k_conductance  << "." << endl;
         if (old_c.c_mem_potential != new_c.c_mem_potential)
            logstrm << popstr << " membrane time constant from " << old_c.c_mem_potential 
                   << " to " << new_c.c_mem_potential  << "." << endl;
         if (old_c.c_ap_k_delta != new_c.c_ap_k_delta)
            logstrm << popstr << " K conductance change with AP from " << old_c.c_ap_k_delta 
                   << " to " << new_c.c_ap_k_delta  << "." << endl;
         if (old_c.c_pop != new_c.c_pop)
            logstrm << popstr << " population size from " << old_c.c_pop 
                   << " to " << new_c.c_pop  << "." << endl;
         if (old_c.c_accom_param != new_c.c_accom_param)
            logstrm << popstr << " accommodation parameter from " << old_c.c_accom_param 
                   << " to " << new_c.c_accom_param  << "." << endl;
         if (old_c.c_dc_injected != new_c.c_dc_injected)
            logstrm << popstr << " DC injected current " << old_c.c_dc_injected 
                   << " to " << new_c.c_dc_injected  << "." << endl;
         if (old_c.c_resting_thresh != new_c.c_resting_thresh)
            logstrm << popstr << " resting threshold from " << old_c.c_resting_thresh 
                   << " to " << new_c.c_resting_thresh  << "." << endl;
         if (old_c.c_resting_thresh_sd != new_c.c_resting_thresh_sd)
            logstrm << popstr << " resting threshold SD from " << old_c.c_resting_thresh_sd 
                   << " to " << new_c.c_resting_thresh_sd  << "." << endl;
         if (old_c.c_rebound_param != new_c.c_rebound_param)
            logstrm << popstr << " noise amplitude from " << old_c.c_rebound_param 
                   << " to " << new_c.c_rebound_param  << "." << endl;
         if ((old_c.c_injected_expression == nullptr && new_c.c_injected_expression != nullptr)
            || (old_c.c_injected_expression != nullptr && new_c.c_injected_expression == nullptr) 
            || (old_c.c_injected_expression != nullptr && new_c.c_injected_expression != nullptr && strcmp(old_c.c_injected_expression,new_c.c_injected_expression) != 0))
            logstrm << popstr << " injected expression from " << old_c.c_injected_expression << " to " << new_c.c_injected_expression  << "." << endl;
      }
      for (node = 0; node < TABLE_LEN; ++node)
      {
         if (old_c.c_target_nums[node] > 0 && new_c.c_target_nums[node] == 0)
            logstrm << popstr << " to cell target " << old_c.c_target_nums[node] << " deleted";
         else if (old_c.c_target_nums[node] == 0 && new_c.c_target_nums[node] > 0)
            logstrm << popstr << " to cell target " << new_c.c_target_nums[node] << " added" << "." << endl;
         else if (old_c.c_target_nums[node] != new_c.c_target_nums[node])
            logstrm << popstr << " cell target " << new_c.c_target_nums[node] << " type changed from cell population " << old_c.c_target_nums[node] << " to cell population " << new_c.c_target_nums[node] << "." << endl;
         else 
         {
            if (old_c.c_synapse_type[node] != new_c.c_synapse_type[node])
            {
               int oldNum = old_c.c_synapse_type[node];
               int newNum = new_c.c_synapse_type[node];
               QString oldName = Dref->inode[SYNAPSE_INODE].unode.synapse_node.synapse_name[oldNum];
               QString newName = OnDisk->inode[SYNAPSE_INODE].unode.synapse_node.synapse_name[newNum];
               logstrm << popstr << " cell synapse type changed from #" 
               << oldNum << " " << oldName << " to #" << newNum << " " 
               << newName << endl;
               continue; // params may have changed, but comparing to old ones irrelevant
            }

            if (old_c.c_min_conduct_time[node] != new_c.c_min_conduct_time[node])
               logstrm << popstr << " cell target population " << new_c.c_target_nums[node]
               << " minumum conduction time changed from " << old_c.c_min_conduct_time[node]
               << " to " << new_c.c_min_conduct_time[node] << "." << endl;
            if (old_c.c_conduction_time[node] != new_c.c_conduction_time[node])
               logstrm << popstr << " cell target population " << new_c.c_target_nums[node]
               << " maximum conduction time changed from " << old_c.c_conduction_time[node]
               << " to " << new_c.c_conduction_time[node] << "." << endl;
            if (old_c.c_terminals[node] != new_c.c_terminals[node])
               logstrm << popstr << " cell target population " << new_c.c_target_nums[node]
               << " number of terminals changed from " << old_c.c_terminals[node]
               << " to " << new_c.c_terminals[node] << "." << endl;
            if (old_c.c_synapse_strength[node] != new_c.c_synapse_strength[node])
               logstrm << popstr << " cell target population " << new_c.c_synapse_strength[node]
               << " synapse strength changed from " << old_c.c_synapse_strength[node]
               << " to " << new_c.c_synapse_strength[node] << "." << endl;
            if (old_c.c_target_seed[node] != new_c.c_target_seed[node])
               logstrm << popstr << " cell target population " << new_c.c_target_seed[node]
               << " random number seed changed from " << old_c.c_target_seed[node]
               << " to " << new_c.c_target_seed[node] << "." << endl;
         }
      }
      old_txt = old_i.comment1;
      new_txt = new_i.comment1;
      if (old_txt != new_txt)
         logstrm << popstr << " comment from" << endl << "\"" << old_txt << "\"" 
                 << endl << "to" <<endl << "\"" << new_txt << "\"" << endl;
      old_txt = old_c.group;
      new_txt = new_c.group;
      if (old_txt.length() == 0)
         old_txt = "Not in group";
      if (new_txt.length() == 0)
         new_txt = "Not in group";
      if (old_txt != new_txt)
         logstrm << popstr << " group from \"" << old_txt
                 << "\" to \"" << new_txt  << "\"." << endl;
   }
}


void ChgLog::compareFibers(const I_NODE& old_i, const I_NODE& new_i)
{
   const F_NODE& old_f = old_i.unode.fiber_node;
   const F_NODE& new_f = new_i.unode.fiber_node;
   QString from, to, popstr;
   int node;
   int oldNum, newNum;
   QString oldName, newName;
   QString old_txt, new_txt;
   QString old_freq, new_freq;
   int old_sub = old_f.pop_subtype;
   int new_sub = new_f.pop_subtype;
   int old_pop = old_i.node_number;
   int new_pop = new_i.node_number;

   if (old_pop != new_pop) // adding and deleting can result in this
   {
      popstr = QString("Changed fiber population %1 to fiber population %2").arg(old_pop).arg(new_pop);
      logstrm << popstr << "." << endl;
      return;
   }

   popstr = QString("Changed fiber population %1").arg(old_pop);
    // Earlier v6 files set the pop_subtype to UNUSED if it was the standard
    // cell. It simplfies things elsewhere if we set the subtype to the node
    // type.
   if (old_sub == UNUSED)
      old_sub = FIBER;
   if (new_sub == UNUSED)
      new_sub = FIBER;
   if (old_sub != new_sub) // if changed type, can't really compare params
   {
      popToStr(old_sub,from);
      popToStr(new_sub,to);
      logstrm << popstr << " from " << from
              << " to " << to << "." << endl;
   }
   else // Chk for param changes.  Some vars are overloaded and the value
   {
      if (old_sub == FIBER) // type-specific params
      {
         if (old_f.f_prob != new_f.f_prob)
            logstrm << popstr << " probability of firing from " << old_f.f_prob 
                    << " to " << new_f.f_prob  << "." << endl;
         if (old_f.f_pop != new_f.f_pop)
            logstrm << popstr << " number of fibers from " << old_f.f_pop 
                    << " to " << new_f.f_pop  << "." << endl;
      }
      else if (old_sub == ELECTRIC_STIM)
      {
         if (old_f.freq_type != new_f.freq_type)
         {
            freqToStr(old_f.freq_type, old_freq);
            freqToStr(new_f.freq_type, new_freq);
            logstrm << popstr << " frequency type from " << old_freq
                    << " to " << new_freq << "." << endl;
         }
         if (old_f.frequency != new_f.frequency)
            logstrm << popstr << " firing frequnecy from " << old_f.frequency 
                    << " to " << new_f.frequency  << "." << endl;
         if (old_f.fuzzy_range != new_f.fuzzy_range)
            logstrm << popstr << " fuzzy firing range from " << old_f.fuzzy_range 
                    << " to " << new_f.fuzzy_range  << "." << endl;
      }
      else if (old_sub == AFFERENT)
      {
         if (strcmp(old_f.afferent_file,new_f.afferent_file) != 0)
            logstrm << "afferent file changed from:" << endl << old_f.afferent_file << endl
                    << "to:" << endl << new_f.afferent_file  << "." << endl;
         if (strcmp(old_f.afferent_prog,new_f.afferent_prog) != 0)
            logstrm << "afferent program changed from:" << endl << old_f.afferent_prog
                    << "to:" << endl << new_f.afferent_prog  << "." << endl;
         if (old_f.offset != new_f.offset)
            logstrm << "afferent signal offset changed from:" << old_f.offset
                    << " to: " << new_f.offset << "." << endl;
         if (old_f.slope_scale != new_f.slope_scale)
            logstrm << "Afferent slope probability scaling factor changed from:" << old_f.slope_scale
                    << " to: " << new_f.slope_scale << "." << endl;
         int count = min(old_f.num_aff, new_f.num_aff);
         for (int idx = 0; idx < count; ++idx)
         {
            if (old_f.aff_val[idx] != new_f.aff_val[idx] ||   // any change, print 
                old_f.aff_prob[idx] != new_f.aff_prob[idx] ||  // both tables
                old_f.num_aff != new_f.num_aff)
            {
               logstrm << "afferent probability table changed from:" << endl;
               for (int idx = 0; idx < old_f.num_aff; ++idx)
                  logstrm << "(" << old_f.aff_val[idx] << " , " << old_f.aff_prob[idx] << ") ";
                logstrm << endl;
               logstrm << "to:" << endl;
               for (int idx = 0; idx < new_f.num_aff; ++idx)
                  logstrm << "(" << new_f.aff_val[idx] << " , " << new_f.aff_prob[idx] << ") ";
                logstrm << endl;
                break;
            }
         }
      }
         // common params
      if (old_f.f_begin != new_f.f_begin)
         logstrm << popstr << " time to begin firing from " << old_f.f_begin 
                 << " to " << new_f.f_begin  << "." << endl;
      if (old_f.f_end != new_f.f_end)
      {
         QString oldstr, newstr; 
         if (old_f.f_end == -1)
            oldstr = "Never";
         else
            oldstr = QString::number(old_f.f_end);
         if (new_f.f_end == -1)
            newstr = "Never";
         else
            newstr = QString::number(new_f.f_end);
         logstrm << popstr << " time to end firing from " << oldstr
                 << " to " << newstr  << "." << endl;
      }
      if (old_f.f_seed != new_f.f_seed)
         logstrm << popstr << " random number seed from " << old_f.f_seed 
                 << " to " << new_f.f_seed  << "." << endl;

      for (node = 0; node < TABLE_LEN; ++node)
      {
         if (old_f.f_target_nums[node] > 0 && new_f.f_target_nums[node] == 0)
            logstrm << popstr << " fiber target to cell population " << old_f.f_target_nums[node] << " deleted" << "." << endl;
         else if (old_f.f_target_nums[node] == 0 && new_f.f_target_nums[node] > 0)
            logstrm << popstr << " fiber target to cell population " << new_f.f_target_nums[node] << " added" << "." << endl;
         else if (old_f.f_target_nums[node] != new_f.f_target_nums[node])
            logstrm << popstr << " fiber target to cell population " << new_f.f_target_nums[node] << " changed from " << old_f.f_target_nums[node] << " to " << new_f.f_target_nums[node] << "." << endl;
         else 
         {
            if (old_f.f_synapse_type[node] != new_f.f_synapse_type[node])
            {
               oldNum = old_f.f_synapse_type[node];
               newNum = new_f.f_synapse_type[node];
               oldName = Dref->inode[SYNAPSE_INODE].unode.synapse_node.synapse_name[oldNum];
               newName = OnDisk->inode[SYNAPSE_INODE].unode.synapse_node.synapse_name[newNum];
               logstrm << popstr << " fiber synapse type changed from #" 
               << oldNum << " " << oldName << " to #" << newNum << " " 
               << newName << endl;
               continue; // params may have changed, but comparing to old ones irrelevant
            }

            if (old_f.f_min_conduct_time[node] != new_f.f_min_conduct_time[node])
               logstrm << popstr << " fiber to target to cell population " << new_f.f_target_nums[node]
               << " minumum conduction time changed from " << old_f.f_min_conduct_time[node]
               << " to " << new_f.f_min_conduct_time[node] << "." << endl;
            if (old_f.f_conduction_time[node] != new_f.f_conduction_time[node])
               logstrm << popstr << " fiber target to cell population " << new_f.f_target_nums[node]
               << " maximum conduction time changed from " << old_f.f_conduction_time[node]
               << " to " << new_f.f_conduction_time[node] << "." << endl;
            if (old_f.f_terminals[node] != new_f.f_terminals[node])
               logstrm << popstr << " fiber target to cell population " << new_f.f_target_nums[node]
               << " number of terminals changed from " << old_f.f_terminals[node]
               << " to " << new_f.f_terminals[node] << "." << endl;
            if (old_f.f_synapse_strength[node] != new_f.f_synapse_strength[node])
               logstrm << popstr << " fiber target to cell population " << new_f.f_synapse_strength[node]
               << " synapse strength changed from " << old_f.f_synapse_strength[node]
               << " to " << new_f.f_synapse_strength[node] << "." << endl;
            if (old_f.f_target_seed[node] != new_f.f_target_seed[node])
               logstrm << popstr << " fiber target to cell population " << new_f.f_target_seed[node]
               << " random number seed changed from " << old_f.f_target_seed[node]
               << " to " << new_f.f_target_seed[node] << "." << endl;
         }
      }
      old_txt = old_i.comment1;
      new_txt = new_i.comment1;
      if (old_txt != new_txt)
         logstrm << popstr << " comment from" << endl << "\"" << old_txt << "\"" 
                 << endl << "to" <<endl << "\"" << new_txt << "\"" << endl;
      old_txt = old_f.group;
      new_txt = new_f.group;
      if (old_txt.length() == 0)
         old_txt = "Not in group";
      if (new_txt.length() == 0)
         new_txt = "Not in group";
      if (old_txt != new_txt)
         logstrm << popstr << " group from \"" << old_txt
                      << "\" to \"" << new_txt  << "\"." << endl;
   }
}

void ChgLog::compareSynapses()
{
   const S_NODE& new_s = Dref->inode[SYNAPSE_INODE].unode.synapse_node;
   const S_NODE& old_s = OnDisk->inode[SYNAPSE_INODE].unode.synapse_node;
   int node, old_type, new_type;
   QString old_name, new_name, strmstr;
   QStringList types={{ "Not used","Normal","Presynaptic","Postsynaptic"}};
   for (node = 1; node < TABLE_LEN; ++node) // index 0 unused for synapse node
   {
      old_name = old_s.synapse_name[node];
      new_name = new_s.synapse_name[node];
      strmstr = QString("Synapse number ") + QString::number(node) + " " + new_name + " ";  
      old_type = old_s.syn_type[node];
      new_type = new_s.syn_type[node];
      if (old_name != new_name)
         logstrm << QString("Synapse number ") + QString::number(node) + " change name from " + old_name + " to " << new_name << "." << endl; 
      if (old_type == SYN_NOT_USED and new_type != SYN_NOT_USED)
         logstrm << strmstr << " type " << types[new_type] << " added" << "." << endl;
      else if (old_type != SYN_NOT_USED and new_type == SYN_NOT_USED)
         logstrm << strmstr << " deleted" << "." << endl;
      else if (old_type != new_type)
         logstrm << strmstr << " changed from " << types[old_type] 
         << " to " << " type " << types[new_type] << "." << endl;
      else
      {
         if (old_s.s_eq_potential[node] != new_s.s_eq_potential[node])
            logstrm << strmstr << " equilibrium potential changed from " << 
            old_s.s_eq_potential[node] << " to " << new_s.s_eq_potential[node] << "." << endl;
         if (old_s.s_time_constant[node] != new_s.s_time_constant[node])
            logstrm << strmstr << " time constant changed from " << 
            old_s.s_time_constant[node] << " to " << new_s.s_time_constant[node] << "." << endl;
      }
   }
}

