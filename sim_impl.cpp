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


#include <QCoreApplication>
#include <QMessageBox>
#include <QSettings>
#include <QValidator>
#include <QPrinter>
#include <QtGlobal>
#include <QScrollArea>
#include <QPageSize>
#include <QPageLayout>

#include "simwin.h"
#include "ui_simwin.h"
#include "sim2build.h"
#include "colormap.h"
#include "c_globals.h"
#include "helpbox.h"
#include "chglog.h"
#include "fileio.h"

#if defined Q_OS_WIN
#include <windows.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <iostream>
#include <fstream>
#endif

using namespace std;

extern bool Debug;
// These magic numbers come from the designer.
// No property holds these.  This is what looks good
// on my monitor. Maybe not on others.
int winWidth = 1900;
int winHeight = 940;


// JAH additions
void SimWin::setStepCount(int stepCount) {
   GLOBAL_NETWORK *gn = &D.inode[GLOBAL_INODE].unode.global_node;
   gn->sim_length = stepCount;      
}

int SimWin::getStepCount() {
   GLOBAL_NETWORK *gn = &D.inode[GLOBAL_INODE].unode.global_node;
   return gn->sim_length;  
}
// end JAH additions

// sim scene, var & other setups from original newsned.c
void SimWin::simInit()
{
#if defined Q_OS_WIN
   HANDLE stdHandle;
   int con;
   FILE* fp;

   if (Debug)
   {
      if (!AttachConsole(ATTACH_PARENT_PROCESS))
            AllocConsole();
      stdHandle = GetStdHandle(STD_OUTPUT_HANDLE);
      con = _open_osfhandle((long long) stdHandle, _O_TEXT);
      fp = _fdopen(con,"w");
      *stdout = *fp;
      setvbuf(stdout,NULL,_IONBF,0);

      stdHandle = GetStdHandle(STD_ERROR_HANDLE);
      con = _open_osfhandle((long long) stdHandle, _O_TEXT);
      fp = _fdopen(con,"w");
      *stderr = *fp;
      setvbuf(stderr,NULL,_IONBF,0);
      printf("Console for simbuild\n");
   }
#endif
   afferentModel = new affModel(this);

   loadSettings();
   launch = new launchWindow(this);  // we make just one of these and
                                     // show/hide it as needed.
   findDlg = new findDialog(this);   // And one of these
   connect(findDlg,&findDialog::findThisOne,this,&SimWin::showFindItem);

   initScene();
   resetNew();
   createSynapseColors();

     // these lets us move things around in the toolbar at the top
   QWidget* spacer = new QWidget();
   spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
   ui->mainToolBar->insertWidget(ui->actionAdd_Fiber,spacer);
   QWidget* spacer1 = new QWidget();
   spacer1->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
   ui->mainToolBar->addWidget(spacer1);

   ui->globStepTime->setValidator(new QDoubleValidator(0.00001,10.0,5));
   ui->globLength->setValidator(new QDoubleValidator(1,86400,2));
   ui->iLmElmFiringRate->setValidator(new QDoubleValidator(1.0,10000.0,5));

   // setup afferent table
   connect(afferentModel,&affModel::notifyChg,this,&SimWin::fiberChanged);
   ui->afferentTable->setModel(afferentModel);
   ui->afferentTable->setAcceptDrops(true);
   ui->afferentTable->setStyle(new dropStyle(style()));
   ui->afferentTable->setItemDelegateForColumn(0, new affSrcDelegate);
   ui->afferentTable->setItemDelegateForColumn(1, new affProbDelegate);
   ui->afferentTable->horizontalHeader()->setVisible(true);
   ui->afferentTable->verticalHeader()->setVisible(true);

     // I always use this style for Quit clickage
   ui->mainToolBar->widgetForAction(ui->actionQuit_2)->setStyleSheet("QWidget {color: darkred; font-style:bold;}");

     // we use hidden line edit on pages that show an object to keep
     // track of which object's params we are showing.
     // for debugging and testing, comment these out to leave them visible
   ui->cellObjId->hide();
   ui->fiberObjId->hide();
   ui->synapseObjId->hide();

    // This make sure you can scroll all the controls into
    // view even on smaller/lower res  monitors.
   ui->centralWidget->setMinimumSize(winWidth,winHeight);

   scrollArea = new QScrollArea(this);
   scrollArea->setWidgetResizable(true);
   scrollArea->setWidget(ui->centralWidget);
   QGridLayout *lay= new QGridLayout;
   scrollArea->setLayout(lay);
   setCentralWidget(scrollArea);
   styleScroll();
}

