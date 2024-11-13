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
#include <unistd.h>
#include <iostream>
#if defined WIN32
#include <libloaderapi.h>
#endif
#include "simwin.h"
#include "ui_simwin.h"
#include "QScrollArea"
#include "QScrollBar"

using namespace std;


SimWin::SimWin(QWidget *parent) : QMainWindow(parent), ui(new Ui::SimWin)
{
   ui->setupUi(this);

   QString title("SimBuild Version: ");
   title = title.append(VERSION);
   setWindowTitle(title);
   simInit();
}

SimWin::~SimWin()
{
   delete scene;
   delete ui;
}

QGraphicsView* SimWin::getView()
{
   return ui->simView;
}

void SimWin::on_actionQuit_2_triggered()
{
   close();
}

void SimWin::on_actionAdd_Fiber_triggered()
{
   addFiber();
}

void SimWin::on_actionAddCell_triggered()
{
   addCell();
}

void SimWin::on_actionAddConnector_triggered()
{
    addConnector();
}

void SimWin::on_actionDeleteSelected_triggered()
{
   deleteSelected();
}


void SimWin::on_cellShowTarg_clicked()
{
   doCellRetro();
}

void SimWin::on_cellShowSrc_clicked()
{
   doCellAntero();
}

void SimWin::on_fiberShowTarg_clicked()
{
   doFiberRetro();
}


void SimWin::on_actionQuit_triggered()
{
   close();
}


void SimWin::on_actionLoad_snd_File_triggered()
{
   loadSnd();
}

void SimWin::on_standardCell_toggled(bool checked)
{
   doStdCell(checked);
}

void SimWin::on_bursterCell_toggled(bool checked)
{
   doBurster(checked);
}

void SimWin::on_psrCell_toggled(bool checked)
{
   doPsr(checked);
}

void SimWin::on_actionBuild_Synapse_Globals_triggered()
{
   showBuildSynapse();
}
void SimWin::on_actionOne_Shot_Add_triggered(bool checked)
{
   doOneShotAdd(checked);
}

void SimWin::on_actionNew_triggered()
{
   doNewFile();
}

void SimWin::on_actionRun_Simulation_triggered()
{
   launchPopup();
}

void SimWin::on_cellSendLaunchBdt_clicked()
{
   cellSendToLauncherBdt();
}

void SimWin::on_cellSendLaunchPlot_clicked()
{
   cellSendToLauncherPlot();
}

void SimWin::on_fiberSendLaunchBdt_clicked()
{
   fiberSendToLauncherBdt();
}

void SimWin::on_selAll_clicked()
{
   doSelAll();
}

void SimWin::on_fitToView_clicked()
{
   fitToView();
}

void SimWin::on_actionSave_snd_File_triggered()
{
   saveSnd();
}

void SimWin::on_actionSave_sim_File_triggered()
{
   saveSim();
}

void SimWin::on_actionShell_triggered()
{
   if (!system("gnome-terminal"))
      {;}
   else if (!system("xterm"))
      {;}
}

void SimWin::on_lineLayer_clicked()
{
  connLayerToggle();

}

void SimWin::on_actionHelp_triggered()
{

   openHelp();
}

void SimWin::on_actionOpen_Launcher_triggered()
{
   launchPopup();
}


void SimWin::on_cellAccom_valueChanged(double /*arg1*/)
{
   cellChanged();
}

void SimWin::on_cellKCond_valueChanged(double /*arg1*/)
{
  cellChanged();
}

void SimWin::on_cellMembTC_valueChanged(double /*arg1*/)
{
   cellChanged();
}

void SimWin::on_cellKCondChg_valueChanged(double /*arg1*/)
{
   cellChanged();
}

void SimWin::on_cellAccomParam_valueChanged(double /*arg1*/)
{
   cellChanged();
}

void SimWin::on_cellDCInj_valueChanged(double /*arg1*/)
{
   cellChanged();
}

void SimWin::on_cellRestThresh_valueChanged(double /*arg1*/)
{
   cellChanged();
}

void SimWin::on_cellRestThreshSD_valueChanged(double /*arg1*/)
{
   cellChanged();
}

void SimWin::on_noiseAmp_valueChanged(double /*arg1*/)
{
   cellChanged();
}

void SimWin::on_cellPopSize_valueChanged(int /*arg1*/)
{
   cellChanged();
}

void SimWin::on_cellComment_textEdited(const QString &/*arg1*/)
{
   cellChanged();
}

void SimWin::on_paramTabs_currentChanged(int /*index*/)
{
//   cout << "tab changed" << endl;
}

void SimWin::on_paramTabs_tabCloseRequested(int /*index*/)
{
//   cout << "tab closing" << endl;
}

void SimWin::on_cellApply_clicked()
{
   scene->cellApply();
}

