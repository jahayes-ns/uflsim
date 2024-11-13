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
#include <iostream>
#include <QtMath>
#include <QScrollBar>
#include "simview.h"

using namespace std;

SimView::SimView(QWidget *parent) : QGraphicsView(parent)
{
   horizontalScrollBar()->setPageStep(1);
   verticalScrollBar()->setPageStep(1);
}

SimView::~SimView()
{
}

void SimView::keyPressEvent(QKeyEvent *evt)
{
   QWheelEvent *w_evt;
   QPointF pt(0,0);

   switch(evt->key())
   {
      case Qt::Key_Up:
         if (QApplication::queryKeyboardModifiers() & Qt::ControlModifier)
         {
            w_evt = new QWheelEvent(pt, pt, QPoint(0,120),QPoint(0,120),
                              Qt::NoButton,Qt::ControlModifier,Qt::NoScrollPhase,
                              false,Qt::MouseEventNotSynthesized);
            wheelEvent(w_evt);
            delete w_evt;
         }
         else
            QGraphicsView::keyPressEvent(evt);
         break;

      case Qt::Key_Down:
         if (QApplication::queryKeyboardModifiers() & Qt::ControlModifier)
         {
            w_evt = new QWheelEvent(pt, pt, QPoint(0,-120),QPoint(0,-120),
                              Qt::NoButton,Qt::ControlModifier,Qt::NoScrollPhase,
                              false,Qt::MouseEventNotSynthesized);
            wheelEvent(w_evt);
            delete w_evt;
         }
         else
            QGraphicsView::keyPressEvent(evt);
         break;

      default:
         QGraphicsView::keyPressEvent(evt);
         break;
   }
}

void SimView::wheelEvent(QWheelEvent* event)
{
   static const int sensitive=4;

   int angle = event->angleDelta().y();
   if (event->modifiers() & Qt::ControlModifier) 
   {
      const ViewportAnchor anchor = transformationAnchor();
      setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
      qreal factor;
      if (angle > 0) 
         factor = 1.05;
      else 
         factor = 0.95;
      scale(factor, factor);
      setTransformationAnchor(anchor);
   }
   else if (event->modifiers() & Qt::ShiftModifier)
   {
      int pos = horizontalScrollBar()->value() - angle/sensitive;
      horizontalScrollBar()->setSliderPosition(pos);
   }
   else
   {
      int pos = verticalScrollBar()->value() - angle/sensitive;
      verticalScrollBar()->setSliderPosition(pos);
   }
   event->accept();
}

void SimView::unZoom()
{
   resetTransform();
   QRectF rect(0,0,1,1);
   ensureVisible(rect,0,0);
}

void SimView::scaleToFit()
{
   QRectF b = scene()->itemsBoundingRect();
   fitInView(b, Qt::KeepAspectRatio);
}