// Make the scroll area scrollbars a bit different because there
// are adjacent scrollbars in the drawing area and it is easy to get confused.
void SimWin::styleScroll()
{
   QString style;

      // Vertical
   style += " QScrollBar:vertical {" 
    " border: 2px solid grey;"
    " background: #c0c0c0;"
    " width:20px;    "
    " margin: 21px 0px 21px 0px;"
    "}";
    style += " QScrollBar::handle:vertical {"
    " background: #bad8ef;"
    " min-height: 10px;"
    "}";

       // UP
   style += " QScrollBar::sub-line:vertical {"
    " border: 1px solid #b0b0b0;"
    " background-color: #b0b0b0;"
    " height: 20 px;"
    " subcontrol-position: top;"
    " subcontrol-origin: margin;"
    "}";
//   style += " QScrollBar::up-button:vertical {"
//      " border: none; "
//      " background: none;"
//    " background-color: #b0b0b0;"
//      "}";
   style += " QScrollBar::up-arrow:vertical {"
//      " border: none; "
      " background-image: url(:/uparrow.png);"
      " background-repeat: repeat-n;"
      " background-position: center;"
      "}";
   style += " QScrollBar::sub-page:vertical {"
      " background: none;"
      "}";

       // DOWN
   style += " QScrollBar::add-line:vertical {"
    " border: 1px solid #b0b0b0;"
    " background-color: #b0b0b0;"
    " height: 20 px;"
    " subcontrol-position: bottom;"
    " subcontrol-origin: margin;"
    "}";
   style += " QScrollBar::down-arrow:vertical {"
      " background-image: url(:/downarrow.png);"
      " background-repeat: repeat-n;"
      " background-position: center;"
      "}";
   style += " QScrollBar::add-page:vertical {"
      " background: none;"
      "}";

    // Horizontal
   style += "\nQScrollBar:horizontal {" 
    " border: 2px solid grey;"
    " background: #b0b0b0;"
    " height: 20px; "
    " margin: 0 22px 0px 22px;"
    "}";
   style += " QScrollBar::handle:horizontal {"
    " background: #bad8ef;"
    " min-width: 40px;"
    "}";

     // LEFT
   style += " QScrollBar::sub-line:horizontal {"
    " border: 1px solid #b0b0b0;"
    " background-color: #b0b0b0;"
    " width: 20 px;"
    " subcontrol-position: left;"
    " subcontrol-origin: margin;"
    "}";
   style += " QScrollBar::left-arrow:horizontal {"
      " background-image: url(:/leftarrow.png);"
      " background-repeat: repeat-n;"
      " background-position: center;"
      "}";
   style += " QScrollBar::sub-page:horizontal {"
      " background: none;"
      "}";

     //RIGHT
   style += " QScrollBar::add-line:horizontal {"
    " border: 1px solid #b0b0b0;"
    " background-color: #b0b0b0;"
    " width: 20px;"
    " subcontrol-position: right;"
    " subcontrol-origin: margin;"
    "}";
   style += " QScrollBar::right-arrow:horizontal {"
      " background-image: url(:/rightarrow.png);"
      " background-repeat: repeat-n;"
      " background-position: center;"
      "}";
   style += " QScrollBar::add-page:horizontal {"
      "  background: none;"
      "}";

   scrollArea->setStyleSheet(style);


}

void SimWin::initScene()
{
   scene = new SimScene(this);
   scene->setSceneRect(0,0,SCENE_SIZE,SCENE_SIZE);
   scene->setBackgroundBrush(scene->sceneBG);
    // there appears to be bugs when using bsp indexing. We don't have a lot
    // of objects, so this works for us.
   scene->setItemIndexMethod(QGraphicsScene::ItemIndexMethod::NoIndex);
   QFont font = QFont("Century Schoolbook L",20);
   font.setStyleStrategy(QFont::NoAntialias);
   scene->setFont(font);
   ui->simView->setScene(scene);
   ui->simView->centerOn(0,0);
   ui->simView->setMouseTracking(true);
   ui->simView->setDragMode(QGraphicsView::RubberBandDrag);
   ui->simView->setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);

     // all hidden until there is something in graphic win
   ui->paramTabs->clear();
     // leave 1st one in child tab selected by default
   int numtabs = ui->cellTypes->count()-1;
   for (int tab=0; tab < numtabs; ++tab)
      ui->cellTypes->removeTab(1);
   numtabs = ui->fiberTypes->count()-1;
   for (int tab=0; tab < numtabs; ++tab)
      ui->fiberTypes->removeTab(1);
}


void SimWin::loadSettings()
{
   bool onoff;
   QSettings settings("simulator","simbuild");
   if (settings.status() == QSettings::NoError  && settings.contains("geometry")) // does it exist at all?
   {
      restoreGeometry(settings.value("geometry").toByteArray());
      restoreState(settings.value("windowState").toByteArray());
      ui->splitter->restoreState(settings.value("splitterSizes").toByteArray());
      ui->buildSynapsesView->header()->restoreState(settings.value("buildHeader").toByteArray());
      onoff = settings.value("oneshot",false).toBool();
      ui->actionOne_Shot_Add->setChecked(onoff);
      oneShotAddFlag = onoff;
      onoff = settings.value("savewarn",false).toBool();
      ui->actionUnsaved_Changes_Warning_Off->setChecked(onoff);
      warningsOff = onoff;
      onoff = settings.value("promptlog",false).toBool();
      ui->actionPrompt_For_Change_Log->setChecked(onoff);
      logPromptOff = onoff;
      onoff = settings.value("usesndlog",false).toBool();
      ui->actionUseSndName->setChecked(onoff);
      useSndForChangelog = onoff;
      onoff = settings.value("launchTop",true).toBool();
      ui->actionLaunch_Window_Always_On_Top->setChecked(onoff);
      launchOnTop = onoff;
      onoff = settings.value("monoMode",false).toBool();
      ui->actionDisplay_Model_as_Monochrome->setChecked(onoff);
      showMono = onoff;
   }
     // Find second sep and add invisible slots before it
     // for recent file list.
   QList<QAction*> list = ui->menuFile->actions();
   int sep = 0;
   for (int loop = 0; loop < list.size(); ++loop)
   {
      if (list[loop]->isSeparator())
         ++sep;
      if (sep == 2) // before second sep
      {
         for (int add = 0; add < maxRecent; ++add)
         {
            recentFiles[add] = new QAction(this);
            recentFiles[add]->setVisible(false);
            ui->menuFile->insertAction(list[loop],recentFiles[add]);
            connect(recentFiles[add],&QAction::triggered,this,&SimWin::openRecent);
         }
         break;
      }
   }
   updateRecentFiles();
}


