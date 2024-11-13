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

#ifndef LAUNCHWINDOW_H
#define LAUNCHWINDOW_H

#include <QDialog>
#include <QPointer>
#include <QProxyStyle>
#include <QProcess>
#include <QTimer>
#include <QMouseEvent>
#include "c_globals.h"
#include "launch_model.h"
#include "sim_proc.h"

namespace Ui {
class launchWindow;
}

class SimScene;
class SimWin;


struct simChild
{
   QPointer<SimProc>  simProc;
   QPointer<SimProc> viewProc;
};


enum fType {BDT,EDT,CUSTOM};

class launchWindow : public QDialog
{
    Q_OBJECT
       friend SimScene;

   public:
      explicit launchWindow(SimWin *parent = 0);
       ~launchWindow();
      void fileLoaded();
      void resetModels();
      bool launchDirty(){return scrDirtyFlag;}
      void bdtSel();
      void edtSel();
      void customSel();
      void saveSnd();
      void adjustLaunchFibers();
      void reParent(bool);

   private slots:
       void on_launchCancel_clicked();
       void on_launchDelSelected_clicked();
       void on_launchButton_clicked();
       void on_bdtDelSelected_clicked();
       void on_launchNum_valueChanged(int arg1);
       void on_launchCopyToNextButton_clicked();
       void on_launchKill_clicked();
       void on_pauseResume_clicked();
       void on_launchAddLung_clicked();
       void notifySimConnLost(int);
       void notifySimConnOk(int);
       void notifyViewConnLost(int);
       void notifyViewConnOk(int);
       void fwdMsg(QString);
       void gotPort();
       void chkSimRun();
       void setScrDirty();
       void on_midRunUpdateButton_clicked();
       void newRow(const QModelIndex &, const QModelIndex &);

   signals:

   protected:
      void closeEvent(QCloseEvent *evt) override;
      void mouseReleaseEvent(QMouseEvent *) override;
      bool event(QEvent* event) override;
      void loadSettings();
      void saveSettings();
      void copyModels();
      void loadModels();
      void launchExit();
      void launchNumChanged(int);
      void launchSim();
      void killProgs();
      void killSim();
      void killView();
      void launchPause();
      void addLungRow();
      void midRunUpdate();
      int valid (int, int );
      const char* get_comment1(int,int);
      void modelsToVars();
      int lookupDidx(int,int);
      int getMaxPop(int);
      void initAnalogRecs();
      void initHostNameRecs();
      bool validateFiles();

   private:
      SimWin* mainwin;
      Ui::launchWindow *ui;
      plotModel *plotDeskModel=nullptr;
      bdtModel *bdtListModel=nullptr;
      analogWidgetModel *analogModel=nullptr;
      hostNameWidgetModel *hostNameModel=nullptr;
      plotCombo *plotDeskCombo=nullptr;
      plotSpin *plotDeskBin=nullptr;
      plotSpin *plotDeskScale=nullptr;
      plotEdit *plotLineEdit=nullptr;
      QDataWidgetMapper *analogMapper=nullptr;
      QDataWidgetMapper *hostNameMapper=nullptr;
      simChild simLaunches[MAX_LAUNCH]={{nullptr,nullptr}};
      bool scrDirtyFlag = false;
      int killRun = 0;
      fType fnameSel = BDT;
      QByteArray simMsg;
      QByteArray sndMsg;
      QByteArray scriptMsg;
};

class dropStyle: public QProxyStyle
{
   public:
   dropStyle(QStyle* style = 0);
   virtual ~dropStyle() {};

   void drawPrimitive(PrimitiveElement element, const QStyleOption * option, QPainter * painter, const QWidget * widget = 0 ) const;
};


#endif // LAUNCHWINDOW_H
