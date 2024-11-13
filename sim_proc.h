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


#ifndef SIM_PROC_H
#define SIM_PROC_H

#include <sys/types.h>
#include <signal.h>
#include <QString>
#include <QStringList>
#include <QProcess>
#include <QTimer>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>


// We need a bit more info about our launched processes.
// Here it is.

// Several built-in messages
const QByteArray pauseMsg("P");
const QByteArray resumeMsg("R");
const QByteArray terminateMsg("T");
const QByteArray updateMsg("U");

class SimProc : public QProcess
{
   Q_OBJECT

     public:
      explicit SimProc(QObject *, QString, int, bool);
      virtual ~SimProc();
      bool progInvoke(QStringList list = QStringList());
      void terminateProg();
      void pauseProc();
      void resumeProc();
      void updateProc();
      bool isStarted() { return started;}
      bool isPaused() { return paused; }
      int getInstance() {return instance;}
      size_t sendMsg(const QByteArray&);
      void getMsg(QString&);
      QTcpSocket* getSocket() { return procSock;}
      QString getPort() { return portStr;}

   private slots:
      void connRqst();
      void connectionClosed();
      void gotData();
      void ultimateDeath();

   signals:
      void connLost(int);
      void connOk(int);
      void haveData();
      void printInfo(QString);

   protected:
      void startServer();
   
   private:
    QString program;
    QStringList progArgs;
    unsigned short simPort=0;
    QString portStr;
    int instance=0;
    bool started = false;
    bool useSocket = false;
    QTcpServer* server=nullptr;
    QTcpSocket*  procSock = nullptr;
    qint64 pid=0;
    bool paused = false;
    QTimer *dieDieDie;
};

#endif // SIM_PROC_H