void SimWin::saveSettings()
{
   QSettings settings("simulator","simbuild");
   if (settings.status() == QSettings::NoError)
   {
      settings.setValue("geometry",saveGeometry());
      settings.setValue("windowState",saveState());
      settings.setValue("splitterSizes",ui->splitter->saveState());
      settings.setValue("buildHeader",ui->buildSynapsesView->header()->saveState());
      settings.setValue("oneshot",ui->actionOne_Shot_Add->isChecked());
      settings.setValue("savewarn",ui->actionUnsaved_Changes_Warning_Off->isChecked());
      settings.setValue("promptlog",ui->actionPrompt_For_Change_Log->isChecked());
      settings.setValue("usesndlog",ui->actionUseSndName->isChecked());
      settings.setValue("launchTop",ui->actionLaunch_Window_Always_On_Top->isChecked());
      settings.setValue("monoMode",ui->actionDisplay_Model_as_Monochrome->isChecked());
   }
}

// manage recent file list, thanks Qt guys for the examples
void SimWin::loadRecent(const QString& fileName)
{
   QSettings recent("simulator","recentfiles");
   if (recent.status() == QSettings::NoError)
   {
      QStringList files = recent.value("list").toStringList();
      files.removeAll(fileName);
      files.prepend(fileName);
      while (files.size() > maxRecent)
         files.removeLast();
      recent.setValue("list", files);
      updateRecentFiles();
   }
}

// file is gone, remove from list
void SimWin::delRecent(const QString& fileName)
{
   QSettings recent("simulator","recentfiles");
   if (recent.status() == QSettings::NoError)
   {
      QStringList files = recent.value("list").toStringList();
      files.removeAll(fileName);
      recent.setValue("list", files);
      updateRecentFiles();
   }
}

void SimWin::updateRecentFiles()
{
   QSettings recent("simulator","recentfiles");
   if (recent.status() == QSettings::NoError)
   {
      QStringList files = recent.value("list").toStringList();
      int num = min(files.size(), maxRecent);

      for (int line = 0; line < num; ++line) 
      {
         QString justname = QFileInfo(files[line]).fileName();
         QString text = tr("%1 %2").arg(line + 1).arg(justname);
         recentFiles[line]->setText(text);
         recentFiles[line]->setData(files[line]);
         recentFiles[line]->setVisible(true);
       }
       for (int line = num; line < maxRecent; ++line)
           recentFiles[line]->setVisible(false);
   }
}




// set up for reading in a new file
void SimWin::newFile()
{
   resetNew();
}

// a blank slate, new button
void SimWin::resetNew()
{
   init_nodes();      // init the overall nodal free-stack and node array
   init_sp();         // Init the launch space
   launch->resetModels();
   scene->resetModels();
   afferentModel->reset();
   Version = 5;       // defaults may be v5
   loadDefaults();
   scene->initControls();
   currSndFile = QString();
   currSimFile = QString();
   linesOnTop = true;
   connLayerToggle();
   Version = FILEIO_FORMAT_CURRENT; // but the in-memory version is current
   SubVersion = FILEIO_SUBVERSION_CURRENT;
   fitZoom = true;
   fitToView();
   scene->clearAllDirty();
   scene->clear();
   buildFindList();
   addCellOff();
   addFiberOff();
   addConnectorOff();
   ui->paramTabs->clear();
   ui->simView->centerOn(0,0);
   ui->actionupdateChgLog->setEnabled(false);
}

void SimWin::loadDefaults()
{
   global_loader();   // Load the global variables from globals.sned
   synapse_loader();  // Load the synapse variables from syn_def.sned
   fiber_loader();    // Load the fiber variables from fiber_def.sned
   cell_loader();     // Load the cell variables from cell_def.sned
}

void SimWin::keyPressEvent(QKeyEvent *k)
{
   switch(k->key())
   {
      case Qt::Key_Space:
      case Qt::Key_Escape:
         if (addCellToggle)
            addCellOff();
         else if (addFiberToggle)
            addFiberOff();
         else if (connectorMode != connMode::OFF)
            addConnectorOff();
         else
            QMainWindow::keyPressEvent(k);
         break;

      default:
         QMainWindow::keyPressEvent(k);
         break;
   }
}