void SimWin::on_cellUndo_clicked()
{
   scene->cellUndo();
}

void SimWin::on_fiberUndo_clicked()
{
   scene->fiberUndo();
}

void SimWin::on_fiberApply_clicked()
{
   scene->fiberApply();
}

void SimWin::on_fibProbFire_valueChanged(double /*arg1*/)
{
   fiberChanged();
}

void SimWin::on_fibRndSeed_valueChanged(int /*arg1*/)
{
   fiberChanged();
}

void SimWin::on_fibNumPop_valueChanged(int /*arg1*/)
{
   fiberChanged();
}


void SimWin::on_fibComment_textEdited(const QString &/*arg1*/)
{
   fiberChanged();
}

void SimWin::on_actionUnsaved_Changes_Warning_Off_triggered(bool checked)
{
   warningsOff = checked;
}

void SimWin::on_synapseMinCond_valueChanged(int /*arg1*/)
{
   synapseChanged();
}

void SimWin::on_synapseMaxCond_valueChanged(int /*arg1*/)
{
   synapseChanged();
}

void SimWin::on_synapseNumTerm_valueChanged(int /*arg1*/)
{
   synapseChanged();
}

void SimWin::on_synapseSynStren_valueChanged(double /*arg1*/)
{
   synapseChanged();
}

void SimWin::on_synapseRndSeed_valueChanged(int /*arg1*/)
{
   synapseChanged();
}

void SimWin::on_synapseUndo_clicked()
{
   scene->synapseUndo();
}

void SimWin::on_synapseApply_clicked()
{
   scene->synapseApply();
}

void SimWin::on_buildUndo_clicked()
{
   scene->buildUndo();
}

void SimWin::on_buildApply_clicked()
{
   scene->buildApply();
}

void SimWin::on_globComment_textEdited(const QString &/*arg1*/)
{
   globalsChanged();
}

void SimWin::on_globLength_textEdited(const QString &/*arg1*/)
{
   globalsChanged();
}

void SimWin::on_globPhrenicRecruit_textEdited(const QString &/*arg1*/)
{
   globalsChanged();
}

void SimWin::on_globKEqPot_textEdited(const QString &/*arg1*/)
{
   globalsChanged();
}

void SimWin::on_globStepTime_textEdited(const QString &/*arg1*/)
{
   globalsChanged();
}

void SimWin::on_globLumbarRecruit_textEdited(const QString &/*arg1*/)
{
   globalsChanged();
}

void SimWin::on_globUndo_clicked()
{
   scene->globalsUndo();
}

void SimWin::on_globApply_clicked()
{
   scene->globalsApply();
}

bool SimWin::needToSave()
{
   if (chkForDrawing())
      return false;
   return scene->allChkDirty();
}

void SimWin::on_iLmElmFiringRate_textEdited(const QString &/* arg1*/)
{
   globalsChanged();
}

void SimWin::on_buildAdd_clicked()
{
   scene->addSynType();
}
void SimWin::dobuildAddPre()
{
   scene->addSynPre();
}

void SimWin::dobuildAddPost()
{
   scene->addSynPost();
}

void SimWin::dobuildAddLearn()
{
   scene->addSynLearn();
}

void SimWin::on_buildDel_clicked()
{
   scene->delSynType();
}

void SimWin::on_lungPhrenicCell_toggled(bool checked)
{
   doLungPhren(checked);
}

void SimWin::on_lungLumbarCell_toggled(bool checked)
{
   doLungLumb(checked);
}

void SimWin::on_lungInspiLarCell_toggled(bool checked)
{
   doLungInspLar(checked);
}

void SimWin::on_lungExpirLarCell_toggled(bool checked)
{
   doLungExpiLar(checked);
}


// Open with the default .txt editor. Really should not be here if
// we do not have a snd file on disk.
void SimWin::on_actionView_Change_Log_triggered()
{
   int ret;
   QString viewCmd, quote;
#ifdef Q_OS_LINUX
   viewCmd = "xdg-open ";
   quote = "'";
#else
   viewCmd = "start \"\" ";  // Windows, need blank title arg for start cmd
   quote = "\"";
#endif
   QFileInfo info(currSndFile);
   QString logName = makeChgName(currSndFile);
   viewCmd += quote + logName + quote;
   // the file functions in Qt expect "/" as the separator for all paths/names.
   // Since we are going to use a windows shell to execute the cmd, it expects
   // "\" as the sep.
#if defined Q_OS_WIN
   logName = logName.replace('/','\\');
#endif
   printMsg("View log cmd is ");
   printMsg(viewCmd);
   ret = system(viewCmd.toLatin1().data());
   if (ret != 0)
      printMsg("There was a problem launching the default text editor.");
}

