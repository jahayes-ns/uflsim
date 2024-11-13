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


/* A utility class derived from qprocess 
   An instance of this starts a program as a detached process.
   Optionally starts a network server that listens for a connection
   with the program. This sets up a communication channel between the
   program using this class and the program it launches.
*/

#include <iostream>
#include <unistd.h>
#include <QCoreApplication>
#include "sim_proc.h"
#include "lin2ms.h"

using namespace std;

SimProc::SimProc(QObject* parent, QString progName, int inst, bool sock):QProcess(parent),program(progName),instance(inst), useSocket(sock)
{
   if (useSocket)
      startServer();
   dieDieDie = new QTimer();
   dieDieDie->setSingleShot(true);
   connect(dieDieDie,&QTimer::timeout,this,&SimProc::ultimateDeath);
}

SimProc::~SimProc()
{
}

// if simrun is killed externally or is hung due to bugs,
// it never gets the terminate msg, so we never see the lost
// connection. Fake it.
void SimProc::ultimateDeath()
{
#ifdef Q_OS_LINUX
      ::kill(pid,SIGKILL);
#else
      TerminateProcess((HANDLE)pid,1);
#endif
   connectionClosed();
}

// start a unique listening server for this process
void SimProc::startServer()
{
   server = new QTcpServer();
   server->listen(QHostAddress(QHostAddress::Any));
   simPort = server->serverPort();
   portStr = QString::number(simPort);
   connect(server,&QTcpServer::newConnection,this,&SimProc::connRqst);
}

//  This lets us know our process has started
void SimProc::connRqst()
{
   QString msg = "Got a connection event for process ";
   msg += program.toLatin1().data();
   QString args = progArgs.join(" ");
   msg += " ";
   msg += args.toLatin1().data(); 
   emit printInfo(msg);
   procSock = server->nextPendingConnection();
   if (procSock)
   {
      started = true;
   }
   else
      emit printInfo("Unexpected connection rqst without a connection");
   connect(procSock, &QTcpSocket::disconnected, this, &SimProc::connectionClosed);
   connect(procSock, &QTcpSocket::readyRead, this, &SimProc::gotData);
   emit connOk(instance);
   close(); // only 1 listen/connection per class instance
}

void SimProc::connectionClosed()
{
   dieDieDie->stop();
   QString msg = "SimProc: disconnected from ";
   msg += program.toLatin1().data();
   QString args = progArgs.join(" ");
   msg += " ";
   msg += args.toLatin1().data(); 
   emit printInfo(msg);
   emit connLost(instance);
}

void SimProc::gotData()
{
   emit haveData();
}

void SimProc::pauseProc()
{
      sendMsg(pauseMsg);
      paused = true;
}

void SimProc::resumeProc()
{
      sendMsg(resumeMsg);
      paused = false;
}

void SimProc::updateProc()
{
      sendMsg(updateMsg);
}

void SimProc::terminateProg()
{
   if (procSock)
      sendMsg(terminateMsg);
   else
#ifdef Q_OS_LINUX
      ::kill(pid,SIGTERM);
#else
      TerminateProcess((HANDLE)pid,1);
#endif
   dieDieDie->start(4000); // TERM may not kill a hung simrun
   paused = false;
}


size_t SimProc::sendMsg(const QByteArray& msg)
{
   size_t sent=0;
   if (procSock)
   {
      sent = procSock->write(msg);
      procSock->flush();
   }
   return sent;
}

// Called generally in response to a readyRead signal
void SimProc::getMsg(QString& msg)
{
   msg.clear();
   QByteArray bytes;
   if (procSock)
   {
      bytes = procSock->readAll();
      msg = QString::fromStdString(bytes.toStdString());
   }
   else
   {
      QString info = "simproc:: no socket for ";
      info += program.toLatin1().data();
      info += " to read from";
      emit printInfo(info);
   }
}

// run the program as detached process
bool SimProc::progInvoke(QStringList args)
{
   if (!started)
   {
      if (useSocket)
         args << "--port" << portStr;
      progArgs=args;
      setProgram(program);
      setArguments(args);
      bool launch = startDetached(&pid);

      if (launch && procSock)  // if using socket, wait a bit for process to start
      {
         int tries = 10;
         while (tries)
         {
            if (!started)
            {
               sleep(1);
               QCoreApplication::processEvents(); // need msg loop to get signal
               --tries;
            }
            else 
               tries = 0;
         }
      }
      else
         started = launch;
   }
   return started;
}