void SimWin::loadSnd(const QString* filename)
{
   if (chkForDrawing())
      return;
   if (needToSave())
      saveSnd();
   QString fName;
   if (!filename)
   {
      QFileDialog openDlg(this,"Load A Simulation Model");

      openDlg.setFileMode(QFileDialog::ExistingFile);
      openDlg.setNameFilter(tr(".snd (*.snd )"));
      openDlg.setDirectory(QDir::current());
      openDlg.setLabelText(QFileDialog::DialogLabel::FileType,tr("Select .snd file to load"));
      openDlg.resize(QSize(900,500));
      if (!openDlg.exec())
         return;
      QStringList list = openDlg.selectedFiles();
      fName = list[0];
   }
   else
      fName = *filename;

   if (fName.length())
   {
      newFile();
      scene->clearAllDirty();
      Load_snd(fName.toLatin1().data());
      currSndFile = fName;
      QFileInfo info;
      info.setFile(fName);
      ui->currFile->setText(info.fileName());
      scene->drawFile();
      scene->initControls();
      launch->fileLoaded();
      printMsg("loaded " + fName);
      ui->actionupdateChgLog->setEnabled(true);
      QDir::setCurrent(info.path());
      buildFindList();
      loadRecent(fName);
   }
}


void SimWin::openRecent()
{
   QAction *action = qobject_cast<QAction *>(sender());
   if (action)
   {
      QString name(action->data().toString());
      // make sure it still exists
      QFileInfo check(name);
      if (check.exists() && check.isFile())
         loadSnd(&name);
      else
      {
         QMessageBox msgBox;
         msgBox.setIcon(QMessageBox::Warning);
         msgBox.setWindowTitle("File Does Not Exist");
         msgBox.setStandardButtons(QMessageBox::Ok);
         QString msg = "The file ";
         msg += name;
         msg += " does not exist, removing it from the recent file list.";
         msgBox.setText(msg);
         msgBox.exec();
         delRecent(name);
      }
   }
}

void SimWin::printMsg(QString msg)
{
   ui->chatterBox->appendPlainText(msg);
}

// If there are incomplete connector lines, caller should abort
// the operation, such as saving to a file.  
// Return true if there is an incomplete line.
bool SimWin::chkForDrawing()
{
   QMessageBox msgBox;
   if (connectorMode != connMode::OFF)
   {
      msgBox.setIcon(QMessageBox::Warning);
      msgBox.setWindowTitle("Finish Drawing The Model");
      msgBox.setStandardButtons(QMessageBox::Ok);
      QPushButton *del = msgBox.addButton("Delete Connector Now",QMessageBox::ApplyRole);
      msgBox.setText("There is an incomplete connector in the model drawing.\nFinish drawing the connector or delete the connector by pressing Space or Esc key or the Delete button before continuing with this operation.");
      msgBox.exec();
      if (msgBox.clickedButton() == del)
      {
         addConnectorOff();
         return false;
      }
      return true;
   }
   return false; 
}

void SimWin::saveSnd()
{
   QFileInfo info;
   QMessageBox msgBox;

   if (chkForDrawing())
      return;
   
   scene->chkControlsDirty();

   //QString fName = currSndFile;
   
   QFileDialog openDlg(this,"Save The Current Model");
   openDlg.setAcceptMode(QFileDialog::AcceptMode::AcceptSave);
   openDlg.setNameFilter(tr(".snd (*.snd )"));
   openDlg.selectFile(currSndFile);
   openDlg.setLabelText(QFileDialog::DialogLabel::FileType,tr("Save the model to .snd file"));
   openDlg.resize(QSize(900,500));
   openDlg.setOption(QFileDialog::DontConfirmOverwrite);
   openDlg.setDirectory(QDir::current());
   if (!openDlg.exec())
      return;
   QStringList list = openDlg.selectedFiles();
   QString fName = list[0];
   if (fName.length())
   {
       // warn if over-writing earlier file version
      QFile check(fName);
      info.setFile(fName);
      currSndFile = info.path() + "/" + info.completeBaseName() + ".snd";
      currSimFile = info.path() + "/" + info.completeBaseName() + ".sim";
      info.setFile(fName);

      if (info.exists())
      {
         check.open(QIODevice::ReadOnly);
         QTextStream in(&check);
         QString line = in.readLine();
         QStringList field=line.split(" ");
         check.close();
         if (field.length() == 0 || (field.length() >= 2 && field[1].toInt() < FILEIO_FORMAT_VERSION6))
         {
           msgBox.setIcon(QMessageBox::Question);
           msgBox.setWindowTitle("Overwrite Warning");
           msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
           msgBox.setText(tr("WARNING: You are overwriting an earlier verison of %1 with a newer version. The newsned program will not be able to read this file after you overwrite it. Are you sure you want to overwrite this file?").arg(info.fileName()));
           if (msgBox.exec() == QMessageBox::No)
              return;
         }
         else if (field.length() >= 2 && field[1].toInt() >= FILEIO_FORMAT_VERSION6)
         {
             // JAH: this is so annoying
             /*if (QMessageBox::warning(this, windowTitle(),
                tr("%1 already exists.\nDo you want to replace it?")
                .arg(info.fileName()),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::No)
                  return;
                  */
         }
      }
      // If a new .snd file, create initial change log file.  If existing file
      // use the current .snd file for comparisons even if this is a rename to
      // an existing .snd file. If the current file name is not the same as new
      // one, we may be overwriting an existing file whose contents may have
      // nothing in common with the current file.
      if (!logPromptOff)
      { 
         QString logName = makeChgName(currSndFile);
         unique_ptr<ChgLog> updLog;
         updLog = make_unique<ChgLog>(logName);

         if (!info.exists())  // new .snd file
            updLog->newChangeLog(currSndFile);
         else 
         {
            // JAH: this is so annoying
            /*msgBox.setIcon(QMessageBox::Question);
            msgBox.setWindowTitle("Update The Change Log File");
            msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            msgBox.setText("Do you want to update " + logName + "?\n\nIf Yes, the current model will be compared with the current file and changes will be added to " + logName + ".\n\nThe " + logName + "file will then be opened in the default text editor. You can simply close the window, or make additional notes in the file.");
            if (msgBox.exec() == QMessageBox::Yes)
            {*/
            updLog->updateChangeLog(currSndFile);
            
            // JAH: this is so annoying   
            // on_actionView_Change_Log_triggered();
            //}
         }
      }
      QDir::setCurrent(info.path());
      scene->saveSnd();  // make sure no dirty controls.
      launch->saveSnd();
      Save_snd(currSndFile.toLatin1().data());
      printMsg("Saved " + fName);
      scene->clearAllDirty();
      ui->currFile->setText(info.fileName());
      ui->actionupdateChgLog->setEnabled(true);
      loadRecent(fName);
   }
}

