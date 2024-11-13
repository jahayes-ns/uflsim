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


// A non-modal child window of the simbuild parent, but it acts as if it were a
// separate app. Control the simulator engine from here.


#include <stdlib.h>
#include <QSettings>
#include "simwin.h"
#include "launchwindow.h"
#include "ui_launchwindow.h"
#include "c_globals.h"

using namespace std;

launchWindow::launchWindow(SimWin *parent) : QDialog(parent), ui(new Ui::launchWindow)
{
   ui->setupUi(this);
   mainwin = parent;
   if (!parent->launchOnTop)
      setParent(nullptr);
   mainwin->printMsg("Creating simlaunch window");
   ui->launchPlotView->setStyle(new dropStyle(style()));
   ui->launchBdtList->setStyle(new dropStyle(style()));

    // for now, hide these, the host stuff is incomplete and
    // is more a prototyping thing.
   ui->launcherHostName->setVisible(false);
   ui->hostNameLabel->setVisible(false);
         // make the models
   plotDeskModel = new plotModel(this);
   connect(plotDeskModel,&plotModel::notifyChg,this,&launchWindow::setScrDirty);
   bdtListModel = new bdtModel(this);
   connect(bdtListModel,&bdtModel::notifyChg,this,&launchWindow::setScrDirty);
   analogModel = new analogWidgetModel(this);
   connect(analogModel,&analogWidgetModel::notifyChg,this,&launchWindow::setScrDirty);
   hostNameModel = new hostNameWidgetModel(this);
   connect(hostNameModel,&hostNameWidgetModel::notifyChg,this,&launchWindow::setScrDirty);

     // create mapper to associate analog data model with discrete controls
   analogMapper = new QDataWidgetMapper(this);
   analogMapper->setModel(analogModel);
   analogMapper->addMapping(ui->analogId,ANALOG_ID);
   analogMapper->addMapping(ui->analogCellPop,ANALOG_CELLPOP);
   analogMapper->addMapping(ui->analogRate,ANALOG_RATE);
   analogMapper->addMapping(ui->analogIntTime,ANALOG_TIME);
   analogMapper->addMapping(ui->analogScale,ANALOG_SCALE);
   analogMapper->addMapping(ui->period1Code,ANALOG_P1CODE);
   analogMapper->addMapping(ui->period1Start,ANALOG_P1START);
   analogMapper->addMapping(ui->period1End,ANALOG_P1STOP);
   analogMapper->addMapping(ui->period2Code,ANALOG_P2CODE);
   analogMapper->addMapping(ui->period2Start,ANALOG_P2START);
   analogMapper->addMapping(ui->period2End,ANALOG_P2STOP);
   analogMapper->addMapping(ui->ie_smooth,ANALOG_IE_SMOOTH);
   analogMapper->addMapping(ui->ie_sample,ANALOG_IE_SAMPLE);
   analogMapper->addMapping(ui->ie_plus,ANALOG_IE_PLUS);
   analogMapper->addMapping(ui->ie_minus,ANALOG_IE_MINUS);
   analogMapper->addMapping(ui->ie_freq,ANALOG_IE_FREQ);

     // create mapper to associate host name data model with discrete control
   hostNameMapper = new QDataWidgetMapper(this);
   hostNameMapper->setModel(hostNameModel);
   hostNameMapper->addMapping(ui->launcherHostName,HOSTNAME_ID);

   currModel = 0; // current launch index

   plotDeskModel->setCurrentModel(currModel);
   bdtListModel->setCurrentModel(currModel);
   analogModel->setCurrentModel(currModel);
   analogMapper->setSubmitPolicy(QDataWidgetMapper::AutoSubmit);
     // our custom ctrls
   plotDeskCombo = new plotCombo(this);
   plotDeskBin = new plotSpin(this);
   plotDeskScale = new plotSpin(this);
   plotLineEdit = new plotEdit(this);
   ui->launchNum->setMaximum(MAX_LAUNCH-1);
   ui->launchBdtList->setModel(bdtListModel);
   ui->launchBdtList->setItemDelegateForColumn(BDT_MEMB,plotLineEdit);
   ui->launchBdtList->setAcceptDrops(true);
       // set up the view tables
   ui->launchPlotView->setModel(plotDeskModel);
   ui->launchPlotView->setItemDelegateForColumn(PLOT_TYPE ,plotDeskCombo);
   ui->launchPlotView->setItemDelegateForColumn(PLOT_BINWID, plotDeskBin);
   ui->launchPlotView->setItemDelegateForColumn(PLOT_SCALE, plotDeskScale);
   ui->launchPlotView->setItemDelegateForColumn(PLOT_MEMB, plotLineEdit);
   ui->launchPlotView->setAcceptDrops(true);
   loadSettings();

   analogMapper->toFirst();

   connect(ui->launchPlotView->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &launchWindow::newRow);
   ui->launchPlotView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
   ui->launchBdtList->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
   mainwin->printMsg("simlaunch window creation completed.");
}

