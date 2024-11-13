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


// Implementation of simviewer

#include <iostream>
#include <typeinfo>
#include <ctime>
#include <QPen>
#include <QBrush>
#include <QScrollBar>
#include <QLine>
#include <QList>
#include <QGraphicsLineItem>
#include <QTextStream>
#include <QGraphicsSimpleTextItem>
#include <QtPrintSupport/QPrinter>
#include <QFileDialog>
#include <QPrinterInfo>
#include <QPrintDialog>
#include <QtGlobal>
#include <QSettings>
#include <QHostInfo>
#include <QPageSize>
#include <QPageLayout>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <math.h>
#include <limits.h>
#include <dirent.h>

#include "simviewer.h"
#include "ui_simviewer.h"
#include "lin2ms.h"
#include "c_globals.h"
#include "wavemarkers.h"
#include "launch_model.h"

QColor SimViewer::sceneFG=Qt::white;
QColor SimViewer::sceneBG=Qt::black;

using namespace std;

// This is called from the ctor
// Don't assume the window is open for business yet.
void SimViewer::setupViewer()
{
   QString msg1;
   QTextStream stream(&msg1);
   stream << "Launch Instance is " << launchNum << Qt::endl;
   ui->infoBox->appendPlainText(msg1);

   plot = new SimVScene(this);
   ui->plotView->centerOn(0,0);
   ui->plotView->setScene(plot);
   ui->plotView->setMouseTracking(true);
   plot->setBackgroundBrush(QBrush(sceneBG));

   itemFont = QFont("Arial",11);
   fontInfo = QFontMetrics(itemFont);

   ui->plotView->horizontalScrollBar()->setSliderPosition(0);
   ui->plotView->verticalScrollBar()->setSliderPosition(0);
   ui->plotView->setDragMode(QGraphicsView::NoDrag);

    // the synapse_gc array holds color and line props.
    //   the first 6 have specific colors values.
   synapse_gc[0]=QColor("lime");
   synapse_gc[1]=Qt::red;
   synapse_gc[2]=Qt::blue;
   synapse_gc[3]=Qt::yellow;
   synapse_gc[4]=Qt::magenta;
   synapse_gc[5]=Qt::cyan;
   // the rest of the colors set by a formuala
   for (int gcl=6; gcl<DRAW_COLORS; gcl++)
      synapse_gc[gcl] = gcl*7+23;

      // we'll need to catch scrollbar events
   connect(ui->plotView->horizontalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(hScroll(int)));
   connect(ui->plotView->verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(vScroll(int)));

     // timer to check for new files or info via socket
   tickToc = new QTimer(this);
   tickToc->setTimerType(Qt::PreciseTimer);
   tickToc->setSingleShot(true);
   connect(tickToc,SIGNAL(timeout()),this,SLOT(timerFired()));
   QString msg2 = "simbuild connection port: ";
   msg2 += QString::number(guiPort);
   ui->infoBox->appendPlainText(msg2);
   // simbuild <-> simviewer always on local host.
   QString ip = QHostAddress(QHostAddress::LocalHost).toString();
   if (guiPort)
   {
      guiSocket = new QTcpSocket(this);
      connect(guiSocket, &QTcpSocket::connected, this, &SimViewer::buildConnectOk);
      connect(guiSocket, &QTcpSocket::disconnected, this, &SimViewer::buildConnectionClosedByServer);
      connect(guiSocket, &QTcpSocket::readyRead, this, &SimViewer::fromSimBuild);
//      connect(guiSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error), this, &SimViewer::buildSocketError);
      connect(guiSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred), this, &SimViewer::buildSocketError);
      QString msg3 = "Trying to connect to simbuild on ";
      msg3 += ip.toLatin1().data();
      msg3 += " ";
      msg3 += QString::number(guiPort);
      ui->infoBox->appendPlainText(msg3);
      guiSocket->connectToHost(ip,guiPort);
   }

   if (guiPort)
   {
    // simrun will connect to this to send simviewer sim results
      viewerServer = new QTcpServer();
      QHostInfo hinfo = QHostInfo::fromName(hostName);
      cout << "SIMVIEWER: Using hostname " << hostName.toLatin1().data() << endl;
      QHostAddress ip;
      if (!hinfo.addresses().isEmpty())
         ip = hinfo.addresses().first();
      else
         ip = QHostAddress::LocalHost;

      viewerServer->listen(QHostAddress(QHostAddress::Any),0);
      cout << "VIEWER: listen for connections on IP: "
           << ip.toString().toLatin1().data()
           << " port: "
           << viewerServer->serverPort() << endl;
      connect(viewerServer,&QTcpServer::newConnection,this,&SimViewer::runConnRqst);
      connect(viewerServer,&QTcpServer::newConnection,this,&SimViewer::runConnectOk);
      ui->actionChange_Directory->setEnabled(false);  // no file stuff if we are
      ui->actionRestart->setEnabled(false);           // using sockets.
   }
   lineParent = new QGraphicsItemLayer;
   plot->addItem(lineParent);
   lineParent->setZValue(4);
   voltLineParent = new QGraphicsItemLayer;
   voltLineParent->setZValue(3);
   plot->addItem(voltLineParent);
   timeLineParent = new QGraphicsItemLayer;
   timeLineParent->setZValue(2);
   plot->addItem(timeLineParent);
   textParent = new QGraphicsItemLayer;
   textParent->setZValue(10);
   plot->addItem(textParent);
   loadSettings();
     // I always use this style for Quit clickage
   ui->mainToolBar->widgetForAction(ui->actionQuit)->setStyleSheet("QWidget {color: darkred; font-style:bold;}");

   if (!guiPort)
   {
      QLabel *label = new QLabel("Launch Number");
      currLaunch = new QSpinBox(this);
      currLaunch->setMinimum(0);
      currLaunch->setRange(0,20);
      currLaunch->setAlignment(Qt::AlignHCenter);
      currLaunch->setValue(launchNum);
      ui->mainToolBar->insertWidget(ui->actionactionPDF,label);
      ui->mainToolBar->insertWidget(ui->actionactionPDF,currLaunch);
      ui->mainToolBar->insertSeparator(ui->actionQuit);
      ui->infoBox->appendPlainText("Checking for and loading existing wave files. . .\n");
   }
   tickToc->start(checkTime);
}