void SimWin::saveSim(const QString* fileName)
{
   QFileInfo info;
   QString fName;
   if (!fileName) {
       if (chkForDrawing())
          return;
       QFileDialog openDlg(this,"Save Simulation Run Time File");
       openDlg.setAcceptMode(QFileDialog::AcceptMode::AcceptSave);
       openDlg.setNameFilter(tr(".sim (*.sim )"));
       if (!currSimFile.length() && currSndFile.length())
       {
          info.setFile(currSndFile);  // create .sim name based on .snd name
          currSimFile = info.path() + "/" + info.completeBaseName() + ".sim";
       }
       openDlg.selectFile(currSimFile);
       openDlg.setLabelText(QFileDialog::DialogLabel::FileType,tr("Create a .sim file"));
       openDlg.resize(QSize(900,500));
       openDlg.setDirectory(QDir::current());
       if (!openDlg.exec())
          return;
       QStringList list = openDlg.selectedFiles();
       fName = list[0];
   }
   else {
       fName = *fileName;
   }
   
   if (fName.length())
   {
      info.setFile(fName);  // make sure a .sim extension
      currSimFile = info.path() + "/" + info.completeBaseName() + ".sim";
      scene->saveSnd();  // make vars that will be saved current.
      launch->saveSnd();
      Save_sim(currSimFile.toLatin1().data(),currSndFile.toLatin1().data(),currModel);
      QDir::setCurrent(info.path());
   }
}


void SimWin::doUpdChgLog()
{
   if (currSndFile.length() > 0)
   {
      unique_ptr<ChgLog> updLog;
      QString logName = makeChgName(currSndFile);
      updLog = make_unique<ChgLog>(logName);
      updLog->updateChangeLog(currSndFile);
      on_actionView_Change_Log_triggered(); // the view may want to do some stuff
   }
}

void SimWin::doPDF()
{
   QString name, msg;
   QTextStream stream(&msg);
   QFileInfo info;

   if (currSndFile.length() > 0)
   {
      info.setFile(currSndFile);
      name = info.path() + "/" + info.completeBaseName() + ".pdf";
    }
   else
      name = "newmodel.pdf";

   stream << name;
   QFileDialog openDlg(this);
   openDlg.setAcceptMode(QFileDialog::AcceptMode::AcceptSave);
   openDlg.setNameFilter(tr(".pdf (*.pdf )"));
   openDlg.selectFile(name);
   openDlg.setLabelText(QFileDialog::DialogLabel::FileType,tr("Enter .pdf file name"));
   openDlg.resize(QSize(900,500));
   if (!openDlg.exec())
      return;
   QStringList list = openDlg.selectedFiles();
   QString fName = list[0];
   if (fName.length())
   {
      QPrinter printer(QPrinter::HighResolution);
      printer.setOutputFormat(QPrinter::PdfFormat);
      printer.setOutputFileName(fName);
      printer.setPageSize(QPageSize(QPageSize::Letter));
      printer.setColorMode(QPrinter::Color);
      printer.setPageOrientation(QPageLayout::Landscape);
      QRectF rec = ui->simView->mapToScene(ui->simView->viewport()->rect()).boundingRect();
      QPainter painter;
      if (painter.begin(&printer))
      {
         scene->render(&painter,QRect(),rec);
         painter.end();

         msg.clear();
#if defined Q_OS_LINUX
         stream << "xdg-open " << "\"" << fName << "\"" << "&";
#elif defined Q_OS_WIN
         stream << "start " << "\"\"" << fName;
#endif
         const char *str = msg.toLocal8Bit().constData();
         system(str);
      }
      else
      {
         QString msg;
         QTextStream stream(&msg);
         stream << "Unable to create .pdf document." << Qt::endl;
         ui->chatterBox->appendPlainText(msg);
      }
   }
}