launchWindow::~launchWindow()
{
   delete ui;
}

void launchWindow::loadSettings()
{
   int num, head, val;
   QString name;
   QSettings settings("simulator","simbuild");
   if (settings.status() == QSettings::NoError && settings.contains("launchgeometry"))
   {
      setGeometry(settings.value("launchgeometry").toRect());
      num = ui->launchPlotView->horizontalHeader()->count();
      for (head = 0; head < num; ++head)
      {
         name = QString("launchcol%1").arg(head);
         val = settings.value(name).toInt();
         if (val == 0)  // ensure visible
            val = 80;
         ui->launchPlotView->horizontalHeader()->resizeSection(head,val);
      }
      num = ui->launchBdtList->horizontalHeader()->count();
      for (head = 0; head < num; ++head)
      {
         name = QString("bdtcol%1").arg(head);
         val = settings.value(name).toInt();
         if (val == 0)
            val = 80;
         ui->launchBdtList->horizontalHeader()->resizeSection(head,val);
      }
      val = settings.value("selbdt",true).toBool();
      ui->selLaunchBdt->setChecked(val);
      val = settings.value("selsmr",false).toBool();
      ui->selLaunchSmr->setChecked(val);
      val = settings.value("selanalog",true).toBool();
      ui->selLaunchAnalog->setChecked(val);
      val = settings.value("selsocket",true).toBool();
      ui->selLaunchPlotSocket->setChecked(val);
      val = settings.value("selfile",false).toBool();
      ui->selLaunchPlot->setChecked(val);
      val = settings.value("selnone",false).toBool();
      ui->selLaunchNoPlot->setChecked(val);
      val = settings.value("selcondi",false).toBool();
      ui->condiCheck->setChecked(val);
      val = settings.value("selblung",false).toBool();
      ui->lungCheck->setChecked(val);
      val = settings.value("selwave",false).toBool();
      ui->selLaunchSmrWave->setChecked(val);
   }
   else
      setGeometry(100,100,1000,800);
}

void launchWindow::saveSettings()
{
   QSettings settings("simulator","simbuild");
   QString name;
   int num, head, val;
   if (settings.status() == QSettings::NoError)
   {
      settings.setValue("launchgeometry",geometry());
      num = ui->launchPlotView->horizontalHeader()->count();
      for (head = 0; head < num; ++head)
      {
         val = ui->launchPlotView->horizontalHeader()->sectionSize(head);
         name = QString("launchcol%1").arg(head);
         settings.setValue(name,val);
      }
      num = ui->launchBdtList->horizontalHeader()->count();
      for (head = 0; head < num; ++head)
      {
         val = ui->launchBdtList->horizontalHeader()->sectionSize(head);
         name = QString("bdtcol%1").arg(head);
         settings.setValue(name,val);
      }
      settings.setValue("selbdt", ui->selLaunchBdt->isChecked());
      settings.setValue("selsmr", ui->selLaunchSmr->isChecked());
      settings.setValue("selanalog", ui->selLaunchAnalog->isChecked());
      settings.setValue("selsocket", ui->selLaunchPlotSocket->isChecked());
      settings.setValue("selfile", ui->selLaunchPlot->isChecked());
      settings.setValue("selnone", ui->selLaunchNoPlot->isChecked());
      settings.setValue("selcondi", ui->condiCheck->isChecked());
      settings.setValue("selblung", ui->lungCheck->isChecked());
      settings.setValue("selwave", ui->selLaunchSmrWave->isChecked());
   }
}


