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

#include "slope_spin.h"

slopeSpin::slopeSpin(QWidget *parent): QDoubleSpinBox(parent)
{
   setRange(-1000.0, 1000.0);
}  
   
slopeSpin::~slopeSpin()
{
}

double slopeSpin::valueFromText(const QString &text) const 
{
   if (text == "Not used")
      return 0.0;
   else
      return text.toDouble();
}

QString slopeSpin::textFromValue(double value) const 
{
   if (value == specialVal)
      return QString("Not used");
   else
      return QString::number(value);
}