// Create the changelog filename. May include current .snd file basename.
// Linux wants quoted with 'like this' char, windows wants quoted in "like this"
QString SimWin::makeChgName(QString sndName)
{
   QString logName;
   QDir dir = QDir::current();
   if (sndName.length() > 0)
   {
      QFileInfo info(sndName);
      if (useSndForChangelog)
      {
         logName = info.absoluteDir().absolutePath() + "/"
                   + "ChangeLog_"
                   + info.completeBaseName()
                   + ".txt";
      }
      else
      {
         logName = info.absoluteDir().absolutePath() + "/" + "ChangeLog.txt";
      }
   }
   else
   {
      logName = dir.canonicalPath() + "/" + "ChangeLog.txt";
   }
   return logName;
}


void SimWin::doNewFile()
{
   if (chkForDrawing())
      return;
   if (needToSave())
      saveSnd();
   scene->clear();
   ui->paramTabs->clear();
   ui->simView->centerOn(0,0);
   ui->actionupdateChgLog->setEnabled(false);
   resetNew();
}

void SimWin::addFiber()
{
   if (ui->actionAdd_Fiber->isChecked())
   {
      addFiberToggle = true;
      addCellOff();
      addConnectorOff();
      scene->views().first()->viewport()->setCursor(Qt::CrossCursor);
      ui->simView->setDragMode(QGraphicsView::DragMode::NoDrag);
      scene->setFocus();
   }
   else
      addFiberOff();
}

void SimWin::addCell()
{
   if (ui->actionAddCell->isChecked())
   {
      addCellToggle = true;
      addFiberOff();
      addConnectorOff();
      scene->views().first()->viewport()->setCursor(Qt::CrossCursor);
      ui->simView->setDragMode(QGraphicsView::DragMode::NoDrag);
      scene->setFocus();
   }
   else
      addCellOff();
}

void SimWin::addConnector()
{
   if (ui->actionAddConnector->isChecked())
   {
      scene->synapseChkDirty();
      addCellOff();
      addFiberOff();
      scene->deSelAll();
      connectorMode = connMode::START;
      scene->views().first()->viewport()->setCursor(Qt::CrossCursor);
      ui->simView->setDragMode(QGraphicsView::DragMode::NoDrag);
      scene->setFocus();
   }
   else
   {
      addConnectorOff();
   }
}

void SimWin::deleteSelected()
{
   scene->deleteSelected();
   buildFindList();
}

void SimWin::doCellRetro()
{
   cRetroOn = !cRetroOn;
   cAnteroOn = false;
   scene->showCellAntero(false);
   scene->showCellRetro(cRetroOn);
}

void SimWin::doCellAntero()
{
   cAnteroOn = !cAnteroOn;
   fRetroOn = false;
   scene->showCellRetro(false);
   scene->showCellAntero(cAnteroOn);
}

void SimWin::doFiberRetro()
{
   fRetroOn = !fRetroOn;
   cRetroOn = cAnteroOn = false;
   scene->showCellRetro(false);
   scene->showCellAntero(false);
   scene->showFiberTarg(fRetroOn);
}

void SimWin::connLayerToggle()
{
   linesOnTop = !linesOnTop;
   scene->toggleLines(linesOnTop);
   if (linesOnTop)
      ui->lineLayer->setText("Lines In Back\nTo Edit Populations");
   else
      ui->lineLayer->setText("Lines on Top\nTo Edit Connectors");
}

void SimWin::doStdCell(bool on)
{
   if (on)
   {
      int d_idx = ui->cellObjId->text().toInt();
      scene->cellRecToUi(d_idx, CELL);
      scene->setCellDirty(true);
   }
}

void SimWin::doBurster(bool on)
{
   if (on)
   {
      int d_idx = ui->cellObjId->text().toInt();
      scene->cellRecToUi(d_idx, BURSTER_POP);
      scene->setCellDirty(true);
   }
}

void SimWin::doPsr(bool on)
{
   if (on)
   {
      int d_idx = ui->cellObjId->text().toInt();
      scene->cellRecToUi(d_idx,PSR_POP);
      scene->setCellDirty(true);
   }
}

void SimWin::doLungPhren(bool on)
{
   if (on)
   {
      ui->cellTypes->clear();
      int d_idx = ui->cellObjId->text().toInt();
      scene->cellRecToUi(d_idx,PHRENIC_POP);
      scene->setCellDirty(true);
   }
}

void SimWin::doLungLumb(bool on)
{
   if (on)
   {
      int d_idx = ui->cellObjId->text().toInt();
      scene->cellRecToUi(d_idx, LUMBAR_POP);
      scene->setCellDirty(true);
   }
}
void SimWin::doLungInspLar(bool on)
{
   if (on)
   {
      int d_idx = ui->cellObjId->text().toInt();
      scene->cellRecToUi(d_idx, INSP_LAR_POP);
      scene->setCellDirty(true);
   }
}

void SimWin::doLungExpiLar(bool on)
{
   if (on)
   {
      int d_idx = ui->cellObjId->text().toInt();
      scene->cellRecToUi(d_idx, EXP_LAR_POP);
      scene->setCellDirty(true);
   }
}

void SimWin::doFiberNormal(bool on)
{
   if (on)
   {
      int d_idx = ui->fiberObjId->text().toInt();
      scene->fiberRecToUi(d_idx, FIBER);
      scene->setFiberDirty(true);
   }
}

void SimWin::doFiberElectric(bool on)
{
   if (on)
   {
      int d_idx = ui->fiberObjId->text().toInt();
      scene->fiberRecToUi(d_idx, ELECTRIC_STIM);
      scene->setFiberDirty(true);
   }
}

