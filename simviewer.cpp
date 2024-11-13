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


// This plots output from sim2 from previous runs or in real time during a sim run

#include <iostream>
using namespace std;
#include "simviewer.h"
#include "ui_simviewer.h"

SimViewer::SimViewer(QWidget *parent, int l, int p, bool s, bool r, QString name) :
   QMainWindow(parent),
   ui(new Ui::SimViewer),launchNum(l),guiPort(p), useSocket(s), useFiles(r), hostName(name)
{
   ui->setupUi(this);
   QString title("Simviewer Version: ");
   title = title.append(VERSION);
   title = title.append(" Launch Instance ");
   title = title.append(QString::number(launchNum));
   setWindowTitle(title);
   if (hostName.length() == 0)
      hostName = "localhost";
   setupViewer();
   setWindowIcon(QIcon(":/simviewert.png"));
}

SimViewer::~SimViewer()
{
   tickToc->stop(); // stop timer
   delete ui;
   if (guiSocket)
      delete guiSocket;
   if (tickToc)
      delete tickToc;
   if (runSocket)
      delete runSocket;
}

void SimViewer::on_spacingSlider_valueChanged(int value)
{
   doSpacing(value);
}

void SimViewer::on_vgainSlider_valueChanged(int value)
{
   doVGain(value);
}

void SimViewer::on_timemarkerSlider_valueChanged(int value)
{
    doTime(value);
}


void SimViewer::on_voltmarkerSlider_valueChanged(int value)
{
   doVolt(value);
}


void SimViewer::on_invertBW_stateChanged(int arg1)
{
   doInvert(arg1);
}

void SimViewer::closeEvent(QCloseEvent *evt)
{
   on_actionQuit_triggered();
   evt->accept();
}

void SimViewer::on_actionQuit_triggered()
{
   saveSettings();
   close();
}

void SimViewer::on_apVisible_stateChanged(int arg1)
{
   doApVisible(arg1);
}

void SimViewer::on_colorOn_stateChanged(int arg1)
{
    doColorOn(arg1);
}

void SimViewer::on_showText_stateChanged(int arg1)
{
   doShowText(arg1);
}

void SimViewer::on_actionactionPDF_triggered()
{
   CreatePDF();
}

void SimViewer::on_actionFit_triggered()
{
   toggleFit();
}

void SimViewer::on_launchViewer_stateChanged(int arg1)
{
   launchViewerState(arg1);
}


void SimViewer::on_winSplitter_splitterMoved(int /*pos*/, int /*index*/)
{
   updateLineText();
}

void SimViewer::on_actionDebug_show_row_boxes_toggled(bool arg1)
{
  doShowBoxes(arg1);
}


void SimViewer::on_autoScale_stateChanged(int arg1)
{
   doAutoScaleChg(arg1);
}

void SimViewer::on_tbaseSlider_actionTriggered(int action)
{
   doTBase(action);
}

void SimViewer::on_actionChange_Directory_triggered()
{
   doChDir();
}

void SimViewer::on_actionRestart_triggered()
{
  doRestart();
}

void SimViewer::on_autoAntiClip_stateChanged(int arg1)
{
   doClipChanged(arg1);
}