void SimViewer::runConnRqst()
{
   QString msg1 = "Simviewer got a connection request from simrun.";
   ui->infoBox->appendPlainText(msg1);
   runSocket = viewerServer->nextPendingConnection();
   if (!runSocket)
   {
      QString msg2 ="Launcher got unexpected connection request without a connection.";
      ui->infoBox->appendPlainText(msg2);
   }
   connect(runSocket, &QTcpSocket::disconnected, this, &SimViewer::runConnectionClosed);
   connect(runSocket, &QTcpSocket::readyRead, this, &SimViewer::fromSimRun);
   connect(runSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred), this, &SimViewer::runSocketError);
}


double SimViewer::timeValFromPos(int pos)
{
   double t = (pos / hScale) / 1000.0 * stepSize;
   return t;
}

void SimViewer::buildConnectOk()
{
   QString msg1 = "Simviewer connected with simbuild.";
   ui->infoBox->appendPlainText(msg1);
    // tell simbuild the port # so it can tell simrun
   char portnum[64];
   sprintf(portnum,"%d",viewerServer->serverPort());
   auto sval = guiSocket->write(portnum);
   guiSocket->flush();
   QString msg2 = "Sending port # ";
   msg2 += portnum;
   msg2 += " to simbuild, sent ";
   msg2 += QString::number(sval);
   msg2 += " bytes.";
   ui->infoBox->appendPlainText(msg2);
}

void SimViewer::buildConnectionClosedByServer()
{
   QString msg = "Simviewer disconnected from simbuild.";
   ui->infoBox->appendPlainText(msg);
}

void SimViewer::fromSimBuild()
{
//   cout << "Simviewer: msg from simbuild" << endl;
   QByteArray msg = guiSocket->readAll();
//   if (msg.size())
//      cout << "got [" << msg[0] << "]" << endl;
   if (msg[0] == 'T')
   {
      cout << "simviewer closing" << endl;
      guiSocket->close();
      close();
   }
}

void SimViewer::runConnectOk()
{
   QString msg = "Simviewer connected to simrun.";
   ui->infoBox->appendPlainText(msg);
}

void SimViewer::runConnectionClosed()
{
   QString msg = "Simviewer disconnected from simrun.";
   ui->infoBox->appendPlainText(msg);
   QString msg2;
   QTextStream stream(&msg2);
   ui->infoBox->appendPlainText(msg2);
}


/* This shows up as a stream of bytes. It can be incomplete. The start
   of block is marked with the "impossible" char value of MSG_START,
   the end is MSG_END. Accumulate the bytes until we have a complete
   block.  Then process it as a set of strings
   A block looks like this:
   MSG_START
   launch number
   wave number
   data
   MSG_END
*/
void SimViewer::fromSimRun()
{
   static bool have_start = false;
   QByteArray msg;
   static stringstream strm;
   char val;
   int ret;
   int read = 0;

   while ((ret = runSocket->read(&val,sizeof(val))) != 0)
   {
      ++read;
      if (ret == -1)
      {
         cout << "error reading from runSocket" << endl;
         return;
      }
      if ((unsigned char) val == MSG_EOF)
      {
         char eof_msg[2];
         eof_msg[0] = MSG_EOF;
         eof_msg[1] = 0;
         runSocket->write(eof_msg);
         return;
      }
      if ((unsigned char) val == MSG_START)
      {
         if (have_start)
            cout << " two starts without an end????" << endl;
         have_start = true;
         continue;
      }
      if (have_start)
      {
         if ( (unsigned char) val != MSG_END)
            strm.put(val);
         else
         {
            createWave(strm);
            strm.clear();
            strm.str(string());
            have_start = false;
            read = 0;
         }
      }
   }
}

// We have a complete wave block. Add it to our wave list.
void SimViewer::createWave(stringstream& strm)
{
   int waveNum = 0;
   string line;

   strm.clear();
   strm.seekg(0);
   getline(strm,line); // skip launch num, don't need it
   getline(strm,line);
   waveNum = stoi(line);
   pair <waveMapIter, bool> newItem;
   newItem = waveData.emplace(make_pair(waveNum,oneWave()));
   waveMapIter iter = newItem.first;
   oneWave& it = iter->second;
   char val;
   while (strm.get(val))
      it << val;

   tickToc->start(0); // this causes the timer to fire
}

void SimViewer::buildSocketError(QAbstractSocket::SocketError err)
{
   if (err != 1)
   {
      QString msg = "Simviewer: simbuild socket error " + guiSocket->errorString();
      ui->infoBox->appendPlainText(msg);
   }
}

void SimViewer::runSocketError(QAbstractSocket::SocketError err)
{
   if (err != 1)
   {
      QString msg = "Simviewer: simrun socket error " + guiSocket->errorString();
      ui->infoBox->appendPlainText(msg);
   }
}

void SimViewer::loadSettings()
{
   bool onoff;
   int pos;

   QSettings settings("simulator","viewer");
   if (settings.status() == QSettings::NoError  && settings.contains("geometry")) // does it exist at all?
   {
        // sometimes events are fired as we set values unless the current value
        // is the same as the one from the settings. We still need the action
        // of the signal handlers because sometimes there are side-effects. So,
        // make sure they all get called.
      this->blockSignals(true);
      restoreGeometry(settings.value("geometry").toByteArray());
      restoreState(settings.value("windowState").toByteArray());
      pos = settings.value("spacingSlider").toInt();
      ui->spacingSlider->setSliderPosition(pos);
      doSpacing(pos);
      pos = settings.value("vgainSlider").toInt();
      ui->vgainSlider->setSliderPosition(pos);
      doVGain(pos);
      pos = settings.value("timemarkerSlider").toInt();
      ui->timemarkerSlider->setSliderPosition(pos);
      doTime(pos);
      pos = settings.value("voltmarkerSlider").toInt();
      ui->voltmarkerSlider->setSliderPosition(pos);
      doVolt(pos);
      onoff = settings.value("apVisible").toBool();
      ui->apVisible->setChecked(onoff);
      doApVisible(onoff);
      onoff = settings.value("colorOn").toBool();
      ui->colorOn->setChecked(onoff);
      doColorOn(onoff);
      onoff = settings.value("invertBW").toBool();
      ui->invertBW->setChecked(onoff);
      doInvert(onoff);
      onoff = settings.value("showText").toBool();
      ui->showText->setChecked(onoff);
      doShowText(onoff);
      pos = settings.value("autoScale").toBool();
      ui->autoScale->setChecked(pos);
      doAutoScaleChg(pos);
      pos = settings.value("autoAntiClip",false).toBool();
      ui->autoAntiClip->setChecked(pos);
      doClipChanged(pos);
      onoff = settings.value("launchViewer").toBool();
      ui->launchViewer->setChecked(onoff);
      launchViewerState(onoff);
      onoff = settings.value("Debug").toBool();
      ui->actionDebug_show_row_boxes->setChecked(onoff);
      doShowBoxes(onoff);
      pos = settings.value("tbaseSlider").toInt();
      ui->tbaseSlider->setMaximum(pos);
      ui->tbaseSlider->setSliderPosition(pos);
      doTBase(0);
      this->blockSignals(false);
   }
}