void SimWin::doFiberAfferent(bool on)
{
   if (on)
   {
      int d_idx = ui->fiberObjId->text().toInt();
      scene->fiberRecToUi(d_idx, AFFERENT);
      scene->setFiberDirty(true);
   }
}

void SimWin::addAfferentRow()
{
   int num = afferentModel->numRecs();

   if (num < MAX_AFFERENT_PROB)
   {
      affRec rec = {0,0.0};
      afferentModel->addRow(rec, -1);
      ui->afferentTable->scrollToBottom();
      QModelIndex cell = afferentModel->index(num,0);
      ui->afferentTable->selectionModel()->select(cell,QItemSelectionModel::ClearAndSelect);
   }
}

void SimWin::delAfferentRow()
{
   QModelIndex to_del = ui->afferentTable->selectionModel()->currentIndex();
   if (to_del.row() != -1)
      afferentModel->removeRow(to_del.row());
}

void SimWin::affRecToModel(F_NODE* fiber)
{
   afferentModel->reset();
   affRec rec;
   for (int row = 0; row < fiber->num_aff; ++row)
   {
      rec.rec[AFF_VAL] = fiber->aff_val[row];
      rec.rec[AFF_PROB] = fiber->aff_prob[row];
      afferentModel->addRow(rec,-1);
   }
   afferentModel->sortRows();
}

void SimWin::affModelToRec(F_NODE* fiber)
{
   affRec rec;
   int num = afferentModel->numRecs();
   fiber->num_aff = 0;
    // clear out deleted rec vals
   memset(fiber->aff_val,0,sizeof(fiber->aff_val));
   memset(fiber->aff_prob,0,sizeof(fiber->aff_prob));
   afferentModel->sortRows();
   for (int row = 0; row < num; ++row)
   {
      if (afferentModel->readRec(rec,row))
      {
         fiber->aff_val[row] = rec.rec[AFF_VAL].toDouble();
         fiber->aff_prob[row] = rec.rec[AFF_PROB].toDouble();
         ++fiber->num_aff;
      }
   }
}


void SimWin::doGlobalBdt()
{
   ui->globStepTime->setText("0.5000");
   ui->globStepTime->setReadOnly(true);
   launch->bdtSel();
   globalsChanged();
}

void SimWin::doGlobalEdt()
{
   ui->globStepTime->setText("0.1000");
   ui->globStepTime->setReadOnly(true);
   launch->edtSel();
   globalsChanged();
}

void SimWin::doGlobalOther()
{
   ui->globStepTime->setReadOnly(false);
   launch->customSel();
   globalsChanged();
}

void SimWin::doOneShotAdd(bool checked)
{
   oneShotAddFlag=checked;
}

void SimWin::addFiberOff()
{
   ui->actionAdd_Fiber->setChecked(false);
   addFiberToggle = false;
   scene->views().first()->viewport()->unsetCursor();
   ui->simView->setDragMode(QGraphicsView::RubberBandDrag);
}

void SimWin::addCellOff()
{
   ui->actionAddCell->setChecked(false);
   addCellToggle = false;
   scene->views().first()->viewport()->unsetCursor();
   ui->simView->setDragMode(QGraphicsView::RubberBandDrag);
}

void SimWin::addConnectorOff()
{
   ui->actionAddConnector->setChecked(false);
   if (connectorMode != connMode::OFF)
   {
      scene->removePath();
      connectorMode = connMode::OFF;
      scene->views().first()->viewport()->unsetCursor();
      ui->simView->setDragMode(QGraphicsView::RubberBandDrag);
   }
}

void SimWin::showBuildSynapse()
{
   scene->fiberChkDirty();
   scene->cellChkDirty();
   scene->synapseChkDirty();
   scene->globalsChkDirty();
   scene->deSelAll();
   ui->paramTabs->clear();
   ui->paramTabs->addTab(ui->buildTab,"Edit Synapse Types");
}


void SimWin::showGlobals()
{
   scene->fiberChkDirty();
   scene->cellChkDirty();
   scene->synapseChkDirty();
   scene->buildChkDirty();
   scene->deSelAll();
   ui->paramTabs->clear();
   ui->paramTabs->addTab(ui->globalsTab,"Edit Globals");
}


// TODO: there is no machinery for telling the launch window that
// its lists are out of date. E.G., we could delete a populuation or
// change a cell type and it would not know. The newsned program
// has the same problem.
// To be fixed in later release.
void SimWin::fiberSendToLauncherBdt()
{
   if (launch)
      scene->fiberViewSendToLauncherBdt();
}

void SimWin::fiberSendToLauncherPlot()
{
   if (launch)
      scene->fiberViewSendToLauncherPlot();
}

void SimWin::cellSendToLauncherBdt()
{
   if (launch)
      scene->cellViewSendToLauncherBdt();
}

void SimWin::cellSendToLauncherPlot()
{
   if (launch)
      scene->cellViewSendToLauncherPlot();
}

void SimWin::doSelAll()
{
   scene->selAll();
}

// all cell page changes come here
void SimWin::cellChanged()
{
   scene->setCellDirty(true);
}

// all fiber page changes come here
void SimWin::fiberChanged()
{
   scene->setFiberDirty(true);
}