void SimWin::on_actionOpen_Users_Manual_triggered()
{
   int ret;
#ifdef Q_OS_LINUX
   ret = system("xdg-open /usr/local/share/doc/usfsim/sim.pdf");
#else
   char path[2048];
   GetModuleFileNameA(NULL,path,sizeof(path)-1);
   for (int len = strlen(path); len > 0; --len)
      if (path[len] != '\\'  && path[len] != '/')
         path[len] = 0;
      else
         break;
   QString cmd;       // Windows, what dir are we being run from?
   cmd = "start \"\" ";
   cmd += "\"";
   cmd += path;
   cmd += "sim.pdf\"";
printf("pdf cmd is %s\n",cmd.toLatin1().data());
fflush(stdout);
   ret = system(cmd.toLatin1().data());
#endif
   if (ret != 0)
      printMsg("There was a problem launching the default PDF file viewer.");
}

void SimWin::on_buildAddPre_clicked()
{
  dobuildAddPre();
}

void SimWin::on_buildAddPost_clicked()
{
   dobuildAddPost();
}


void SimWin::on_actionEdit_Globals_triggered()
{
   showGlobals();
}

void SimWin::closeEvent(QCloseEvent* /*evt*/)
{
   timeToQuit();
}

void SimWin::timeToQuit() // We never come back from this
{
   if (scene->allChkDirty())
      saveSnd();
   saveSettings();
   if (launch)
   {
      launch->reParent(true); // so it will close when main win does
      launch->close();
   }
}



void SimWin::on_globalBdt_clicked()
{
   doGlobalBdt();
}


void SimWin::on_globalEdt_clicked()
{
   doGlobalEdt();
}

void SimWin::on_globalOther_clicked()
{
   doGlobalOther();
}

void SimWin::on_actionupdateChgLog_triggered()
{
   doUpdChgLog();
}


void SimWin::on_actionPrompt_For_Change_Log_triggered(bool checked)
{
   doChgLogPrompt(checked);
}


void SimWin::on_bursterTimeConst_valueChanged(const QString &/*arg1*/)
{
  cellChanged();
}

void SimWin::on_bursterSlopeH_valueChanged(const QString &/*arg1*/)
{
  cellChanged();
}

void SimWin::on_bursterHalfVAct_valueChanged(const QString &/*arg1*/)
{
  cellChanged();
}

void SimWin::on_bursterSlopeAct_valueChanged(const QString &/*arg1*/)
{
  cellChanged();
}

void SimWin::on_bursterResetV_valueChanged(const QString &/*arg1*/)
{
  cellChanged();
}

void SimWin::on_bursterThreshV_valueChanged(const QString &/*arg1*/)
{
  cellChanged();
}

void SimWin::on_bursterHalfVH_valueChanged(const QString &/*arg1*/)
{
  cellChanged();
}

void SimWin::on_bursterKCond_valueChanged(const QString &/*arg1*/)
{
  cellChanged();
}

void SimWin::on_bursterDeltaH_valueChanged(const QString &/*arg1*/)
{
  cellChanged();
}

void SimWin::on_bursterAppliedCurr_valueChanged(const QString &/*arg1*/)
{
  cellChanged();
}

void SimWin::on_psrRiseT_valueChanged(const QString &/*arg1*/)
{
  cellChanged();
}

void SimWin::on_psrFallT_valueChanged(const QString &/*arg1*/)
{
  cellChanged();
}

void SimWin::on_psrOutThresh_valueChanged(const QString &/*arg1*/)
{
  cellChanged();
}

void SimWin::on_psrOutThreshSd_valueChanged(const QString &/*arg1*/)
{
  cellChanged();
}

void SimWin::on_psrCellPopSize_valueChanged(const QString & /*arg1*/)
{
   cellChanged();
}

void SimWin::on_bursterCellPopSize_valueChanged(const QString &/*arg1*/)
{
   cellChanged();
}

void SimWin::on_actionCreate_Group_triggered()
{
   doCreateGroup();
}

void SimWin::on_bursterNoiseAmp_valueChanged(const QString &/*arg1*/)
{
   cellChanged();
}

void SimWin::on_actionImport_Group_triggered()
{
   doImportGroup();
}

void SimWin::on_actionUngroup_triggered()
{
   doUnGroup();
}

void SimWin::on_actionExport_Group_triggered()
{
   doExportGroup();
}

void SimWin::on_actionSelect_Group_triggered()
{
   doSelectGroup();
}

void SimWin::on_fiberNormal_clicked(bool checked)
{
   doFiberNormal(checked);
}

void SimWin::on_fiberElectric_clicked(bool checked)
{
   doFiberElectric(checked);
}

void SimWin::on_fiberElectricDeterministic_clicked(bool checked)
{
  doFiberTypeFixed(checked);
}

void SimWin::on_fiberElectricFuzzy_clicked(bool checked)
{
   doFiberTypeFuzzy(checked);
}