void SimViewer::saveSettings()
{
   QSettings settings("simulator","viewer");
   if (settings.status() == QSettings::NoError)
   {
      settings.setValue("geometry",saveGeometry());
      settings.setValue("windowState",saveState());
      settings.setValue("tbaseSlider",ui->tbaseSlider->sliderPosition());
      settings.setValue("spacingSlider",ui->spacingSlider->sliderPosition());
      settings.setValue("vgainSlider",ui->vgainSlider->sliderPosition());
      settings.setValue("timemarkerSlider",ui->timemarkerSlider->sliderPosition());
      settings.setValue("voltmarkerSlider",ui->voltmarkerSlider->sliderPosition());
      settings.setValue("apVisible",ui->apVisible->isChecked());
      settings.setValue("colorOn",ui->colorOn->isChecked());
      settings.setValue("invertBW",ui->invertBW->isChecked());
      settings.setValue("showText",ui->showText->isChecked());
      settings.setValue("launchViewer",ui->launchViewer->isChecked());
      settings.setValue("autoScale",ui->autoScale->isChecked());
      settings.setValue("autoAntiClip",ui->autoAntiClip->isChecked());
      settings.setValue("Debug",ui->actionDebug_show_row_boxes->isChecked());
   }
}

// Timer handler
// Are we reading files or reading a socket?
void SimViewer::timerFired()
{
   size_t maxname;
#if defined Q_OS_LINUX
   maxname = NAME_MAX;
#elif defined Q_OS_WIN
   maxname = _MAX_FNAME;
#endif
   char nextname[maxname];
   stringstream waveStrm;

   if (useSocket)  // if there are wave blocks in our list, add them to plot.
   {
      waveMapIter curr;
      while ((curr = waveData.find(nextWave)) != waveData.end())
      {
         currentWave = nextWave;
         nextWave++;
         loadWave(curr->second);
         waveStrm.clear();
         waveStrm.str(string());
         waveData.erase(curr);
      }
      updatePaint();
   }
   else if (useFiles)
   {
      int feedback = 0;
      sprintf(nextname,"wave.%02d.%04d",launchNum, nextWave);
      if (access (nextname, R_OK) == 0)
      {
         while (access(nextname, R_OK) == 0)
         {
            ifstream fStream(nextname);
            if (!fStream)
               cerr << "***********Error opening " << nextname << endl;
            if (!fStream.good())
            {
               cerr << "***********Problem with " << nextname << endl;
               cerr << "bad bit " << fStream.bad() << "eof bit " << fStream.eof()
                    << "fail bit: " << fStream.fail() << endl;
               fStream.close();
               return;
            }

            waveStrm << fStream.rdbuf();
            currentWave = nextWave;
            nextWave++;
            loadWave(waveStrm);
            waveStrm.clear();
            waveStrm.str(string());
            fStream.close();
            sprintf(nextname,"wave.%02d.%04d",launchNum, nextWave);
            ++feedback;
            if (feedback % 500 == 0) // if we're in a dir with a previous run,
            {                        // can take a while to read files, assuage user
               ui->infoBox->appendPlainText("Loading...");
               ui->infoBox->repaint();
               updatePaint();
               QCoreApplication::processEvents();
            }
         }
         updatePaint();
      }
   }
   tickToc->start(checkTime); // restart timer
}

void SimViewer::hScroll(int /* change */)
{
   updateLineText();
}

void SimViewer::vScroll(int /* change */)
{
   updateLineText();
}

void SimViewer::resizeEvent(QResizeEvent *event)
{
   updateLineText();
   QMainWindow::resizeEvent(event);
}

void SimViewer::keyPressEvent(QKeyEvent *evt)
{
   switch(evt->key())
   {
      case Qt::Key_L:
         show_labels = !show_labels;
         doShowText(show_labels);
         plot->invalidate();
         break;

      default:
         QMainWindow::keyPressEvent(evt);
         break;
   }
}

void SimViewer::doShowText(int value)
{
   show_labels = value;
   if (show_labels)
      textParent->show();
   else
      textParent->hide();
}

// This fires before the value is actually changed.  We single step 1-20, then
// step in larger steps for larger values.  We need the precision when zooming
// in, but it is very tedious when zooming out.
void SimViewer::doTBase(int /*action*/)
{
   QString text;
   int rem;
   int  max = ui->tbaseSlider->maximum() - 10;
   int value = ui->tbaseSlider->sliderPosition();
   int curr = ui->tbaseSlider->value();
   int sign = value-curr >= 0 ? 1 : -1;
   if (value > 20 && value < max)
   {
      rem = value % 10;
      value = value + (sign*10) - rem;
   }

   ui->tbaseSlider->setSliderPosition(value);
   text = QString("%1").arg(value);
   ui->tbaseText->setText(text);
   int pos = ui->plotView->horizontalScrollBar()->sliderPosition();
   double currt0 = timeValFromPos(pos);
   hScale = 1.0/(value*.01);
   updateBounds();
   updatePaint();
   double currt1 = timeValFromPos(pos);
     // scroll to the previous visible time value
   ui->plotView->horizontalScrollBar()->setSliderPosition(pos * (currt0/currt1));
}

void SimViewer::doSpacing(int value)
{
   QString text;
   text = QString("%1").arg(value);
   ui->spaceText->setText(text);
   cellHeight = value;
   updateBounds();
   ui->voltmarkerSlider->setMaximum(value);
   updatePaint();
}


void SimViewer::doVGain(int value)
{
   QString text;
   double val = value / 5.0;
   text = QString("%1").arg(val,2,'f',1);
   ui->vgainText->setText(text);
   vScale= val;
   updatePaint();
}


void SimViewer::doTime(int value)
{
   QString text;
   if (value > 0)
      text = QString("%1 ms").arg(value);
   ui->timemarkerText->setText(text);
   tMark = value;
   updateTimeLines();
}


void SimViewer::doVolt(int value)
{
   QString text;
   if (value >= 0)
      text = QString("%1").arg(value);
   ui->voltmarkerText->setText(text);
   voltMark = value;
   if (value < 0)
   {
      voltLineParent->hide();
      show_voltage_markers = false;
   }
   else
   {
      voltLineParent->show();
      show_voltage_markers = true;
   }
   updateVoltLines();
   updateLineText();
}


