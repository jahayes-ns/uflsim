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

#ifndef SIMVIEWER_H
#define SIMVIEWER_H

#include <QMainWindow>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QTimer>
#include <QKeyEvent>
#include <QFont>
#include <QFontMetrics>
#include <QPoint>
#include <QLine>
#include <QRect>
#include <vector>
#include <sstream>
#include <QPen>
#include <QBrush>
#include <QScrollBar>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsSceneMouseEvent>
#include <QScrollBar>
#include <QBrush>
#include <QSpinBox>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QTcpServer>


const int TS=100;     // time steps per wave file (todo: make arbitrary)
const int checkTime = 500; // ms to wait between file checks
const int DRAW_COLORS=100;
const int VOLT_TEXT=0;
namespace Ui { class SimViewer; }

// This is a dummy parent class that lets us group items and operate on them as
// a group, such as zorder or show/hide or scale a set of objects, such as
// plot lines, without scaling all the other objects.
class QGraphicsItemLayer : public QGraphicsItem
{
   public:
      QGraphicsItemLayer() {};
      virtual ~QGraphicsItemLayer(){};
      QRectF boundingRect() const {return QRectF(0,0,0,0);};
      void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *) {};
};

class OverlayText : public QGraphicsSimpleTextItem
{
   public:
      explicit OverlayText(QGraphicsItemLayer *parent=nullptr);
      explicit OverlayText(QString str,QGraphicsItemLayer *parent=nullptr);
      virtual ~OverlayText();
      QRectF boundingRect() const override { return QGraphicsSimpleTextItem::boundingRect();};

   protected:
      void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override ;
};


using yPoints = std::vector <double>;
using yPointsIter = yPoints::iterator;

using actPot = std::vector <int>;
using actPotIter = actPot::iterator;

using lineObj = std::vector <QGraphicsLineItem*>;
using lineObjIter = lineObj::iterator;

using lineSegs = std::vector <QPoint>;
using lineSegsIter = lineSegs::iterator;

using apSegs = std::vector <QLine>;
using apSegsIter = apSegs::iterator;

using textObj = std::vector <OverlayText*>;
using textObjIter = textObj::iterator;

using oneWave = std::stringstream;
using waveMap = std::map<size_t, oneWave>;
using waveMapIter = waveMap::iterator;

class SimViewer;

enum class PlotType {wave=1,event};

// each one of these is a row of lines in the view.
class CellRow : public QGraphicsItem
{
   friend SimViewer;

   public:
      explicit CellRow(int id, QColor, QGraphicsItem *parent=nullptr);
      virtual ~CellRow();
      void addYVal(float val)      { yVal.push_back(val);}
      void addAP(int val)        { aPot.push_back(val); if (val) haveAP=true;}
      void addPaintPt(QPoint pt) { paintPts.push_back(pt);}
      void addPaintActPot(QLine line) { paintActPot.push_back(line);}
      void adjustRect(QPoint,QPoint);
      int numPts() {return yVal.size();}
      void setPlotColor(QColor c) {plot_color = c;}
      void setAPColor(QColor c) {ap_color = c;}
      void setPlotType(PlotType type) {plotType = type;}
      void setApScale(double scale) {apScale = scale;}
      void setHaveBound(bool);
      void Reset();
      int rownum;

   protected:
      QRectF boundingRect() const override;
      void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override ;

   private:
      QPoint   topLeft;
      QPoint   bottomRight;
      yPoints  yVal;        // raw voltages from file
      actPot   aPot;        // action potential plot/don't plot flags
      lineSegs paintPts;    // current list of voltage lines to draw
      apSegs   paintActPot; // current list of actpot lines to draw
      QColor   plot_color;
      QColor   ap_color=Qt::yellow;
      bool     haveAP=false;
      PlotType  plotType;
      double    min=1000000;
      double    max=-1000000;
      double    apScale = 0.5;
};


// a list of the above
using CellRowList = std::vector<CellRow*>;
using CellRowListIter = std::vector<CellRow*>::iterator;
using ConstCellRowListIter = std::vector<CellRow*>::const_iterator;


class SimVScene : public QGraphicsScene
{
   Q_OBJECT

   public:
      explicit SimVScene(QObject *parent=0);
      virtual ~SimVScene();

   protected:
      void mouseMoveEvent(QGraphicsSceneMouseEvent *mouseEvent) override;
      void mousePressEvent(QGraphicsSceneMouseEvent *mouseEvent) override;
      void mouseReleaseEvent(QGraphicsSceneMouseEvent *mouseEvent) override;
};

class SimViewer : public QMainWindow
{
   Q_OBJECT
   friend SimVScene;
   friend CellRow;

