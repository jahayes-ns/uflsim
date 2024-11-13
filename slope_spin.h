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


#include <QDoubleSpinBox>


// Custom spinbox
class slopeSpin : public QDoubleSpinBox
{
  Q_OBJECT
   public:
      slopeSpin(QWidget *parent = 0);
      virtual ~slopeSpin();
      void setSpecialVal(double val) { specialVal = val;};

   protected:
      double valueFromText(const QString &text) const override;
      QString textFromValue(double value) const override;

   private:
      double specialVal = 0.0;
};