void SimViewer::doInvert(int state)
{
   if (state)
   {
      sceneFG = Qt::black;
      sceneBG = Qt::white;
   }
   else
   {
      sceneFG = Qt::white;
      sceneBG = Qt::black;
   }
   plot->setBackgroundBrush(QBrush(sceneBG));

   plot->invalidate(plot->sceneRect(),QGraphicsScene::ItemLayer);
   updateVoltLines();
   updateTimeLines();
   updateLineText();
   updateCellRows();
}

// pdf saver, optionally launch a viewer
void SimViewer::CreatePDF()
{
   QString msg;
   QTextStream stream(&msg);
   stream << "simviewer." << launchNum << ".pdf";

   QFileDialog openDlg(this,"Save pdf file");
   openDlg.setAcceptMode(QFileDialog::AcceptMode::AcceptSave);
   openDlg.setNameFilter(tr(".pdf (*.pdf )"));
   openDlg.setDirectory(QDir::current());
   openDlg.selectFile(msg);
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
      QRectF rec = ui->plotView->mapToScene(ui->plotView->viewport()->rect()).boundingRect();
      QPainter painter;
      if (painter.begin(&printer))
      {
//         plot->render(&painter,QRect(),rec,Qt::IgnoreAspectRatio);
         plot->render(&painter,QRect(),rec);
         painter.end();

         if (launchPdfViewer)
         {
            msg.clear();
#if defined Q_OS_LINUX
            stream << "xdg-open " << "\"" << fName << "\"" << "&";
#elif defined Q_OS_WIN
            stream << "start " << "\"\" " << "\"" << fName << "\"";
            cout << "cmd is " << msg.toLatin1().data() << endl;
#endif
            const char *str = msg.toLocal8Bit().constData();
            if (system(str) == -1)
               ui->infoBox->appendPlainText("Problem launching PDF viewer");
         }
      }
      else
      {
         QString msg;
         QTextStream stream(&msg);
         stream << "Unable to create .pdf document." << Qt::endl;
         ui->infoBox->appendPlainText(msg);
      }
   }
}

void SimViewer::launchViewerState(bool state)
{
   launchPdfViewer=state;
}


void SimViewer::toggleFit()
{
   if (!cellRows.size()) // nothing to zoom
      return;
   if (!zoomed)
   {
      QRect vp = ui->plotView->viewport()->rect();
      double pts = cellRows[0]->yVal.size();
      double new_hscale = (vp.width()-rightMargin) / pts;
      hScaleSave = hScale;
      hScale = new_hscale;
      double new_rows= (vp.height()-bottomMargin-(numwaves*fontHeight)) / (numwaves);
      cellHeightSave = cellHeight;
      cellHeight = new_rows;
        // remember where we are
      int xpos = ui->plotView->horizontalScrollBar()->sliderPosition();
      int ypos = ui->plotView->verticalScrollBar()->sliderPosition();
      zoomCenter = QPoint(xpos,ypos);
      ui->actionFit->setText("Restore");
      updatePaint();
      updateBounds();
   }
   else
   {
      hScale = hScaleSave;
      cellHeight = cellHeightSave;
      updatePaint();
      updateBounds();
      ui->actionFit->setText("Fit to Screen");
      ui->plotView->verticalScrollBar()->setSliderPosition(zoomCenter.y());
      ui->plotView->horizontalScrollBar()->setSliderPosition(zoomCenter.x());
      ui->plotView->verticalScrollBar()->setSliderPosition(zoomCenter.y());
   }
   zoomed = !zoomed;
}


void SimViewer::doApVisible(int state)
{
   ap_flag = state;
   updatePaint();
}


void SimViewer::doColorOn(int state)
{
   colorFlag = state;
   updatePaint();
}

void SimViewer::doAutoScaleChg(int chg)
{
   autoScale = chg;
   if (chg)
   {
      ui->autoAntiClip->blockSignals(true);
      ui->autoAntiClip->setChecked(false);
      ui->autoAntiClip->blockSignals(false);
      autoAntiClip = false;
   }
   updateCellRows();
}

void SimViewer::doClipChanged(int chg)
{
   autoAntiClip = chg;
   if (chg)
   {
      ui->autoScale->blockSignals(true);
      ui->autoScale->setChecked(false);
      ui->autoScale->blockSignals(false);
      autoScale = false;
   }
   updateCellRows();
}

// Something has changed that affects the view, update the paint lists
void SimViewer::updatePaint()
{
   updateCellRows();
   updateVoltLines();
   updateTimeLines();
   updateLineText();
   plot->invalidate(plot->sceneRect(),QGraphicsScene::ItemLayer);
}

