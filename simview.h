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

#ifndef SIMVIEW_H
#define SIMVIEW_H

#include <QGraphicsView>
#include <QWheelEvent>
#include <QApplication>

class SimWin;

class SimView : public QGraphicsView
{
   Q_OBJECT
   friend SimWin;

   public:
      explicit SimView(QWidget *parent=0);
      virtual ~SimView();
      void unZoom();
      void scaleToFit();

   protected:
      void wheelEvent(QWheelEvent*) override;
      void keyPressEvent(QKeyEvent *) override;

   public slots:

   private:
};

#endif // SIMVIEW_H