void SimWin::on_fiberElectricFreq_valueChanged(const QString &/*arg1*/)
{
   fiberChanged();
}

void SimWin::on_fiberElectricFuzzyRange_valueChanged(const QString &/*arg1*/)
{
   fiberChanged();
}

void SimWin::on_fiberElectricSeed_valueChanged(const QString &/*arg1*/)
{
   fiberChanged();
}

void SimWin::on_fiberElectricStartTime_valueChanged(const QString &/*arg1*/)
{
   fiberChanged();
}

void SimWin::on_fiberElectricStopTime_valueChanged(const QString &/*arg1*/)
{
   fiberChanged();
}

void SimWin::on_fiberAfferent_clicked(bool checked)
{
   doFiberAfferent(checked);
}

void SimWin::on_actionLaunch_Window_Always_On_Top_toggled(bool arg1)
{
   doLaunchOnTop(arg1);
}

void SimWin::on_fibStartFire_valueChanged(const QString &/*arg1*/)
{
   fiberChanged();
}

void SimWin::on_fibEndFire_valueChanged(const QString &/*arg1*/)
{
   fiberChanged();
}

void SimWin::on_actionUseSndName_triggered(bool checked)
{
   useSndForChangelog = checked;
}

void SimWin::on_actionOpen_Updated_Manual_in_progress_triggered()
{
   int ret;
#ifdef Q_OS_LINUX
   ret = system("xdg-open /usr/local/share/doc/usfsim/usfsim.pdf");
#else
   char path[2048];
   GetModuleFileNameA(NULL,path,sizeof(path)-1);
   for (int len = strlen(path); len > 0; --len)
      if (path[len] != '\\'  && path[len] != '/')
         path[len] = 0;
      else
         break;
   QString cmd;       // Windows, what dir are we being run from?
   cmd = "start \"\" ";
   cmd += "\"";
   cmd += path;
   cmd += "usfsim.pdf\"";
printf("pdf cmd is %s\n",cmd.toLatin1().data());
fflush(stdout);
   ret = system(cmd.toLatin1().data());
#endif
   if (ret != 0)
      printMsg("There was a problem launching the default PDF file viewer.");

}

void SimWin::on_actionOpen_Release_Notes_triggered()
{
   int ret;
#ifdef Q_OS_LINUX
   ret = system("xdg-open /usr/local/share/doc/usfsim/ReleaseNotes.pdf");
#else
   char path[2048];
   GetModuleFileNameA(NULL,path,sizeof(path)-1);
   for (int len = strlen(path); len > 0; --len)
      if (path[len] != '\\'  && path[len] != '/')
         path[len] = 0;
      else
         break;
   QString cmd;       // Windows, what dir are we being run from?
   cmd = "start \"\" ";
   cmd += "\"";
   cmd += path;
   cmd += "ReleaseNotes.pdf\"";
printf("pdf cmd is %s\n",cmd.toLatin1().data());
fflush(stdout);
   ret = system(cmd.toLatin1().data());
#endif
   if (ret != 0)
      printMsg("There was a problem launching the default PDF file viewer.");
}

void SimWin::on_actionFind_triggered()
{
   doFindPopulation();
}

void SimWin::on_actionSave_Model_as_PDF_triggered()
{
   doPDF();
}

void SimWin::on_actionDisplay_Model_as_Monochrome_triggered(bool checked)
{
  doMonochrome(checked);
}

void SimWin::on_fiberSendLaunchPlot_clicked()
{
    fiberSendToLauncherPlot();
}

void SimWin::on_afferentFileButton_clicked()
{
   selAfferentFile();
}


void SimWin::on_fiberAfferentStartFire_valueChanged(const QString &/*arg1*/)
{
   fiberChanged();
}

void SimWin::on_fiberAfferentNumPop_valueChanged(const QString &/*arg1*/)
{
   fiberChanged();
}

void SimWin::on_fiberAfferentStopFire_valueChanged(const QString &/*arg1*/)
{
   fiberChanged();
}

void SimWin::on_afferentFileName_textEdited(const QString &/*arg1*/)
{
   fiberChanged();
}

void SimWin::on_addAfferentRow_clicked()
{
    addAfferentRow();
}

void SimWin::on_delAfferentRow_clicked()
{
   delAfferentRow();
}

void SimWin::on_fiberAffSeed_valueChanged(int /*arg1*/)
{
   fiberChanged();
}

void SimWin::on_fiberAfferentOffset_valueChanged(int /*arg1*/)
{
   fiberChanged();
}

void SimWin::on_buildAddLearn_clicked()
{
   dobuildAddLearn();
}

void SimWin::on_fiberAfferentScale_valueChanged(const QString &/*arg1*/)
{
   fiberChanged();
}