// (re)init text and position if either has changed
void SimViewer::updateLineText()
{
   textObjIter t_iter = textItems.begin();
   if (t_iter == textItems.end()) // no list to update
      return;

   int xpos = ui->plotView->horizontalScrollBar()->sliderPosition();
   int ypos = ui->plotView->verticalScrollBar()->sliderPosition();
   QString vstr;
   QRect vp = ui->plotView->viewport()->rect();
   int wid = vp.width();
   QTextStream vstream(&vstr);
   QBrush brush(sceneFG,Qt::NoBrush);
   QPen pen(brush,2.0);
   pen.setColor(sceneFG);
   brush.setColor(sceneFG);
   brush.setStyle(Qt::SolidPattern);

   double timeval = timeValFromPos(xpos);
   double timeend = timeValFromPos(xpos+wid);
   if (timeend > currX0/1000)
      timeend  = currX0/1000;
   vstream.setRealNumberNotation(QTextStream::FixedNotation);
   vstream.setRealNumberPrecision(3);
   vstream.setFieldWidth(6);
   vstream.setPadChar(' ');
   vstream << "  Left Time: " << timeval << "   Right Time: "  << timeend << "   Width: " << timeend-timeval << " sec    ";
  if (show_voltage_markers)
      vstream << voltMark << " mv        ";
  else
      vstream << "                           ";
   (*t_iter)->setText(vstr);
   (*t_iter)->setBrush(brush);
   (*t_iter)->setPos(QPoint(xpos+10,ypos)); // 10 is offset from left edge
   ++t_iter;

   const char *varnames[] = {"", "Vm", "gK", "Thr", "Vm", "h", "Thr"};
   for (int wave = 0; wave < numwaves; ++wave)
   {
      char *txt = 0, *vartxt = 0, *var = nullptr, *val = nullptr;
      if (show_voltage_markers)
      {
         switch (popvars[wave])
         {
            case -1:
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
            {
               double vmval = popids[wave] / 10000. + voltMark * popcells[wave] / 10000.;
               const char *units[] = {"%VC", "%VC/s", "cmH2O", "frac", "frac", "frac", "L", "L", "L/s", "L/s", "cmH2O", "cmH2O", "cmH2O", "0->1", "0->1", "-1->1"};
               if (asprintf (&val, "%g %s", vmval, units[abs(popvars[wave]) - 1]) == -1) exit (1);
               break;
            }
            case 0:
               if (asprintf (&val, "%d mV", voltMark) == -1) exit (1);
               break;
            case 1:
            case 3:
               if (poptype[wave])
               {
                  if (asprintf (&val, "%d mV abs", voltMark - 50) == -1) exit (1);
               }
               else
               {
                  if (asprintf (&val, "%d mV rel", voltMark) == -1) exit (1);
               }
               break;
            case 2:
               if (asprintf (&val, "%.2f", poptype[wave] ? voltMark / 60.0 : (voltMark + 20) / 10.0) == -1) exit (1);
               break;
            case AFFERENT_INST:
               if (asprintf (&val, "%d spk/s", voltMark * popcells[wave]) == -1) exit (1);
               break;
            case AFFERENT_BIN:
               if (asprintf (&val, "%d spk/s", voltMark * (popcells[wave] & 0xffff)) == -1) exit (1);
               break;
            case AFFERENT_SIGNAL:
            case AFFERENT_BOTH:
               if (asprintf (&val, "%d ", voltMark - poptype[wave]) == -1) exit (1);
               break;
            default:
               if (popvars[wave] < 1)
                  break;
               if (asprintf (&val, "%d spk/s", voltMark * popcells[wave]) == -1) exit (1);
               break;
         }
      }
      else
      {
         if (asprintf(&val," ") == -1) exit(1);
      }
      if (popvars[wave] >= 1 && popvars[wave] <= 3)
         var = (char*) varnames[poptype[wave] * 3 + popvars[wave]];
      else
      {
         if (popvars[wave] == 4 && stepSize)
         {
            if (asprintf (&vartxt, "%.1f ms", stepSize) == -1) exit (1);
         }
         else if ((popvars[wave] == STD_FIBER || popvars[wave] == AFFERENT_EVENT) && stepSize)
         {
            if (asprintf (&vartxt, " Events") == -1) exit (1);
         }
         else if (popvars[wave] == AFFERENT_SIGNAL && stepSize)
         {
            if (asprintf (&vartxt, " Signal") == -1) exit (1);
         }
         else if (popvars[wave] == AFFERENT_BOTH && stepSize)
         {
            if (asprintf (&vartxt, " Signal and Events") == -1) exit (1);
         }
         else if (popvars[wave] == AFFERENT_INST && stepSize)
         {
            if (asprintf (&vartxt, " %.1f ms",stepSize) == -1) exit (1);
         }
         else if (popvars[wave] == AFFERENT_BIN && stepSize)
         {
            if (asprintf (&vartxt, " %d ms",popcells[wave]>>16) == -1) exit (1);
         }
         else
         {
            if (asprintf (&vartxt, "%d ms", popvars[wave]-4) == -1) exit (1);
         }
         var = vartxt;
      }
      if (popvars[wave] > 0)
      {
         if (asprintf (&txt, "C %d %s %d %s %s", popids[wave], poplabel[wave].toLatin1().data(), popcells[wave], var, val) == -1) exit (1);
      }
      else if (popvars[wave] >= -16 && popvars[wave] <= -1)
      {
         if (asprintf (&txt, " %s %s", poplabel[wave].toLatin1().data(), val) == -1) exit (1);
      }
      else if (popvars[wave] == AFFERENT_SIGNAL || popvars[wave] == AFFERENT_BOTH)
      {
         if (asprintf (&txt, " %d %s %s %s", popids[wave], poplabel[wave].toLatin1().data(), var, val) == -1) exit (1);
      }
      else if (popvars[wave] == STD_FIBER || popvars[wave] == AFFERENT_EVENT
               || popvars[wave] == AFFERENT_INST || popvars[wave] == AFFERENT_BIN)
      {
         if (asprintf (&txt, "F %d %s %d", popids[wave], poplabel[wave].toLatin1().data(), popcells[wave]) == -1) exit (1);
      }
      else
      {
         if (asprintf (&txt, " %d %s %s %s", popids[wave], poplabel[wave].toLatin1().data(), var, val) == -1) exit (1);
      }
         // update contents and position
      QString info(txt);
      int x = xpos;
      int y =  wave*cellHeight+cellHeight+ wave*fontHeight;
      QPoint new_pos(x,y);
      (*t_iter)->setText(info);
      (*t_iter)->setPos(new_pos);
      (*t_iter)->setBrush(brush);
      ++t_iter;
      free (txt);
      free (val);
      free (vartxt);
   }
}

