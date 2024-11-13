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

#ifndef SIMWIN_H
#define SIMWIN_H

#include <QMainWindow>
#include <QKeyEvent>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QColor>
#include <QFileDialog>
#include <QSortFilterProxyModel>
#include <QFileSystemModel>
#include <QAction>
#include <QItemDelegate>
#include <iostream>
#include "simscene.h"
#include "affmodel.h"
#include "ui_simwin.h"
#include "launchwindow.h"
#include "launch_model.h"
#include "finddialog.h"

extern bool Debug;

namespace Ui {
class SimWin;
}

const int maxRecent=10;

class SimWin : public QMainWindow
{
    Q_OBJECT

   friend SimScene;
   friend launchWindow;

   public:
      explicit SimWin(QWidget *parent = 0);
      ~SimWin();

      QGraphicsView* getView();
      connMode getConnMode(){return connectorMode;}
      void setConnMode(connMode state){connectorMode = state;}
      bool needToSave();
      void printMsg(QString);
      void launchChg();
      void loadSnd(const QString* name=nullptr);
      void saveSnd();
      void saveSim(const QString* name=nullptr); 
      void setStepCount(int stepCount);
      int getStepCount();

   protected:
      void closeEvent(QCloseEvent *evt) override;

   private slots:
      void on_actionQuit_2_triggered();
      void on_actionAdd_Fiber_triggered();
      void on_actionAddCell_triggered();
      void on_actionAddConnector_triggered();
      void on_actionDeleteSelected_triggered();
      void on_cellShowTarg_clicked();
      void on_cellShowSrc_clicked();
      void on_fiberShowTarg_clicked();
      void on_actionQuit_triggered();
      void on_actionLoad_snd_File_triggered();
      void on_standardCell_toggled(bool checked);
      void on_bursterCell_toggled(bool checked);
      void on_psrCell_toggled(bool checked);
      void on_actionBuild_Synapse_Globals_triggered();
      void on_actionOne_Shot_Add_triggered(bool checked);
      void on_actionNew_triggered();
      void on_actionRun_Simulation_triggered();
      void on_cellSendLaunchBdt_clicked();
      void on_cellSendLaunchPlot_clicked();
      void on_fiberSendLaunchBdt_clicked();
      void on_selAll_clicked();
      void on_fitToView_clicked();
      void on_actionSave_snd_File_triggered();
      void on_actionSave_sim_File_triggered();
      void on_actionShell_triggered();
      void on_lineLayer_clicked();
      void on_actionHelp_triggered();
      void on_actionOpen_Launcher_triggered();
      void on_cellAccom_valueChanged(double arg1);
      void on_cellKCond_valueChanged(double arg1);
      void on_cellMembTC_valueChanged(double arg1);
      void on_cellKCondChg_valueChanged(double arg1);
      void on_cellAccomParam_valueChanged(double arg1);
      void on_cellDCInj_valueChanged(double arg1);
      void on_cellRestThresh_valueChanged(double arg1);
      void on_cellRestThreshSD_valueChanged(double arg1);
      void on_noiseAmp_valueChanged(double arg1);
      void on_cellPopSize_valueChanged(int arg1);
      void on_cellComment_textEdited(const QString &arg1);
      void on_paramTabs_currentChanged(int index);
      void on_paramTabs_tabCloseRequested(int index);
      void on_cellApply_clicked();
      void on_cellUndo_clicked();
      void on_fiberUndo_clicked();
      void on_fiberApply_clicked();
      void on_fibProbFire_valueChanged(double arg1);
      void on_fibRndSeed_valueChanged(int arg1);
      void on_fibNumPop_valueChanged(int arg1);
      void on_fibComment_textEdited(const QString &arg1);
      void on_actionUnsaved_Changes_Warning_Off_triggered(bool checked);
      void on_synapseMinCond_valueChanged(int arg1);
      void on_synapseMaxCond_valueChanged(int arg1);
      void on_synapseNumTerm_valueChanged(int arg1);
      void on_synapseSynStren_valueChanged(double arg1);
      void on_synapseRndSeed_valueChanged(int arg1);
      void on_synapseUndo_clicked();
      void on_synapseApply_clicked();
      void on_buildUndo_clicked();
      void on_buildApply_clicked();
      void on_buildAdd_clicked();
      void on_buildDel_clicked();
      void on_globComment_textEdited(const QString &arg1);
      void on_globLength_textEdited(const QString &arg1);
      void on_globPhrenicRecruit_textEdited(const QString &arg1);
      void on_globKEqPot_textEdited(const QString &arg1);
      void on_globStepTime_textEdited(const QString &arg1);
      void on_globLumbarRecruit_textEdited(const QString &arg1);
      void on_globUndo_clicked();
      void on_globApply_clicked();
      void on_iLmElmFiringRate_textEdited(const QString &arg1);
      void on_lungPhrenicCell_toggled(bool checked);
      void on_lungLumbarCell_toggled(bool checked);
      void on_lungInspiLarCell_toggled(bool checked);
      void on_lungExpirLarCell_toggled(bool checked);
      void on_actionView_Change_Log_triggered();
      void on_actionOpen_Users_Manual_triggered();
      void on_buildAddPre_clicked();
      void on_buildAddPost_clicked();
      void on_actionEdit_Globals_triggered();
      void on_globalBdt_clicked();
      void on_globalEdt_clicked();
      void on_globalOther_clicked();
      void on_actionupdateChgLog_triggered();
      void on_actionPrompt_For_Change_Log_triggered(bool checked);
      void on_bursterTimeConst_valueChanged(const QString &arg1);
      void on_bursterSlopeH_valueChanged(const QString &arg1);
      void on_bursterHalfVAct_valueChanged(const QString &arg1);
      void on_bursterSlopeAct_valueChanged(const QString &arg1);
      void on_bursterResetV_valueChanged(const QString &arg1);
      void on_bursterThreshV_valueChanged(const QString &arg1);
      void on_bursterHalfVH_valueChanged(const QString &arg1);
      void on_bursterKCond_valueChanged(const QString &arg1);
      void on_bursterDeltaH_valueChanged(const QString &arg1);
      void on_bursterAppliedCurr_valueChanged(const QString &arg1);
      void on_psrRiseT_valueChanged(const QString &arg1);
      void on_psrFallT_valueChanged(const QString &arg1);
      void on_psrOutThresh_valueChanged(const QString &arg1);
      void on_psrOutThreshSd_valueChanged(const QString &arg1);
      void on_psrCellPopSize_valueChanged(const QString &arg1);
      void on_bursterCellPopSize_valueChanged(const QString &arg1);
      void on_actionCreate_Group_triggered();
      void on_bursterNoiseAmp_valueChanged(const QString &arg1);
      void on_actionImport_Group_triggered();
      void on_actionUngroup_triggered();
      void on_actionExport_Group_triggered();
      void on_actionSelect_Group_triggered();
      void on_fiberNormal_clicked(bool checked);
      void on_fiberElectric_clicked(bool checked);
      void on_fiberElectricDeterministic_clicked(bool checked);
      void on_fiberElectricFuzzy_clicked(bool checked);
      void on_fiberElectricFreq_valueChanged(const QString &arg1);
      void on_fiberElectricFuzzyRange_valueChanged(const QString &arg1);
      void on_fiberElectricSeed_valueChanged(const QString &arg1);
      void on_fiberElectricStartTime_valueChanged(const QString &arg1);
      void on_fiberElectricStopTime_valueChanged(const QString &arg1);
      void on_actionLaunch_Window_Always_On_Top_toggled(bool arg1);
      void on_fibStartFire_valueChanged(const QString &arg1);
      void on_fibEndFire_valueChanged(const QString &arg1);
      void on_actionUseSndName_triggered(bool checked);
      void on_actionOpen_Updated_Manual_in_progress_triggered();
      void on_actionOpen_Release_Notes_triggered();
      void on_actionFind_triggered();
      void showFindItem(QListWidgetItem&);
      void on_actionSave_Model_as_PDF_triggered();
      void on_actionDisplay_Model_as_Monochrome_triggered(bool checked);
      void on_fiberSendLaunchPlot_clicked();
      void on_afferentFileButton_clicked();
      void on_fiberAfferent_clicked(bool checked);
      void on_fiberAfferentStartFire_valueChanged(const QString &arg1);
      void on_fiberAfferentNumPop_valueChanged(const QString &arg1);
      void on_fiberAfferentStopFire_valueChanged(const QString &arg1);
      void on_afferentFileName_textEdited(const QString &arg1);
      void on_addAfferentRow_clicked();
      void on_delAfferentRow_clicked();
      void on_fiberAffSeed_valueChanged(int arg1);
      void on_fiberAfferentOffset_valueChanged(int arg1);
      void on_buildAddLearn_clicked();
      void openRecent();

