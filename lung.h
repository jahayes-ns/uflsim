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
typedef struct
{
  double phrenic;         /* JAH: diaphragm activation?, 0->1 */
  double abdominal;       /* JAH: abdominal muscle activation?, 0->1 */
  double pca;             /* JAH: posterior cricoarytenoid */
  double ta;              /* JAH: thyroarytenoid */
  double expic;           /* JAH: expiratory intercostals */
  double inspic;          /* JAH: inspiratory intercostals */
} Motor;

typedef struct
{
  double volume;           /* in %VC, relative to FRC */
  double flow;             /* in %VC/s */
  double pressure;         /* in cmH2O */
  double Phr_d;            /* JAH: diaphragm activation, 0->1 */
  double u;
  double lma;              /* JAH: laryngeal muscle activation, 0->1 */
  double Vdi;              /* JAH: diaphragm volume? */
  double Vab;              /* JAH: abdominal volume? */
  double Vdi_t;
  double Vab_t;
  double Pdi;              /* JAH: diaphragm pressure? */
  double Pab;              /* JAH: abdominal pressure? */
  double PL;               /* JAH: lung pressure? */
} State;

State lung (Motor m, double Sstep);
extern char baby_lung_flag;