// When this is compiled for debugging, it runs about 10 times slower than
// with the optimzer on.  I guess there is bounds checking and maybe other stuff
// in the STL vector code.
void SimViewer::updateCellRows()
{
   int x,x0,y0,rownum;
   int prev_x, prev_y;
   double nv_scale_p = 1;
   double nv_scale_n = 1;
   double cellYOffset;
   double actpotHeight;
   yPointsIter y_iter;
   actPotIter ap_iter;
   PlotType type;
   ConstCellRowListIter rowIter;
   QColor plot_gc = colorFlag ? Qt::cyan : sceneFG;
   QColor ap_gc = colorFlag ? Qt::yellow : sceneFG;

   maxYBound = 0;
// clock_t begin = clock();  // debug timing tests for optimzation
         // for each row of cell rectangles
   for (rowIter = cellRows.cbegin(); rowIter != cellRows.cend(); ++rowIter)
   {
      (*rowIter)->paintPts.clear();
      (*rowIter)->paintActPot.clear();
      (*rowIter)->paintPts.reserve( (*rowIter)->yVal.size());
      (*rowIter)->paintActPot.reserve( (*rowIter)->yVal.size());
      (*rowIter)->setPlotColor(plot_gc);
      (*rowIter)->setAPColor(ap_gc);
      rownum = (*rowIter)->rownum;
      type = (*rowIter)->plotType;
       // show action potentials or fiber events pretending to be such.
       // we show fiber events even if the ap_flag is false.
      if ((ap_flag && (*rowIter)->haveAP) || (type == PlotType::event && (*rowIter)->haveAP))
         actpotHeight = cellHeight * (*rowIter)->apScale; // and make them big
      else
         actpotHeight = 0;
      if (autoScale)
      {
        // we stretch pos values up to the max, neg down to bottom of font area
        // and zero stays zero.
        if ((*rowIter)->max != 0)
           nv_scale_p  = double(cellHeight-actpotHeight) / (*rowIter)->max;
        else
           nv_scale_p  = 1;
        if ((*rowIter)->min != 0)
           nv_scale_n  = double(fontHeight) / (*rowIter)->min;
        else
           nv_scale_n  = 1;
      }
      else if (autoAntiClip)
      {
        if ((*rowIter)->max * vScale > cellHeight-actpotHeight)
           nv_scale_p  = double(cellHeight-actpotHeight) / ((*rowIter)->max*vScale);
        else
           nv_scale_p = 1;
        if ((*rowIter)->min * vScale < 0)
           nv_scale_n  = -double(fontHeight) / ((*rowIter)->min * vScale );
        else
           nv_scale_n  = 1;
      }

      cellYOffset = rownum*cellHeight + cellHeight + rownum*fontHeight;
        // plot lines and action potentials for each box
      prev_x = prev_y = numeric_limits<int>::max();  // "impossible" values
      y_iter = (*rowIter)->yVal.begin();
      ap_iter = (*rowIter)->aPot.begin();
      for(x = 0; y_iter != (*rowIter)->yVal.end(); ++x, ++y_iter,++ap_iter)
      {
         x0 = hScale * x;
         if (autoScale)
         {
            if (*y_iter >= 0)
              y0 = -*y_iter*nv_scale_p + cellYOffset;
            else
              y0 = *y_iter*nv_scale_n + cellYOffset;
         }
         else if (autoAntiClip)
         {
            if (*y_iter >= 0)
              y0 = -*y_iter * nv_scale_p * vScale + cellYOffset;
            else
              y0 = -*y_iter * nv_scale_n * vScale + cellYOffset;
         }
         else
            y0 = (-*y_iter*vScale) + cellYOffset;

            // Draw waveforms and perhaps APs
         if (type == PlotType::wave)
         {
             // only draw unique points, skip duplicates
            if (x0 != prev_x || y0 != prev_y)
               (*rowIter)->paintPts.emplace_back(x0,y0); // plot lines
             // Always draw action potentials or we may miss some because it may
             // not be the first x0,y0, but a later one we may skip.
            if (ap_flag && *ap_iter)
               (*rowIter)->paintActPot.emplace_back(x0,y0,x0,y0-actpotHeight);
         }
         else
         {
            // the spike flag for waveforms doubles as the event flag for events
            if (*ap_iter)
            {
               (*rowIter)->paintPts.emplace_back(x0,y0); // plot vertical event line
               (*rowIter)->paintPts.emplace_back(x0, y0 - cellHeight * (*rowIter)->apScale);
               (*rowIter)->paintPts.emplace_back(x0,y0);
            }
            else
            {
               if (x0 != prev_x || y0 != prev_y)
                  (*rowIter)->paintPts.emplace_back(x0,y0);
            }
         }
         prev_x = x0;
         prev_y = y0;
         if (y0 > maxYBound)
            maxYBound = y0;
      }
   }
   maxYBound += bottomMargin;  // a bit of space at bottom so we can scroll everything into view
//clock_t end = clock();
//double elapsed_secs = double(end - begin) / CLOCKS_PER_SEC;
//cout << "time: " << elapsed_secs << endl;
   updateBounds();
}

void SimViewer::updateVoltLines()
{
   int row = 0;
   QColor gc = colorFlag ? synapse_gc[0] : sceneFG;
   QPen pen(gc);
   double y0;

   for (lineObjIter iter = voltLines.begin(); iter != voltLines.end(); ++iter,++row)
   {
      y0 = -voltMark * vScale + row*cellHeight + cellHeight + row*fontHeight;
      (*iter)->setLine(0,y0,currX0*hScale+rightMargin,y0);
      (*iter)->setPen(pen);
   }
}

// This depends on the current time marker value and the current time scaling factor
// Unlike cell lines or voltage lines, the number of these can change over time
// and with the scaling factors, so we rebuild this each time.
void SimViewer::updateTimeLines()
{
   if (tMark == -1)
   {
      timeLineParent->hide();
      return;
   }

   QColor gc;
   double xstep;

   if (stepSize != 0)
      xstep = (tMark * hScale) / stepSize;
   else
      xstep = tMark * hScale;
   QRectF rect = plot->sceneRect();
   double x_end = rect.right();
   double y_end = rect.bottom();
   gc = colorFlag ? synapse_gc[1] : sceneFG;
   QPen pen(gc);

   timeLines.clear();
   delete timeLineParent;   // quick delete of all timelines
   timeLineParent = new QGraphicsItemLayer;
   plot->addItem(timeLineParent);
   timeLineParent->setZValue(0);
   if (xstep != 0.0)
   {
      if (xstep < 2.0) // don't cover up everything
         xstep = 2.0;
      for (double x = 0 ; x < x_end; x += xstep)
      {
         QGraphicsLineItem* new_tline = new QGraphicsLineItem(x,0,x,y_end,timeLineParent);
         new_tline->setPen(pen);
         timeLines.push_back(new_tline);
      }
      timeLineParent->show();
   }
}


// The H or V size has changed. Update the bounding rects.
// We want them to be as small as possible, but big enough to show all the data,
// so the scrollbars look reasonable.
// Later:  Added the case that if a plot goes really out of its box? Min always
// >=0, but max could be max y value, way outside its box.
void SimViewer::updateBounds()
{
   int yorg = topMargin, y1=0, x1;
   int row;
   CellRowListIter rowIter;

     // in case we are shrinking the rects, invalidate the current(larger) rect
   plot->invalidate(plot->sceneRect(),QGraphicsScene::ItemLayer);
   x1 = currX0*hScale+rightMargin; // same for all
   for (rowIter = cellRows.begin(); rowIter != cellRows.end(); ++rowIter)
   {
       // bounding rect for each cell row
      row = (*rowIter)->rownum;
      y1 = cellHeight*row+cellHeight+(row+1)*fontHeight;
      (*rowIter)->adjustRect(QPoint(0,yorg),QPoint(x1,y1));
      yorg = y1;
   }
   if (maxYBound > y1)
      y1 = maxYBound;
   plot->setSceneRect(0,0,x1,maxYBound);
}

void SimViewer::doShowBoxes(bool onoff)
{
   showBoxes=onoff;
   plot->invalidate(plot->sceneRect(),QGraphicsScene::ItemLayer);
}