      void on_fiberAfferentScale_valueChanged(const QString &arg1);

   protected:
      void keyPressEvent(QKeyEvent *) override;
      void styleScroll();

   private:
      void clearScene();
      void resetNew();
      void newFile();
      void addFiber();
      void addCell();
      void addConnector();
      void deleteSelected();
      void simInit();
      void initScene();
      void doCellRetro();
      void doCellAntero();
      void doFiberRetro();
      void doStdCell(bool);
      void doBurster(bool);
      void doPsr(bool);
      void doLungPhren(bool);
      void doLungLumb(bool);
      void doLungInspLar(bool);
      void doLungExpiLar(bool);
      void doFiberNormal(bool);
      void doFiberElectric(bool);
      void doFiberAfferent(bool);
      void addAfferentRow();
      void delAfferentRow();
      void doOneShotAdd(bool);
      void addFiberOff();
      void addCellOff();
      void addConnectorOff();
      void showBuildSynapse();
      void showGlobals();
      void doNewFile();
      void launchPopup();
      void launchIsClosing();
      void doGlobalBdt();
      void doGlobalEdt();
      void doGlobalOther();
      void doUpdChgLog();
      void doFiberTypeFixed(bool);
      void doFiberTypeFuzzy(bool);
      void fiberSendToLauncherBdt();
      void fiberSendToLauncherPlot();
      void cellSendToLauncherBdt();
      void cellSendToLauncherPlot();
      void fitToView();
      void doSelAll();
      void connLayerToggle();
      void doPDF();
      void openHelp();
      void cellChanged();
      void fiberChanged();
      void synapseChanged();
      void buildChanged();
      void globalsChanged();
      void loadDefaults();
      void saveSettings();
      void loadSettings();
      void dobuildAddPre();
      void dobuildAddPost();
      void dobuildAddLearn();
      void doChgLogPrompt(bool flag);
      void timeToQuit();
      void doCreateGroup();
      void doImportGroup();
      void doUnGroup();
      void doExportGroup();
      void doSelectGroup();
      void doLaunchOnTop(bool);
      void buildFindList();
      void doFindPopulation();
      void selAfferentFile();
      QString makeChgName(QString sndName);
      bool chkForDrawing();
      void doMonochrome(bool);
      void affRecToModel(F_NODE*);
      void affModelToRec(F_NODE*);
      void updateRecentFiles();
      void loadRecent(const QString&);
      void delRecent(const QString&);

      bool addFiberToggle=false;
      bool addCellToggle=false;
      bool fitZoom=false;
      connMode connectorMode=connMode::OFF;
      bool oneShotAddFlag=false;
      launchWindow *launch=nullptr;
      bool cRetroOn=false;
      bool cAnteroOn=false;
      bool fRetroOn=false;
      bool linesOnTop=false;
      bool warningsOff=false;
      bool logPromptOff=false;
      bool useSndForChangelog=false;
      bool launchOnTop=true;
      bool showLaunch=false;
      bool showMono=false;
      findDialog *findDlg=nullptr;
      QString currSndFile;
      QString currSimFile;
      QScrollArea *scrollArea;
      affModel *afferentModel;
      QAction *recentFiles[maxRecent];

      SimScene *scene;
      Ui::SimWin *ui;
};

#endif // SIMWIN_H