// all synapse page changes come here
void SimWin::synapseChanged()
{
   scene->setSynapseDirty(true);
}

void SimWin::buildChanged()
{
   scene->setBuildDirty(true);
}

void SimWin::globalsChanged()
{
   scene->setGlobalsDirty(true);
}

void SimWin::launchChg()
{
   scene->setFileDirty(true);
}

void SimWin::launchPopup()
{
   launch->show();
   launch->raise();
   showLaunch = true;
   scene->setSendControls();
   ui->actionRun_Simulation->setEnabled(false);
}

void SimWin::launchIsClosing()
{
   launch->setVisible(false);
   showLaunch = false;
   scene->setSendControls();
   ui->actionRun_Simulation->setEnabled(true);
}


void SimWin::fitToView()
{
   fitZoom = !fitZoom;
   if (fitZoom)
   {
      ui->fitToView->setText("Unzoom");
      ui->simView->scaleToFit();
   }
   else
   {
      ui->fitToView->setText("Fit To View");
      ui->simView->unZoom();
   }
}

void SimWin::openHelp()
{
   HelpBox *help = new HelpBox(this);
   help->exec();
}


void SimWin::doChgLogPrompt(bool flag)
{
   logPromptOff = flag;
}

void SimWin::doCreateGroup()
{
   scene->createGroup();
}

void SimWin::doImportGroup()
{
   scene->importGroup();
}

void SimWin::doExportGroup()
{
   scene->exportGroup();
}

void SimWin::doUnGroup()
{
   scene->unGroup();
}

void SimWin::doSelectGroup()
{
   scene->selGroup();
}

void SimWin::doFiberTypeFixed(bool /* checked*/)
{
   scene->fiberTypeFixed();
}

void SimWin::doFiberTypeFuzzy(bool /* checked*/)
{
   scene->fiberTypeFuzzy();
}

// If the file is in the current directory, strip the path. 
// The assumption is if the dir is copied, everything will be.
// If the file is in another directory, keep the path. 
// The assumption is the file is shared among multiple simulations.
void SimWin::selAfferentFile()
{
   QDir dir = QDir::current();

   QFileDialog openDlg(this,"Load An Afferent Source File");
   openDlg.setFileMode(QFileDialog::ExistingFile);
   openDlg.setViewMode(QFileDialog::Detail);
   openDlg.setNameFilter("Spike2 (*.smr *.smrx)");
   openDlg.setDirectory(dir);
   openDlg.setLabelText(QFileDialog::DialogLabel::FileType,tr("Select file to use."));
   openDlg.resize(QSize(900,500));
   if (!openDlg.exec())
      return;
   QStringList list = openDlg.selectedFiles();
   QString fName = list[0];

   if (fName.length())
   {
      
      QFileInfo info;
      info.setFile(fName);
      if (info.absolutePath() == dir.absolutePath())
         fName = info.fileName();
      if (ui->afferentFileName->text() != fName) // only dirty if changed
      {
         ui->afferentFileName->setText(fName);
         scene->setFiberDirty(true);
      }
   }
}



void SimWin::doLaunchOnTop(bool onoff)
{
   launchOnTop = onoff;
   if (launch)
   {
      launch->reParent(onoff);
      if (showLaunch)
         launch->show();
   }
}

// (Re)build the cell/fiber list in the find dlg.
void SimWin::buildFindList()
{
   pickList cellList, fiberList;
   int src;
   int popnum;

   for (src = FIRST_INODE; src <= LAST_INODE; ++src) // cells first
   {
      if (D.inode[src].node_type == CELL)
      {
         popnum = D.inode[src].node_number;
         QString str = QString("C %1 %2").arg(popnum).arg(D.inode[src].comment1);
         QListWidgetItem *item = new QListWidgetItem(str);
         item->setData(Qt::UserRole,popnum);
         cellList.emplace(popnum, item);
      }
   }
   for (src = FIRST_INODE; src <= LAST_INODE; ++src) // fibers last
   {
      if (D.inode[src].node_type == FIBER)
      {
         popnum = D.inode[src].node_number;
         QString str = QString("F %1 %2").arg(popnum).arg(D.inode[src].comment1);
         QListWidgetItem *item = new QListWidgetItem(str);
         item->setData(Qt::UserRole,popnum + MAX_INODES); // Both cells & fibers start at 1 
                                                          // This lets us tell them apart.
         fiberList.emplace(popnum,item);
      }
   }
   findDlg->initList(cellList, fiberList);
}

// Menu Find Handler. Update and show the find dialog.
void SimWin::doFindPopulation()
{
   buildFindList();
   findDlg->show();
}

// Signal handler from find dialog. Look up the node and center/select it in the display.
void SimWin::showFindItem(QListWidgetItem& item)
{
   int src, type, popnum;

   popnum = item.data(Qt::UserRole).toInt();
   if (popnum > MAX_INODES)
   {
      type = FIBER;
      popnum -= MAX_INODES;
   }
   else
      type = CELL;

   for (src = FIRST_INODE; src <= LAST_INODE; ++src)
      if (D.inode[src].node_type == type && D.inode[src].node_number == popnum)
         scene->centerOn(src);
}

void SimWin::doMonochrome(bool on_off)
{
   showMono = on_off;
   scene->doMonochrome(on_off);
}