/* This processes the current wave stream from socket or file, reading and
   loading vars and updating display.
*/
void SimViewer::loadWave(stringstream& waveStrm)
{
   int xloop,yloop;
   QColor gc;

    // many ways to fail, lambda function to report & return
   auto bailout  = [&](auto where) {
         cout << "Error " << where << " reading current wave information block, ignoring block. " << endl;
         return;
   };

   int n;
   char *p;
   char inbuf[80] = {0};

   if (firstFile) // read the header, same in every wave file. If it changes, things break
   {
      if (!waveStrm.getline (inbuf, sizeof(inbuf)))
         bailout(1);
      if (sscanf (inbuf, "%d %lf", &numsteps, &stepSize) != 2)
         bailout(2);
      if (!waveStrm.getline (inbuf, sizeof(inbuf)))
         bailout(3);
      if (sscanf(inbuf,"%d",&numwaves) != 1)
         bailout(4);

      if (numsteps != TS)
      {
         fprintf (stderr, "simviewer is set up for %d time steps per wave file\n", TS);
         bailout(5);
      }

         // allocate a display row object for each wave/cell
      for (int cell = 0; cell < numwaves; cell++)
      {
         CellRow* newrow = new CellRow(cell,Qt::cyan,lineParent);
         cellRows.push_back(newrow);
         QGraphicsLineItem* new_vline = new QGraphicsLineItem(voltLineParent);
         voltLines.push_back(new_vline);

         popids.push_back(0);      // reserve slots for header info
         popcells.push_back(0);
         popvars.push_back(0);
         poptype.push_back(0);
         poplabel.push_back(QString());
      }
       // create slots for the text we'll need
       // first is time/mv text at top of view
      auto *text = new OverlayText(QString("Sampley"),textParent);
      text->setZValue(11); // this one always on top of rest of text
      text->setFont(itemFont);
      textItems.push_back(text);
      fontHeight = text->boundingRect().height();
      topMargin = 0;
      for (xloop = 0; xloop < numwaves; xloop++)  // then labels for rows
      {
         auto *text = new OverlayText(QString(),textParent);
         text->setZValue(10);
         text->setFont(itemFont);
         textItems.push_back(text);
      }

      // read info for each channel/wave
      int tmp_v1, tmp_v2, wave;
      for (wave = 0; wave < numwaves; wave++)
      {
         if (!waveStrm.getline (inbuf, sizeof(inbuf))
               || sscanf (p = inbuf, "%d %d%n", &tmp_v1, &tmp_v2, &n) != 2)
            bailout(6);
         popids[wave] =tmp_v1;
         popcells[wave]=tmp_v2;
         if (sscanf (p = inbuf + n, "%d %d %n", &tmp_v1, &tmp_v2, &n) == 2)
         {
            popvars[wave] = tmp_v1;
            poptype[wave] = tmp_v2;
            if (tmp_v1 <= STD_FIBER && tmp_v1 >= AFFERENT_BOTH)
            {
               cellRows[wave]->setPlotType(PlotType::event);
               if (tmp_v1 == AFFERENT_BOTH)
                  cellRows[wave]->setApScale(0.25); 
            }
            else
               cellRows[wave]->setPlotType(PlotType::wave);
            int e = strlen (p += n) - 1;
            while (e > 0 && !isgraph (p[e]))
               p[e--] = 0;
            poplabel[wave]=p;
         }
         else
         {
            popvars[wave]=0;
            poptype[wave]=0;
            poplabel[wave]="";
         }
      }
      firstFile = false;
   }
   else 
   {
      waveStrm.getline (inbuf, sizeof(inbuf)); // skip header
      waveStrm.getline (inbuf, sizeof(inbuf));
      //if anything but text changes, everything breaks,
      // but pick up changes from mid-run update.
      for (int wave = 0; wave < numwaves; wave++) // if anything but text changes, 
      {                                           // everything breaks.
         waveStrm.getline (inbuf, sizeof(inbuf));
         p = inbuf;
         sscanf(p, "%*d %*d %*d %*d %n", &n);
         int e = strlen (p += n) - 1;        // possible text changes from mid-run update
         while (e > 0 && !isgraph (p[e]))
            p[e--] = 0;
         poplabel[wave]=p;
      }
   }

     // Read in the wave info. numsteps is always 100, numwaves can vary, but
     // must be the same for every file.  2 cols per row, the first is the
     // value, the second is 1 if we should draw a spike line, 0 if not
   float tmp_popv;
   int tmp_popap;

   for (xloop = 0; xloop < numsteps; ++xloop)
   {
      int num;
     for (yloop = 0; yloop < numwaves; ++yloop)
      {
         if (!waveStrm.getline (inbuf, sizeof(inbuf)))
         {
            cout << "getline failed, inbuf is " << inbuf << endl;
            bailout(7);
         }
         else
         {
            num = sscanf (inbuf, "%f %d", &tmp_popv, &tmp_popap);
            if (num != 2)
            {
              cout << "sscanf failed, inbuf is " << inbuf << endl;
              bailout(8);
            }
         }
         cellRows[yloop]->addYVal(tmp_popv);
         if (cellRows[yloop]->min > tmp_popv) // used for per-box auto scaling
         {
            cellRows[yloop]->min = tmp_popv;
         }
         if (cellRows[yloop]->max < tmp_popv)
         {
            cellRows[yloop]->max = tmp_popv;
         }
         cellRows[yloop]->addAP(tmp_popap);
      }
   }

   currX0 += numsteps;  // this will eventually be the max X value at highest res

   QRect vp = ui->plotView->viewport()->rect();    // max H slider to show
   double pts = cellRows[0]->yVal.size();          // all pts
   double new_hscale = (vp.width()-rightMargin) / pts;
   int value = ceil(1.0/new_hscale/.01);
   value = value+10-(value%10);           // round up to 10
   if (value > ui->tbaseSlider->maximum())
      ui->tbaseSlider->setMaximum(value);

    // do we need to autoscroll?
   int max = ui->plotView->horizontalScrollBar()->maximum();
   int xpos = ui->plotView->horizontalScrollBar()->sliderPosition();
   updateBounds();
   if (xpos == max) // only if at max, if user has scrolled back, don't autoscroll
   {
      int newmax = ui->plotView->horizontalScrollBar()->maximum();
      ui->plotView->horizontalScrollBar()->setSliderPosition(newmax);
   }
}

