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

#ifndef CHGLOG_H
#define CHGLOG_H


#include <QString>
#include <QTextStream>
#include <memory>

class ChgLog 
{
   public:
      ChgLog(QString name="ChangeLog.txt");
      virtual ~ChgLog();
      bool updateChangeLog(const QString&);
      bool newChangeLog(const QString&);

   protected:
      bool decide(const QString&);
      bool chkForChanges(const QString&);
      void compareGlobals();
      void compareNodes();
      void compareCells(const I_NODE&,const I_NODE&);
      void compareFibers(const I_NODE&,const I_NODE&);
      void compareSynapses();

   private:

      inode_global const *Dref=nullptr;      // current in-memory database for model
      std::unique_ptr<inode_global> OnDisk;  // what's on the disk
      QString logName;
      QString logSoFar;
      QTextStream logstrm;
};

#endif