   public:
      explicit SimViewer(QWidget *parent = 0,int launch=0,int port=0, bool useSocket=false, bool readFile=false, QString hostname="");
      ~SimViewer();
      void connectWithSimBuild();
      double timeValFromPos(int);
      static QColor sceneFG;
      static QColor sceneBG;

   private slots:
      void on_spacingSlider_valueChanged(int);
      void on_vgainSlider_valueChanged(int);
      void on_voltmarkerSlider_valueChanged(int);
      void on_timemarkerSlider_valueChanged(int);
      void on_invertBW_stateChanged(int);
      void timerFired();
      void on_actionQuit_triggered();
      void on_apVisible_stateChanged(int);
      void on_colorOn_stateChanged(int);
      void hScroll(int);
      void vScroll(int);
      void on_showText_stateChanged(int);
      void on_actionactionPDF_triggered();
      void on_actionFit_triggered();
      void on_launchViewer_stateChanged(int);
      void on_winSplitter_splitterMoved(int, int);
      void on_actionDebug_show_row_boxes_toggled(bool);
      void buildConnectOk();
      void buildConnectionClosedByServer();
      void runConnectOk();
      void fromSimBuild();
      void fromSimRun();
      void buildSocketError(QAbstractSocket::SocketError);
      void runSocketError(QAbstractSocket::SocketError);
      void on_autoScale_stateChanged(int);
      void on_tbaseSlider_actionTriggered(int action);
      void on_actionChange_Directory_triggered();
      void on_actionRestart_triggered();
      void runConnRqst();
      void runConnectionClosed();
      void on_autoAntiClip_stateChanged(int arg1);

   signals:
      void fwdInfo(QString);

   protected:
      void closeEvent(QCloseEvent *evt) override; 
      void resizeEvent(QResizeEvent *event) override;
      void setupViewer();
      void doTBase(int);
      void doSpacing(int);
      void doVGain(int);
      void doTime(int);
      void doVolt(int);
      void doInvert(int);
      void toggleFit();
      void doShowText(int);
      void doApVisible(int);
      void doColorOn(int);
      void loadWave(std::stringstream&);
      void keyPressEvent(QKeyEvent *) override;
      void CreatePDF();
      void loadAllExisting();
      void updateCellRows();
      void updateVoltLines();
      void updateTimeLines();
      void updateVoltMarker();
      void updateBounds();
      void updatePaint();
      void updateLineText();
      void launchViewerState(bool);
      void loadSettings();
      void saveSettings();
      void doShowBoxes(bool);
      void doAutoScaleChg(int);
      void doChDir();
      void doRestart();
      void createWave(std::stringstream &);
      void doClipChanged(int);

   private:
      Ui::SimViewer *ui;
      SimVScene *plot=nullptr;
      QTimer *tickToc=nullptr;
      QSpinBox *currLaunch;
      QColor synapse_gc[DRAW_COLORS];
      QFont itemFont;
      QFontMetrics fontInfo=QFontMetrics(itemFont);
      QGraphicsItemLayer *lineParent;
      QGraphicsItemLayer *voltLineParent;
      QGraphicsItemLayer *timeLineParent;
      QGraphicsItemLayer *textParent;
      bool  launchPdfViewer=false;
      int   launchNum=0;
      int   numsteps=0;
      int   numwaves=0;
      waveMap waveData;
      std::vector <int> popids;
      std::vector <int> popcells;
      std::vector <int> popvars;
      std::vector <int> poptype;
      std::vector <QString> poplabel;
      lineObj voltLines;
      lineObj timeLines;
      textObj textItems;
      FILE  *wave_fp=0;
      int   nextWave=0;
      int   currentWave=0;
      int   block=0;
      float hScale=.0625;
      float vScale=1.5;
      float fontHeight=0;
      int   cellHeight=100;
      bool  ap_flag=true;
      bool  colorFlag=true;
      int   tMark=-1;
      int   voltMark=0;
      bool   show_voltage_markers=true;
      bool   show_labels=true;
      double stepSize=0;
      unsigned int currX0=0; // this increases each time we add a new y value
      CellRowList cellRows;
      bool firstFile=true;
      bool zoomed=false;
      bool showBoxes=false;
      double hScaleSave;
      double cellHeightSave;
      QPoint zoomCenter;
      int endMargin=40;
      int topMargin;
      int rightMargin=20;
      int bottomMargin=60;
      int      guiPort=0;
      bool useSocket;
      bool useFiles;
      QTcpSocket *guiSocket=nullptr;
      QTcpServer *viewerServer=nullptr;
      QTcpSocket *runSocket=nullptr;
      QString hostName;
      int      maxYBound;
      bool     autoScale;
      bool     autoAntiClip;
      int      timeSliderSign=1;
};

#endif // SIMVIEWER_H