void launchWindow::fileLoaded()
{
   plotDeskModel->blockSignals(true);
   bdtListModel->blockSignals(true);
   analogModel->blockSignals(true);
   ui->selLaunchBdt->blockSignals(true);
   loadModels();  // any file data in globals to show?
   plotDeskModel->blockSignals(false);
   bdtListModel->blockSignals(false);
   analogModel->blockSignals(false);
   ui->selLaunchBdt->blockSignals(false);
}

void launchWindow::saveSnd()
{
   modelsToVars();  // make sure changes are in global vars
}

// New button
void launchWindow::resetModels()
{
   if(plotDeskModel)
      plotDeskModel->reset();
   if (bdtListModel)
      bdtListModel->reset();
   if (analogModel)
      initAnalogRecs();
   if (hostNameModel)
      initHostNameRecs();
}

// We may update header text depending on pop type in current row.
void launchWindow::newRow(const QModelIndex &curr, const QModelIndex & /*prev*/)
{
   plotDeskModel->updHeader(curr);
}

void launchWindow::on_launchCancel_clicked()
{
   launchExit(); // tell parent we are closing
}

void launchWindow::on_launchDelSelected_clicked()
{
   QModelIndex to_del = ui->launchPlotView->selectionModel()->currentIndex();
   if (to_del.row() != -1)
   {
      plotDeskModel->delRow(to_del.row());
   }
}

void launchWindow::on_launchButton_clicked()
{
   launchSim();
}


// This draws a custom drop target filled box, rather than just a line,
// which is rather hard to see.
dropStyle::dropStyle(QStyle* style) :QProxyStyle(style)
{
}

void dropStyle::drawPrimitive (PrimitiveElement element, const QStyleOption * option, QPainter * painter, const QWidget * widget) const
{
    if (element == QStyle::PE_IndicatorItemViewItemDrop && !option->rect.isNull())
    {
        QStyleOption opt(*option);
        opt.rect.setLeft(0);
        if (widget)
        {
           opt.rect.setRight(widget->width());
           opt.rect.setHeight(4);
        }
        painter->setBrush(QBrush(Qt::green));
        painter->setPen(QPen(Qt::green));
        painter->drawRect(opt.rect);
        painter->fillRect(opt.rect,QBrush(Qt::green));
        return;
    }
    QProxyStyle::drawPrimitive(element, option, painter, widget);
}


void launchWindow::on_bdtDelSelected_clicked()
{
   QModelIndex to_del = ui->launchBdtList->selectionModel()->currentIndex();
   if (to_del.row() != -1)
   {
      bdtListModel->delRow(to_del.row());
   }
}


void launchWindow::on_launchNum_valueChanged(int arg1)
{
   launchNumChanged(arg1);
}

void launchWindow::on_launchCopyToNextButton_clicked()
{
   copyModels();
}

void launchWindow::on_launchKill_clicked()
{
   killProgs();
}

void launchWindow::on_pauseResume_clicked()
{
   launchPause();
}

void launchWindow::on_launchAddLung_clicked()
{
   addLungRow();
}

void launchWindow::on_midRunUpdateButton_clicked()
{
    midRunUpdate();
}
