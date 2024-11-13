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

#ifndef SYNVIEW_H
#define SYNVIEW_H
#include <QTreeView>
#include <QModelIndex>

class synView : public QTreeView
{
   Q_OBJECT

   public:
      synView(QWidget *parent);
      virtual ~synView();
      QModelIndex indexFromType(int);

   signals:
      void rowChg(const QModelIndex&);

   protected:
      void currentChanged(const QModelIndex &, const QModelIndex &) override;

};

#endif