// Time to change where we look for files. If we are currently reading files,
// and we really do chdir, then stop look for them. Need to hit the Restart
// button to wake us back up.
void SimViewer::doChDir()
{
   QString msg;
   QTextStream stream(&msg);
   QDir dir = QDir::current();
   QFileDialog fdlg(this,"Select Directory");

   fdlg.setFileMode(QFileDialog::Directory);
   fdlg.setOption(QFileDialog::ShowDirsOnly,true);
   fdlg.setFilter(QDir::AllEntries | QDir::Hidden);
   fdlg.setViewMode(QFileDialog::List);
   fdlg.setDirectory(dir);
   if (fdlg.exec())
   {
      tickToc->stop();
      QStringList dir = fdlg.selectedFiles();
      stream << "Changing directory to " << dir[0] << Qt::endl
             << "Note: No longer looking for new files." << Qt::endl
             << "Use the Restart button to resume looking for files" << Qt::endl
             << "or load existing ones." << Qt::endl;
      if (useSocket)
      {
         stream << "NOTE: Currently in \"Send Plot Data Directly To Simviewer\" mode. " << Qt::endl
                << "Switching to \"Plot Data To Files\" mode. There is no way back from this." << Qt::endl;
         useSocket = false;
         useFiles = true;
      }
      ui->infoBox->appendPlainText(msg);
      QDir::setCurrent(dir[0]);
   }
}

// Switch to file mode? Have to get here by way of chdir.
void SimViewer::doRestart()
{
   QString msg;
   QTextStream stream(&msg);
   CellRowListIter rowIter;

   if (useSocket)
      return;
   tickToc->stop();
   launchNum = currLaunch->value();
   stream << "Restarting using launch number " << launchNum << Qt::endl;
   ui->infoBox->appendPlainText(msg);

   nextWave = 0;
   currX0 = 0;
   firstFile = true;
   popids.clear();
   popcells.clear();
   popvars.clear();
   poptype.clear();
   poplabel.clear();
   cellRows.clear();
   voltLines.clear();
   for (auto item : lineParent->childItems())
      plot->removeItem(item);
   for (auto item : voltLineParent->childItems())
      plot->removeItem(item);
   for (auto item : textParent->childItems())
      plot->removeItem(item);
   for (auto item : timeLineParent->childItems())
      plot->removeItem(item);

   tickToc->start(checkTime);
}


OverlayText::OverlayText(QGraphicsItemLayer *parent) : QGraphicsSimpleTextItem(parent)
{
}

OverlayText::OverlayText(QString str, QGraphicsItemLayer *parent) : QGraphicsSimpleTextItem(parent)
{
   setText(str);
}

OverlayText::~OverlayText()
{
}

void OverlayText::paint(QPainter *painter, const QStyleOptionGraphicsItem* option, QWidget * widget)
{
   QBrush brush(SimViewer::sceneBG);
   brush.setColor(SimViewer::sceneBG);
   QRectF rect(boundingRect());
   rect.adjust(1,1,-1,-1);
   painter->fillRect(rect,brush);
   QGraphicsSimpleTextItem::paint(painter,option,widget);
}


// each one of these holds a plot line
CellRow::CellRow(int id, QColor linecolor,QGraphicsItem *parent) : QGraphicsItem(parent)
{
   rownum = id;
   plot_color = linecolor;
}

CellRow::~CellRow()
{
}

void CellRow::Reset()
{
   yVal.clear();
   aPot.clear();
   paintPts.clear();
   paintActPot.clear();
   haveAP=false;
   min=1000000;
   max=-1000000;
}

QRectF CellRow::boundingRect() const
{
   return QRectF(topLeft,bottomRight);
}

// we are shrinking or growing in Y, X generally never changes
void CellRow::adjustRect(QPoint new_TL, QPoint new_BR)
{
   prepareGeometryChange();
   topLeft = new_TL;
   bottomRight = new_BR;
}

void CellRow::paint(QPainter *painter, const QStyleOptionGraphicsItem * /*option*/, QWidget* /* widget*/ )
{
   if (paintPts.size() > 1)
   {
      QBrush brush(plot_color,Qt::NoBrush);
      QPen pen(brush,1);
      brush.setColor(plot_color);
      pen.setColor(plot_color);
      painter->setPen(pen);
      painter->setBrush(brush);
      painter->drawPolyline(paintPts.data(),paintPts.size());
      auto par = dynamic_cast<SimViewer*>(scene()->parent());
      if (par && par->showBoxes)
      {
           // debug code for seeing cell bounding rect
         brush.setColor(par->sceneFG);
         pen.setColor(par->sceneFG);
         painter->setPen(pen);
         painter->setBrush(brush);
         QRectF rect(boundingRect());
         rect.adjust(1,1,-1,-1);
         painter->drawRect(rect);
      }

   }
   if (paintActPot.size() > 0)
   {
      QBrush brush(plot_color,Qt::NoBrush);
      QPen pen(brush,1);
      brush.setColor(ap_color);
      pen.setColor(ap_color);
      painter->setPen(pen);
      painter->setBrush(brush);
      painter->drawLines(paintActPot.data(),paintActPot.size());
   }
}

SimVScene::SimVScene(QObject *parent) : QGraphicsScene(parent)
{
}

SimVScene::~SimVScene()
{
}

void SimVScene::mouseMoveEvent(QGraphicsSceneMouseEvent *me)
{
   QPointF pt = me->scenePos();
   auto par = dynamic_cast<SimViewer*>(parent());
   if (par)
   {
      QString coords;
      QString coords2;
      QTextStream update(&coords);
      QTextStream xy(&coords2);
      xy << "x: " << pt.x() << Qt::endl << "y: " << pt.y();
      par->ui->currXYRaw->setText(coords2);
      double timeval = par->timeValFromPos(pt.x());
      update.setRealNumberNotation(QTextStream::FixedNotation);
      update.setRealNumberPrecision(3);
      update.setFieldWidth(6);
      update.setPadChar(' ');
      update << timeval<< " sec";
      par->ui->currXY->setText(coords);
      QGraphicsScene::mouseMoveEvent(me);
   }
}

void SimVScene::mousePressEvent(QGraphicsSceneMouseEvent *me)
{
   auto par = dynamic_cast<SimViewer*>(parent());
   par->ui->plotView->setDragMode(QGraphicsView::ScrollHandDrag);
   QGraphicsScene::mousePressEvent(me);
}

void SimVScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *me)
{
   auto par = dynamic_cast<SimViewer*>(parent());
   par->ui->plotView->setDragMode(QGraphicsView::NoDrag);
   QGraphicsScene::mouseReleaseEvent(me);
}

