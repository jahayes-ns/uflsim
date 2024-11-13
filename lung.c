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


//gcc -DSTANDALONE -Wall --std=c99 -o lung lung.c -lgsl -lgslcblas -lm
//gcc -DNEWMAIN -Wall --std=c99 -o lung lung.c -lgsl -lgslcblas -lm
// JAH: gcc -DNEWMAIN -Wall -L/home/hayesjohn/anaconda3/lib -I/home/hayesjohn/anaconda3/include --std=c99 -o lung lung.c -lgsl -lgslcblas -lm

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <error.h>
#include <assert.h>
#include <time.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_odeiv.h>
#include <gsl/gsl_multiroots.h>
#include <gsl/gsl_integration.h>
#include <gsl/gsl_roots.h>
#include <gsl/gsl_spline.h>
#include "lung.h"

#ifndef NEWMAIN
   extern char inPath[];
   extern char outPath[];
#else
   char outPath[2048];
#endif

#if defined Q_OS_WIN
#include "lin2ms.h"
#endif

typedef struct
{
  double Vdi;                   /* diaphragm volume, liters */
  double Vab;                   /* abdominal volume, liters */

  double u;                     /* abdominal muscle activation, 0->1 */
  double Phr_d;                 /* diaphragm activation, 0->1 */
  double lma;                   /* laryngeal muscle activation, 0->1 */
  double inspic;                /* JAH: inspiratory intercostal, 0->1? */
  double expic;                 /* JAH: expiratory intercostal, 0->1? */

  double u_i;                   /* abdominal muscle activation, before limit */
  double Phr_d_i;               /* diaphragm activation, before limit */
  double lma_i;                 /* laryngeal muscle activation, before limit */
  double inspic_i;              /* JAH: inspiratory intercostal, before limit? */
  double expic_i;               /* JAH: expiratory intercostal, before limit? */

  double k1;                    /* Rohrer's constants */
  double k2;
  double Rrs;                   /* airway resistance, cmH2O/(l/s) */
  double Vdi_t;                 /* time derivative of Vdi_t, liters/s */
  double Vab_t;                 /* time derivative of Vab_t, liters/s */
  double Vdi_diff;
  double best_Vdi;
} Params;

double lmmfr = 40;  /* spikes/s/cell: see notes100128 */

static gsl_interp_accel *acc;
static gsl_spline *spline;

char baby_lung_flag;
bool debug;

/* diaphragm parameters */

#ifdef NEW_C1
static double Pdimax = 260.091659;
static double Vdi0   = 29.0606678;
static double Vdiw   = 30.3416590;
static double C1     = .435958663;
static double VdiFRCst;
static double VCst;
static double passive_scale;
static double passive_offset;
#else
static double Pdimax;                     /* cmH2O, calculated in paramgen */
static double Vdi0 = 6.30549;             /* liters, optimal volume of diaphragm, from ffl_fit, see notes100226 */
static double C1        =  .369;  /* see notes 100410 */
#endif

static double Kdi_psv;                    /* dimensionless, passive tension ratio, calculated in paramgen */
static double Ldi_min = .64;              /* dimensionless, relative diaphragm length at zero volume, see notes100320, calculated in paramgen */
static double Fdi = .15;                    /* dimensionless, fraction of Pdi that expands the rib cage, see notes100320 */
static double PdiRV = 20;                   /* see notes100422 */

/* abdominal muscle parameters */
static double FCEmax = 33;      /* newtons, maximal force for 1.5 cm² unit of human abdominal muscle */
static double LCE0 = 19.1;      /* cm, length of human transversus abdominus, Gaumann, in summary */
static double Vab0 = 5.58636;   /* liters, volume of the abdominal dome at Pab = 0, see notes100204 */
static double th = 1;           /* cm, thickness of the abdominal muscles, see notes090818 */
static double ct = 0.320496;    /* length of the chord connecting the ends of a transverse section */
static double Cab = .108;       /* L/cmH2O, abdominal compliance, see notes100403 */
static double DPab = 0;        /* cmH2O, pressure difference, abdominal wall relative to diaphragm, see notes100303 */

/* lung parameters */
static double CL = .201;          /* l/cmH2O, lung compliance, see notes100227 */
static double VL0 = 1.41;       /* liters, lung volume at 0 pressure, calculated in paramgen */

/* rib cage parameters */
static double Crc = .110;       /* l/cmH2O, rib cage compliance, Lichtenstein Table 1 */
static double Vrc0 = 7.1412;    /* liters, rib cage volume at 0 pressure, see notes100304 */
static double limitVrc = 2.2;   /* liters, upper minus lower asymptotic limits of Vrc, see notes100317 */
static double Rrc = 2.7;        /* see notes120205 */

/* volume parameters */
static double A = 0.425;        /* dimensionless, ratio of decrease in diaphragm volume to increase in rib cage volume, see notes100226 */
static double Vsum = 13.907;    /* liters, constant value of Vrc*A+Vdi+Vab, see notes100304 */
 
/* other parameters */
static double Rrs0 = 4;         /* cmH2O/(l/s), resting resistance of the respiratory system, Riddle Fig 4 */
static double VC = 5.370;       /* L, vital capacity, Riddle & Younes, just used to convert to %VC on output */
static double VdiFRC = 2.967;  /* liters, diaphragm volume at FRC for initialization, from fit_cluzel_2.mx.0 */
static double VabFRC = 5.586;  /* liters, abdominal volume at FRC for initialization, calculated in paramgen */
static double VCEmax = 34.7;    /* cm/s, maximal contractile velocity of the abdominal muscles (external oblique), Ratnovsky 2003 Table 2*/

static double VLRV;
static double VrcRV;
static double VdiRV;
static double VabRV;
static double VLTLC;
static double VrcTLC;
static double VabTLC;
static double VrcKM_FRC = .1282;  /* Konno-Mead 67 Figure 14 B (supine) */
static double VrcKM_TLC = .6609;  /* Konno-Mead 67 Figure 14 B (supine), adjusted, see notes100420 */
static double VabKM_FRC = .0400;  /* Konno-Mead 67 Figure 14 B (supine) */
static double VabKM_TLC = .3391;  /* Konno-Mead 67 Figure 14 B (supine), adjusted, see notes100420 */
static double VrcFRC    =  7.013; /* Cluzel, adjusted, see notes100420 */
static double VLFRC     =  2.290; /* Cluzel, adjusted, see notes100420 */
static double VdiTLC;
static double Vc        =  1.756; /* Cluzel, adjusted, see notes100420 */
static double Pdirc;
static double Pabrc;
static double Vc;
static double VdiTLC;
static double Vrc_min;
static double Vrc_max;
static double Prc_div;
static double Prc_add;
static double Pica_ab_TLC = -135; /* see notes110929 */
static double Pica_ab_RV;
static double Pica_di_TLC;


//enum {Pdimax_, Vdi0_, Kdi_psv_, FCEmax_, LCE0_, Vab0_, th_, limitVrc_, ct_, Cab_, DPab_, CL_, VL0_, Crc_, Vrc0_, A_, Vsum_};
enum {Pdimax_, Vdi0_, Ldi_min_, FCEmax_, LCE0_, Vab0_, th_, limitVrc_, Cab_, DPab_, CL_, VL0_, Crc_, Vrc0_, A_, Vsum_, Fdi_};

static double *fitparam[] = {
  [Pdimax_] =  &Pdimax,
  [Vdi0_]   =  &Vdi0,
  [Ldi_min_]=  &Ldi_min,
  [FCEmax_] =  &FCEmax,
  [LCE0_]   =  &LCE0,
  [Vab0_]   =  &Vab0,
  [th_]     =  &th,
  [limitVrc_] =  &limitVrc,
  [Cab_]    =  &Cab,
  [DPab_]   =  &DPab,
  [CL_]     =  &CL,
  [VL0_]    =  &VL0,
  [Crc_]    =  &Crc,
  [Vrc0_]   =  &Vrc0,
  [A_]      =  &A,
  [Vsum_]   =  &Vsum,
  [Fdi_]    =  &Fdi,
};
static int fitparam_count = sizeof fitparam / sizeof fitparam[0];


#ifdef STANDALONE
static char *paramname[] = {
  [Pdimax_] =  "Pdimax",
  [Vdi0_]   =  "Vdi0",
  [Ldi_min_]=  "Ldi_min",
  [FCEmax_] =  "FCEmax",
  [LCE0_]   =  "LCE0",
  [Vab0_]   =  "Vab0",
  [th_]     =  "th",
  [limitVrc_] =  "limitVrc",
  [Cab_]    =  "Cab",
  [DPab_]   =  "DPab",
  [CL_]     =  "CL",
  [VL0_]    =  "VL0",
  [Crc_]    =  "Crc",
  [Vrc0_]   =  "Vrc0",
  [A_]      =  "A",
  [Vsum_]   =  "Vsum",
  [Fdi_]    =  "Fdi",
};
static double initparam[sizeof fitparam / sizeof fitparam[0]];

static double get_RV (double *Vrcp);
static double get_TLC (double *Vrcp);

#endif

static double get_sigma_ab (double u, double Vab, double Vab_t, double *deriv, bool dof);

static double
get_Vab (double Pab_tgt)
{
  double hi = 7.54865656383003980;
  double lo = 1.37597978514467800;
  double mid = 0;

  if (Pab_tgt > 231.16859958053188961 || Pab_tgt < 0.00000000000002715)
    error (1, 0, "%s", "gab_Vab: Pab out of range");

  while (nextafter (lo, hi) < hi) {
    mid = (hi + lo) / 2;

    Vab0 = mid + VabKM_FRC * VC;

    double Pab = get_sigma_ab (1, mid, 0, NULL, true);
    //    printf ("get_Vab: Pab_tgt = %g, Vab0 = %g, Vab = %g, Pab = %g\n", Pab_tgt, Vab0, mid, Pab);
    if (Pab > Pab_tgt)
      hi = mid;
    else if (Pab < Pab_tgt)
      lo = mid;
    else
      return mid;
  }
  return mid;
}

static double get_sigma_L (double VL);
static double get_sigma_rc (double Vrc, double Vdi_t, double Vab_t);

static inline double
get_fa (double Vdi, double VL)
{
  return (Vdi - VdiTLC) / (Vdi - VdiTLC + VL) / (1 + C1) + .15;
}

static int v_zero_fdf (const gsl_vector *x, void *params, gsl_vector *f, gsl_matrix *j);

static void
static_balance (Params *p)
{
  gsl_vector *x = gsl_vector_alloc (2);
  gsl_vector *f = gsl_vector_alloc (2);
  gsl_vector_set (x, 0, 0);
  gsl_vector_set (x, 1, 0);
  v_zero_fdf (x, p, f, NULL);
  printf ("static balance at Vdi = %g, Vab = %g, Phr_d = %g, u = %g: f0 = %g, f1 = %g\n",
          p->Vdi, p->Vab, p->Phr_d, p->u, gsl_vector_get (f, 0), gsl_vector_get (f, 1));
  gsl_vector_free (f);
  gsl_vector_free (x);
}

static void static_solution (Params *p);
static double get_Vdi_diff (double Vab, void *params) ;

static void
TLC_balance (void)
{
  Params p;
  p.Vdi   = VdiTLC;
  p.Vab   = VabTLC;
  p.u     = 0;
  p.Phr_d = 1;
  p.lma   = 0x0p+0               ; /* 0                      */
  p.k1    = 0x1.cc4ca32b5c82ep-9 ; /* 0.0035118054562923917  */
  p.k2    = 0x1.5c8cd47b4220fp-1 ; /* 0.68076194767587761    */
  p.Rrs   = 0x1.2cd4c2598dddbp+2 ; /* 4.7004857897331602     */
  p.Vdi_t = 0;
  p.Vab_t = 0;
  printf ("Vdi_diff at VabTLC: %-20.17g\n", get_Vdi_diff (VabTLC, &p));
  static_solution (&p);
  printf ("static solution at Vdi = %g, Vab = %g\n", p.Vdi, p.Vab);
  static_balance (&p);
} 

static void
RV_balance (void)
{
  Params p;
  p.Vdi   = VdiRV;
  p.Vab   = VabRV;
  p.u     = 1;
  p.Phr_d = 0;
  p.lma   = 0x0p+0               ; /* 0                      */
  p.k1    = 0x1.cc4ca32b5c82ep-9 ; /* 0.0035118054562923917  */
  p.k2    = 0x1.5c8cd47b4220fp-1 ; /* 0.68076194767587761    */
  p.Rrs   = 0x1.2cd4c2598dddbp+2 ; /* 4.7004857897331602     */
  p.Vdi_t = 0;
  p.Vab_t = 0;
  static_balance (&p);
} 

static void
paramgen (void)
{
  VL0 = VLRV = VLFRC - (VrcKM_FRC + VabKM_FRC) * VC;
  double PLRV = get_sigma_L (VLRV);
  double PabRV = PdiRV - PLRV;
  VabRV = get_Vab (PabRV);
  VabFRC = VabRV + VabKM_FRC * VC;
  Vsum = VdiFRC + C1 * VrcFRC + VabFRC;
  VrcRV = VrcFRC - VrcKM_FRC * VC / (1 + C1);
  VdiRV = Vsum - VabRV - C1 * VrcRV;
  VrcTLC = VrcFRC + (VrcKM_TLC - VrcKM_FRC) * VC / (1 + C1);
  VabTLC = VabFRC + (VabKM_TLC - VabKM_FRC) * VC;
  VdiTLC = Vsum - VabTLC - C1 * VrcTLC;
  VLTLC = VrcTLC - VdiTLC - Vc;
  Vab0 = VabFRC;
  Vdi0 = VdiRV;
  Vrc0 = VrcFRC;
  if (baby_lung_flag) {
    Vrc0 = VrcRV;
    printf ("baby_lung_flag set, VrcRV = Vrc0 = %g\n", Vrc0);
  }
  else {
    printf ("baby_lung_flag not set, VrcFRC = Vrc0 = %g\n", Vrc0);
  }

#ifdef NEW_C1
  double VrcKM_FRC_st = .1852;
  double VabKM_FRC_st = .2873;
  double VCst = 5.08/4.86 * VC;
  VdiFRCst = VdiRV - C1 * VrcKM_FRC_st * VCst / (1 + C1) - VabKM_FRC_st * VCst;
  double Cr = - C1 / (1 + C1);
  passive_scale = 4.27284355705624 / (665.3891927059210 * Cr * Cr + 51.59027787116178 * Cr + 1); 
  passive_offset = 29.48773197572638 * Cr + 19.74349802103057;
#endif

  Kdi_psv = PdiRV / pow (VdiRV - VdiFRC, 2);
  if (baby_lung_flag) Kdi_psv = 0;

  double PLTLC = get_sigma_L (VLTLC);
  double PabTLC = get_sigma_ab (0, VabTLC, 0, NULL, true);
  double PdiTLC = PLTLC + PabTLC;

  double ff = .65; /* Smith says "the diaphragm is -65% shorter at TLC than at RV".
                      He means 65% of the length or 35% shorter. */

  Ldi_min = (VdiTLC - ff * VdiRV) / (VdiTLC - VdiRV / 1.05); /* see notes100420 */
  Pdimax = PdiTLC / exp (-0.5 * pow (((1 - Ldi_min) / Vdi0 * VdiTLC + Ldi_min - 1.05) / 0.19, 2));

  Vrc_min = VrcRV  - .99 * (VrcTLC - VrcRV);
  Vrc_max = VrcTLC + .05 * (VrcTLC - VrcRV);
  Prc_div = -4 * Crc / (Vrc_max - Vrc_min) / (1 + C1);
  Prc_add = log ((Vrc0 - Vrc_min) / (Vrc_max - Vrc0)) / Prc_div;

  //  log ((Vrc_max - Vrc) / (Vrc - Vrc_min)) / Prc_div + Prc_add + Rrc * Vrc_t;
  //  printf ("plot [%g:%g] log ((%g - x) / (x - %g)) / %g + %g\n", Vrc_min, Vrc_max, Vrc_max, Vrc_min, Prc_div, Prc_add);

  double PrcTLC = get_sigma_rc (VrcTLC, 0, 0);
  double f_TLC = get_fa (VdiTLC, VLTLC);
  double Pica = PLTLC + PrcTLC - (f_TLC + Fdi) * PdiTLC;
  Pdirc = Pica / PdiTLC;
  Pica_di_TLC = Pica;

  printf ("PicaTLC: %g, ", Pica);
  
  double PrcRV = get_sigma_rc (VrcRV, 0, 0);
  double f_RV = get_fa (VdiRV, VLRV);
  Pica = PLRV + PrcRV - (f_RV + Fdi) * PdiRV;
  printf ("PicaRV: %g\n", Pica);
  Pica_ab_RV = Pica;
  
  Pabrc = Pica / PabRV;
  printf ("PrcTLC: %g, PrcRV: %g\n", PrcTLC, PrcRV);
  printf ("f_TLC: %g, f_RV: %g\n", f_TLC, f_RV);
  printf ("PdiTLC: %g, PdiRV: %g\n", PdiTLC, PdiRV);
  printf ("PLTLC: %g, PLRV: %g\n", PLTLC, PLRV);
  printf ("Pdirc: %g, Pabrc: %g\n", Pdirc, Pabrc);
  printf ("Pdimax: %g\n", Pdimax);
  if (0) {
    TLC_balance ();
    RV_balance ();
  }
  printf ("\tRV\tFRC\tTLC\n");
  printf ("Vdi:\t%g\t%g\t%g\n", VdiRV, VdiFRC, VdiTLC);
  printf ("Vab:\t%g\t%g\t%g\n", VabRV, VabFRC, VabTLC);
  printf ("Vrc:\t%g\t%g\t%g\n", VrcRV, VrcFRC, VrcTLC);
  printf ("VL:\t%g\t%g\t%g\n", VLRV, VLFRC, VLTLC);

}


enum {BR_OK, BR_ROOT, BR_NOROOT};

/* starting at x_lo x_hi, find new values of x_lo and x_hi that bracket a root of f */
static int
bracket_root (double f (double x, void *params), double *x_lo, double *x_hi, void *params)
{
  double x0 = *x_lo;
  double y0 = f (x0, params);
    
  assert (isfinite (y0));
  if (y0 == 0)
    return BR_ROOT;
  double increment = *x_hi - *x_lo;
  double min = y0;
  double max = y0;
  while (true) {
    double x1 = x0 + increment;
    double y1 = f (x1, params);
    if (isnan (y1))
      return BR_NOROOT;
    //    printf ("f(%g) = %g, f(%g) = %g\n", x0, y0, x1, y1);
    if (y1 < min) min = y1;
    if (y1 > max) max = y1;
    if (fabs (x1) > 1e300)
      return BR_NOROOT;
    if (y1 == 0) {
      *x_lo = x1;
      return BR_ROOT;
    }
    if (y1 * y0 < 0) {
      *x_lo = fmin (x0, x1);
      *x_hi = fmax (x0, x1);
      break;
    }
    if (fabs (y1) == fabs (y0))
      increment *= -2;
    else if (fabs (y1) > fabs (y0))
      increment *= -2;
    else if (fabs (y1) < fabs (y0)) {
      double newinc = -y0 / (y1 - y0) * (x1 - x0);
      if (fabs (newinc) > fabs (increment))
        increment = newinc;
      y0 = y1;
      x0 = x1;
    }
  }
  if (!isfinite (f(*x_lo, params)) || !isfinite (f(*x_hi, params)))
    error (1, 0, "%s", "non-finite y\n");
  return BR_OK;
}

static void
print_params (Params *p)
{
#define P(x) printf ("  p.%-5s = %-21a; /* %-22.17g */\n", #x, p->x, p->x)
  P (Vdi);
  P (Vab);
  P (u);
  P (Phr_d);
  P (lma);
  P (k1);
  P (k2);
  P (Rrs);
  P (Vdi_t);
  P (Vab_t);
}

/* find a root of f, starting around x_lo and x_hi */
static double
solve (double f (double x, void *params), double x_lo, double x_hi, void *params)
{
  double xlo0 = x_lo, xhi0 = x_hi;
  int status = bracket_root (f, &x_lo, &x_hi, params);
  if (status == BR_ROOT  ) return x_lo;
  if (status == BR_NOROOT) return DBL_MAX;

  gsl_function F;
  F.function = f;
  F.params = params;

  gsl_root_fsolver *s = gsl_root_fsolver_alloc (gsl_root_fsolver_brent);
  gsl_root_fsolver_set (s, &F, x_lo, x_hi);

  if (xlo0 == 4.3890224609375048) {
    printf ("f1 start\n");
    debug = true;
    for (double x = xlo0; x <= xhi0; x += (xhi0 - xlo0) / 100)
      printf ("%.17f: %.17f\n", x, get_Vdi_diff (x, params));
    debug = false;

    printf ("%.17f: %.17f\n", xlo0, get_Vdi_diff (xlo0, params));
    printf ("%.17f: %.17f\n", 5.2533923149323725, get_Vdi_diff (5.2533923149323725, params));
    printf ("%.17f: %.17f\n", xhi0, get_Vdi_diff (xhi0, params));
    printf ("f1 end\n");
  }


  for (int i = 0; i < 100 && gsl_root_test_interval (x_lo, x_hi, 0, 0.000001) == GSL_CONTINUE; i++) {
    status = gsl_root_fsolver_iterate (s);
    x_lo = gsl_root_fsolver_x_lower (s);
    x_hi = gsl_root_fsolver_x_upper (s);
  }

  if (status != GSL_SUCCESS)
    error (1, 0, "%s", "root solver failed");

  double root = gsl_root_fsolver_root (s);
  gsl_root_fsolver_free (s);

  return root;
}

/* area of the segment of a circle at position y along the sagittal chord */
/* circle segment formulas can be found at http://mathforum.org/dr.math/faq/faq.circle.segment.html */
static double
segment_area (double y, void *params)
{
  double *r_c = (double *)params;
  double rt = r_c[0];
  double rs = r_c[1];
  double cs = r_c[2];

  /* h is the height of the segment */
  double h = (sqrt (rs * rs - y * y) /* segment peak to the frontal plane that passes through center of sagittal curvature */
              - sqrt (rs * rs - cs * cs / 4)); /* same frontal plane to chord plane */

  return  rt * rt * (2 * acos ((rt - h) / rt) + sin (2 * acos (h / rt - 1))) / 2; /* area of the segment */
}

static double
volume (double rs, double rt, double ct)
{
  //printf ("rs: %g, rt: %g, ct: %g\n", rs, rt, ct);
  if (rt * 2 < ct)
    rt = ct / 2;
  double h0 = rt - sqrt (1 - (ct * ct) / (4 * (rt * rt))) * rt; /* height of circle segment with chord ct and radius rt */
  double cs = 2 * sqrt (1 - pow (h0 / rs - 1, 2)) * rs;         /* length of sagittal chord with height h0 and radius rs */
  double r_c[] = {rt, rs, cs};

  int N = 1000;
  gsl_integration_workspace *w = gsl_integration_workspace_alloc (N);

  double result, error;

  gsl_function F;
  F.function = &segment_area;
  F.params = r_c;

  gsl_integration_qags (&F, -cs / 2, cs / 2, 0, 1e-7, N, w, &result, &error);

  gsl_integration_workspace_free (w);
  return result;                /* cubic meters */
}

/* compute the sagittal radius from the transverse radius */
static double
rs_rt (double rt)
{
  return 8.00479 * rt - 1.10158; /* in meters, from "song_cab rfit" (see song_cab.c) */
}

static double min_Vab;

/* integrating to find the abdominal volume and iterating that to invert the function is too slow,
   so we approximate the inverted function with a spline */
static double
init_spline (void)
{
  double
  get_Vab (double rt)
  {
    double rs = rs_rt (rt);
    return 1000 * volume (rs, rt, ct);  /* liters */
  }

  int N = 10000;                   /* number of point to fit spline */
  double cvt0 = 0;                 /* 1/meters, smallest transverse curvature */
  double cvt1 = 1 / 0.160249;      /* 1/meters, largest transverse curvature, ~2/ct */
  
  double Vab0 = 0;                 /* liters */
  double Vab1 = get_Vab (1 / cvt1); /* liters */
  double dcvt = (cvt1 - cvt0) / N; /* meters */
  double dVab = (Vab1 - Vab0) / N; /* liters */
  double last_cvt = cvt0;          /* meters */
  double last_Vab = Vab0;          /* meters */
  min_Vab = Vab0;
  
  double cvtn[2*N];
  double Vabn[2*N];
  int i = 0;
  cvtn[i] = cvt0; 
  Vabn[i++] = Vab0;
  for (double cvt = cvt0 + dcvt; cvt <= cvt1; cvt += dcvt) {
    double Vab = get_Vab (1 / cvt);

    if (fabs (Vab - last_Vab) > fabs (dVab)) { /* make sure the Vab values are no more than dVab apart */
      cvt = last_cvt + dVab / (Vab - last_Vab) * dcvt; /* by taking a smaller cvt increment when necessary */
      Vab = get_Vab (1 / cvt);
    }
    cvtn[i] = cvt; 
    Vabn[i++] = Vab;
    last_cvt = cvt;
    last_Vab = Vab;
  }
  if (last_cvt < cvt1)
    cvtn[i] = cvt1; 
  Vabn[i++] = Vab1;
  int pcnt = i;

  acc = gsl_interp_accel_alloc ();
  spline = gsl_spline_alloc (gsl_interp_cspline, pcnt);
  gsl_spline_init (spline, Vabn, cvtn, pcnt);
  return Vab1;
}

static void
vab_limit_error (void)
{
  char *msg, *cmd;
  if (asprintf (&msg,
                "\"The abdominal volume has gotten too small.\n"
                "Perhaps you are overdriving the lumbar?\n"
                "The simulation will now abort.\"") == -1) exit (1);
  if (asprintf (&cmd, "simmsg %s %s", "\"SIMRUN NOTICE\"", msg) == -1) exit (1);
  if (system (cmd));
  error (1, 0, "%s", msg);
}

/* pressure at the abdominal wall  */
static double
get_sigma_ab (double u, double Vab, double Vab_t, double *deriv, bool dof)
{
  if (Vab < min_Vab) 
    {
      printf ("%g %g\n", Vab, min_Vab);
      vab_limit_error ();
    }
      
  double cvt = gsl_spline_eval (spline, Vab, acc);        /* transverse curvature in 1 / meters */
  double rt = 1 / cvt;                                    /* transverse radius of curvature in meters */
  double rt_v = -gsl_spline_eval_deriv (spline, Vab, acc) / (cvt*cvt); /* meters/liter */

  double rs = rs_rt (rt);                                   /* meters */
  double s = 2 * rt * asin (ct / (2 * rt));                 /* length of the transverse arc in meters */
  double ds_dVab = (2 * asin (ct / (2 * rt)) * rt_v         /* derivative of s wrt Vab */
                    - ct * rt_v / (sqrt (1 - ct * ct / (4 * rt * rt)) * rt));
  double ds_dt = ds_dVab * Vab_t;     /* meters/s */
  double LCE = s * 100 / 2;     /* cm, LCE = Length of the Contractile Element, divide by 2 because
                                   there are two contractile elements in the arc */
  double LCE_t = ds_dt * 100;   /* cm/s */
  double Ffl = exp (-0.5 * pow(((LCE / LCE0 - 1.05) / 0.19),2)); /* Ratnovsky(2003) eqn A.7, LCE in
                                                                    cm, Ffl is dimensionless, 0->1 */
  double Pab = 0;

  /* constant to convert from force in newtons in a unit of abdominal
     muscle to surface tension (force/length) */
  static double k_th = (1 / 1.5  /* divide by 1.5 cm^2 to get N/cm^2 */
                                 /* 1.5 cm^2 is the cross-section of a unit of external oblique, from
                                    Ratnovsky(2003) page 1776 */
                        * 101.9716  /* cmH2O/(N/cm^2): conversion constant N/cm^2 -> cmH2O, to get cmH20 */
                        /*   * TH           cm, thickness of the muscles */
                        * .01);     /* m/cm, convert thickness to meters */
  double k = k_th * th;          /* cm, thickness of the muscles */
  /* gives the surface tension in cmH2O/meter */

  double Rab = 1.5; //1.5; /* cmH2O/(L/s), resistance of the abdominal wall, see notes091103 */
  double x =  LCE_t / VCEmax;
  double E2 = exp (-1.409 * sinh (3.2 * x + 1.59443531272566456619));
  if (dof) {
    /* Ratnovsky(2003) eqn A.8, (corrected - see notes090730), LCE_t in cm/s, Ffv dimensionless, 0->1.334 */
    double Ffv = 0.1433 / (0.1074 + E2);
    double FCE = u * FCEmax * Ffl * Ffv;   /* Newtons, Ratnovsky 2003 eqn A.6 */
    Pab = (FCE * k * (1/rt + 1/rs)         /* cmH2O, Young-Laplace equation */
           +  ((Vab - Vab0) / Cab )      /* static compliance */
           - DPab);
    Pab += Rab * Vab_t;
    if (!isfinite (Pab))
      error_at_line (1, 0, __FILE__, __LINE__, "Pab: %g, Ffv: %g, Ffl: %g, LCE_t: %g, rt = %g, rt_v = %g, ct = %g, ds_dVab: %g, Vab: %g\n",
                     Pab, Ffv, Ffl, LCE_t, rt, rt_v, ct, ds_dVab, Vab);
  }
  
  if (deriv) {
    double E3 = (x <= -2.5 || x >= 1.7) ? 0 : cosh (3.2 * x + 1.6) * E2 / pow (E2 + 0.1074,2);
    double dFfv_dLCEt = 0.64611104 * E3 / VCEmax;
    double dLCEt_dVabt = 100 * ds_dVab; /* 100 to convert from m to cm */
    *deriv = u * FCEmax * Ffl * k * (1/rt + 1/rs) * dFfv_dLCEt * dLCEt_dVabt;
    *deriv += Rab;
  }

  return Pab;
}

/* transdiaphragmatic pressure - for equations and values, see notes091124 and notes100206 */
static double
get_sigma_di (double Phr_d, double Vdi, double Vdi_t, double *deriv, bool dof)
{
  double Pdi = 0;
  //  double E = (Vdi / Vdi0 - 1.05) / 0.19;

#ifdef NEW_C1
  double VdiKM = (Vdi - VdiFRCst) / VCst * 100;
  double E = (VdiKM - Vdi0) / Vdiw;
  double Ffl = exp (-E * E);
#else
  double E = ((1 - Ldi_min) / Vdi0 * Vdi + Ldi_min - 1.05) / 0.19;
  double Ffl = exp (-0.5 * E * E);
#endif

  double Vdi_t_max = 2.449; /* L/s  */
  double x = Vdi_t / Vdi_t_max;
  double E2 = exp (-1.409 * sinh (3.2 * x + 1.59443531272566456619)); /* Vdi_t and Vdi_tmax are in L/s */

  double Rdi = 6; //6.0; /* cmH2O/(L/s), passive resistance of the diaphragm, added to stabilize simulation */

  if (dof) {
    double Ffv = 0.1433 / (0.1074 + E2);
    Pdi = Phr_d * Pdimax * Ffl * Ffv; /* cmH2O */

    //    double Vdi0_twitch = 1.05 * 1.05 * Vdi0;
#ifdef NEW_C1
    if (VdiKM > passive_offset)
      Pdi += passive_scale * pow (VdiKM - passive_offset, 2);
#else
    if (Vdi > VdiFRC) /* passive stress, see notes091124 and notes100224 */
      Pdi +=  Kdi_psv * pow (Vdi - VdiFRC, 2);
#endif
    Pdi += Rdi * Vdi_t;
  }

  if (deriv) {
    /* diff (0.1433 / (0.1074 + exp (-1.409 * sinh (3.2 * Vdi_t / Vdi_t_max + 1.59443531272566456619))), Vdi_t); */
    /* the conditional expression is to overcome numerical instability at the extremes in the formula */
    /* see notes091201 */
    double E3 = (x <= -2.5 || x >= 1.7) ? 0 : cosh (3.2 * x + 1.59443531272566456619) * E2 / pow (E2 + 0.1074,2);
    double dPdi_dVdit = Phr_d * Pdimax * Ffl * 0.64611104 * E3 / Vdi_t_max;
    *deriv = dPdi_dVdit;
    *deriv += Rdi;
  }
  return Pdi;
}

/* pressure from lung elastance */
static double
get_sigma_L (double VL)
{
  return (VL - VL0) / CL;
}

static inline double
get_Vrc (double Vdi, double Vab)
{
  return (Vsum - Vdi - Vab) / C1;
}

/* pressure from rib cage elastance */
static double
get_sigma_rc (double Vrc, double Vdi_t, double Vab_t)
{
  double Vrc_t = -(Vdi_t + Vab_t) / C1;
  
  double Prc = log ((Vrc_max - Vrc) / (Vrc - Vrc_min)) / Prc_div + Prc_add + Rrc * Vrc_t;
  if (!isfinite (Prc))
    error (1, 0, "Prc = %.17e, limitVrc = %.17e, Vrc = %.17e, Vrc0 = %.17e, Crc = %.17e, %.17e, %.17e\n",
           Prc, limitVrc, Vrc, Vrc0, Crc, 1 / ((Vrc - Vrc0) / limitVrc + .5) - 1, (Vrc - Vrc0) / limitVrc + .5);
  return Prc;
  return (Vrc - Vrc0) / Crc;
}

/* airway resistance */
static double
airway_resistance (double VL_t, Params *p, double *dRrs_dVLt)
{
#define PINF(x) (fpclassify(x) == FP_INFINITE && (x) > 0)
  if (PINF (p->k1) || PINF (p->k2)) {
    p->Rrs = INFINITY;
    if (dRrs_dVLt)
      *dRrs_dVLt = 0;
  }
  else {
    p->Rrs = (p->k1 + p->k2 * fabs (VL_t) /* Rohrer's equation */
              + .72 + .44 * fabs (VL_t)); /* rest of airway, see notes100201 */
    if (dRrs_dVLt)
      *dRrs_dVLt = (VL_t > 0 ? 1 : (VL_t < 0 ? -1 : 0)) * (p->k2 + .44);
  }
  return p->Rrs;
}

/* get the lung volume from the state */
static inline double
get_VL (double Vdi, double Vab)
{
  double VL = (Vsum - (1 + C1) * Vdi - Vab - C1 * Vc) / C1;
  if (VL < 0) VL = 0;
  return VL;
}

/* get the tracheal flow from the state */
static inline double
get_VL_t (double Vdi_t, double Vab_t)
{
  return (-(1 + C1) * Vdi_t - Vab_t) / C1;
}

#ifdef INTERCOSTALS
double intic_boost = 50;
#endif

static inline double
get_pica (Params *p, double sigma_di)
{
  double Vdi   = p->Vdi;
  double Vab   = p->Vab;
  double u     = p->u;
  double Phr_d = p->Phr_d;
  
  return (Phr_d * (Pica_di_TLC * (Vdi - VdiFRC) / (VdiTLC - VdiFRC) * (Vdi < VdiFRC))
          + u * (Pica_ab_RV + (get_Vrc (Vdi, Vab) - VrcRV) / (VrcTLC - VrcRV) * (Pica_ab_TLC - Pica_ab_RV)));
}


/*
  x specifies the value of Vdi_t and Vab_t
  returns the value of two expressions that should be zero in f,
  derivatives in j
  These are the Lichtenstein equations with the addition of the "A"
  term and the airway resistance.  See notes 090814, 090818, 091013.
*/
static int
v_zero_fdf (const gsl_vector *x, void *params, gsl_vector *f, gsl_matrix *j)
{
  Params *p = (Params *)params;
  double Vdi   = p->Vdi;
  double Vab   = p->Vab;
  double u     = p->u;
  double Phr_d = p->Phr_d;
  double Vdi_t = gsl_vector_get (x, 0);
  double Vab_t = gsl_vector_get (x, 1);
  double VL_t  = get_VL_t (Vdi_t, Vab_t);
  double dRrs_dVLt;
  double Rrs   = airway_resistance (VL_t, p, &dRrs_dVLt);

  double VL = get_VL (Vdi, Vab);
  double dsab_Vabt;
  double sigma_ab = get_sigma_ab (u, Vab, Vab_t, j ? &dsab_Vabt : 0, f);
  double dsdi_Vdit;             /* d(sigma_di)/d(Vdi_t) */
  double sigma_di = get_sigma_di (Phr_d, Vdi, Vdi_t, j ? &dsdi_Vdit : 0, f);
  double fa = get_fa (Vdi, VL);

  if (0)
    printf ("Vdi: %a, Vab: %a\n", Vdi, Vab);
  double Pdi_rc = 0;
  if (Vdi < VdiFRC)
    Pdi_rc = (Vdi - VdiFRC) / (VdiTLC - VdiFRC) * Pdirc;
  double Pab_rc = 0;
  if (Vab < VabFRC)
    Pab_rc = (Vab - VabFRC) / (VabRV - VabFRC) * Pabrc;
  double Pica = Pdi_rc * sigma_di + Pab_rc * sigma_ab;
  //  Pica = Pdi_rc * sigma_di + u * (Pica_ab_RV + (get_Vrc (Vdi, Vab) - VrcRV) / (VrcTLC - VrcRV) * (Pica_ab_TLC - Pica_ab_RV));
  Pica = get_pica (p, sigma_di); Pdi_rc = 0;

#ifdef INTERCOSTALS
  Pica = Pdirc * p->inspic + intic_boost * Pabrc * p->expic;
#endif

  if (f) {
    double sigma_L = get_sigma_L (VL);

    if (0) {
      printf ("--Pica: %g, Pdi_rc: %g, Pab_rc: %g\n", Pica, Pdi_rc, Pab_rc);
      printf ("--sigma_di = %g, sigma_L = %g, sigma_ab = %g\n", sigma_di, sigma_L, sigma_ab);
      printf ("--VL: %g, Vab: %g, u: %g\n", VL, Vab, u);
    }
    if (0)
      fprintf (stdout, 
               "double limitVrc = %.17e;\n"
               "double Vrc0 = %.17e;\n"
               "double Crc = %.17e;\n"
               "double Vrc = %.17e;\n"
               "double Vab = %.17e;\n"
               "double Vsum = %.17e;\n"
               "double Vdi = %.17e;\n"
               "double A = %.17e;\n", limitVrc, Vrc0, Crc, VL + Vdi, Vab, Vsum, Vdi, A);

    double sigma_rc = get_sigma_rc (get_Vrc (Vdi, Vab), Vdi_t, Vab_t);
    //    fprintf (stdout, "get_sigma_rc return\n");
    double f0 = -VL_t * Rrs - sigma_L + (fa + Fdi) * sigma_di + Pica - sigma_rc;
    if (0)
    if (!isfinite (f0))
      error (0, 0, "f0: %g, VL_t: %g, Rrs: %g, sigma_L: %g, fa: %g, Fdi: %g, Pdirc: %g, sigma_di: %g, sigma_rc: %g, Vrc: %g, Vrc_max: %g\n",
              f0, VL_t, Rrs, sigma_L, fa, Fdi, Pdirc, sigma_di, sigma_rc, get_Vrc (Vdi, Vab), Vrc_max);
    double f1 = sigma_ab + VL_t * Rrs + sigma_L - sigma_di;
    if (debug){
      //      printf ( "f0: %g, VL: %g, Vdi: %g, sigma_L: %g, sigma_ab: %g, sigma_rc: %g, Vrc: %g\n", f0, VL, Vdi, sigma_L, sigma_ab, sigma_rc, VL + Vdi + Vc);
      static int count;
      if (debug && count++ == 0)
        print_params (p);
      printf ("Vdi_t: %.17e, Vab_t: %.17e, f0: %.17e, f1: %.17e, norm: %g\n", Vdi_t, Vab_t, f0, f1, sqrt (f0*f0+f1*f1));
    }
    //      printf ("Vdi_t: %9.6f, Vab_t: %9.6f, f0: %13.6e, f1: %12.9f\n", Vdi_t, Vab_t, f0, f1);
    gsl_vector_set (f, 0, f0);
    gsl_vector_set (f, 1, f1);
  }

  if (j) {
    double dVLt_dVdit = -(C1 + 1) / C1; /* diff (-((1 + C1) * Vdi_t + Vab_t) / C1, Vdi_t); */
    double dVLt_dVabt = -1 / C1;       /* diff (-((1 + C1) * Vdi_t + Vab_t) / C1, Vab_t); */
    double dRrs_dVdit = dRrs_dVLt * dVLt_dVdit;
    double dRrs_dVabt = dRrs_dVLt * dVLt_dVabt;

    /* f0:-VL_t(Vdi_t, Vab_t) * Rrs(Vdi_t,Vab_t) - sigma_L + (fa + Fdi) * sigma_di(Vdi_t) - sigma_rc(Vdi_t, Vab_t); */
    double df0_dVdit = -Rrs * dVLt_dVdit - VL_t * dRrs_dVdit + (Pdi_rc + Fdi + fa) * dsdi_Vdit + Rrc / C1; /* diff (f0,Vdi_t); */
    double df0_dVabt = -Rrs * dVLt_dVabt - VL_t * dRrs_dVabt + Rrc / C1; /* diff(f0,Vab_t); */
    /* f1:sigma_ab(Vab_t) + VL_t(Vdi_t,Vab_t) * Rrs(Vdi_t,Vab_t) + sigma_L - sigma_di(Vdi_t); */
    double df1_dVdit =  Rrs * dVLt_dVdit + VL_t * dRrs_dVdit - dsdi_Vdit; /* diff(f1,Vdi_t); */
    double df1_dVabt =  Rrs * dVLt_dVabt + VL_t * dRrs_dVabt + dsab_Vabt; /* diff(f1,Vab_t); */
    if (debug)
      printf ("df0_dVdit: %.17e, df0_dVabt: %.17e, df1_dVdit: %.17e, df1_dVabt: %.17e\n",
              df0_dVdit, df0_dVabt, df1_dVdit, df1_dVabt);
    gsl_matrix_set (j, 0, 0, df0_dVdit);
    gsl_matrix_set (j, 0, 1, df0_dVabt);
    gsl_matrix_set (j, 1, 0, df1_dVdit);
    gsl_matrix_set (j, 1, 1, df1_dVabt);
  }
  return 0;
}

static int v_zero_f (const gsl_vector *x, void *params, gsl_vector *f)
{
  return v_zero_fdf (x, params, f, 0);
}

static int v_zero_df (const gsl_vector *x, void *params, gsl_matrix *j)
{
  return v_zero_fdf (x, params, 0, j);
}

typedef struct
{
  int (* f) (const gsl_vector * X, void * PARAMS, gsl_vector * F);
  int (* df) (const gsl_vector * X, void * PARAMS, gsl_matrix * J);
  int (* fdf) (const gsl_vector * X, void * PARAMS, gsl_vector * F, gsl_matrix * J);
} FuncSet;

static FuncSet v_zero_all = {v_zero_f, v_zero_df, v_zero_fdf};

typedef struct
{
  double f0;
  double f1;
  double df0_dVdit;
  double df0_dVabt;
  double df1_dVdit;
  double df1_dVabt;
} Vals;

static inline Vals
get_fn_fn_t (double Vdi_t, double Vab_t, void *params)
{
  Params *p = (Params *)params;
  gsl_vector *x = gsl_vector_alloc (2);
  gsl_vector *f = gsl_vector_alloc (2);
  gsl_matrix *j = gsl_matrix_alloc (2, 2);
  gsl_vector_set (x, 0, Vdi_t);
  gsl_vector_set (x, 1, Vab_t);
  v_zero_fdf (x, p, f, j);
  static Vals v;
  v.f0 = gsl_vector_get (f, 0);
  v.f1 = gsl_vector_get (f, 1);
  v.df0_dVdit = gsl_matrix_get (j, 0, 0);
  v.df0_dVabt = gsl_matrix_get (j, 0, 1);
  v.df1_dVdit = gsl_matrix_get (j, 1, 0);
  v.df1_dVabt = gsl_matrix_get (j, 1, 1);
  gsl_vector_free (x);
  gsl_vector_free (f);
  gsl_matrix_free (j);
  return v;
}

static inline double
get_fn (double Vab_t, void *params, int n)
{
  Params *p = (Params *)params;
  gsl_vector *x = gsl_vector_alloc (2);
  gsl_vector *f = gsl_vector_alloc (2);
  gsl_vector_set (x, 0, p->Vdi_t);
  gsl_vector_set (x, 1, Vab_t);
  double fn = NAN;
  if (v_zero_fdf (x, p, f, 0) == 0)
    fn = gsl_vector_get (f, n);
  gsl_vector_free (x);
  gsl_vector_free (f);
  return fn;
}

static inline double
get_f0 (double Vab_t, void *params)
{
  return get_fn (Vab_t, params, 0);
}

static inline double
get_f1 (double Vab_t, void *params)
{
  return get_fn (Vab_t, params, 1);
}

static void
dynamic_solutions (Params *p, double *xio, double *xio0, double dt)
{
  typedef struct
  {
    double Vdi_t;
    double Vab_t_f0;
    double Vab_t_f1;
    double diff;
  } Point;

  int stepcnt = 1000;
  double Vdi_t_0 = -10;
  double Vdi_t_1 =  10;
  double Vab_t_0 = -10;
  double Vab_t_1 =  10;
  double step = (Vdi_t_1 - Vdi_t_0) / stepcnt;
  Point p1 = {.diff = NAN};
  double min_size = DBL_MAX;
  for (double Vdi_t = Vdi_t_0; Vdi_t <= Vdi_t_1; Vdi_t += step) {
    Point p2;
    p2.Vdi_t = Vdi_t;
    p->Vdi_t = Vdi_t;
    p2.Vab_t_f0 = solve (get_f0, Vab_t_0, Vab_t_1, p);
    p->Vdi_t = Vdi_t;
    p2.Vab_t_f1 = solve (get_f1, Vab_t_0, Vab_t_1, p);
    p2.diff = p2.Vab_t_f1 - p2.Vab_t_f0;
    if (p2.diff * p1.diff <= 0) {
      double Vab_t_m = (p2.Vab_t_f0*p1.Vab_t_f1-p1.Vab_t_f0*p2.Vab_t_f1) /(-p2.Vab_t_f1+p1.Vab_t_f1+p2.Vab_t_f0-p1.Vab_t_f0);
      double Vdi_t_m = -((p1.Vab_t_f0-p1.Vab_t_f1)*p2.Vdi_t +(p2.Vab_t_f1-p2.Vab_t_f0)*p1.Vdi_t) /(-p2.Vab_t_f1+p1.Vab_t_f1+p2.Vab_t_f0-p1.Vab_t_f0);
#ifdef STANDALONE
      printf ("dynamic solution near %g %g\n", Vdi_t_m, Vab_t_m);
#endif
      double Vdi_t2 = dt > 0 ? (Vdi_t_m - xio0[0]) / dt : 0;
      double Vab_t2 = dt > 0 ? (Vab_t_m - xio0[1]) / dt : 0;
      double VL_t2 = get_VL_t (Vdi_t2, Vab_t2);
      double size = fabs (VL_t2);
      printf ("size = %g\n", size);
      if (xio && size < min_size) {
        printf ("\np1: Vdi_t %g, Vab_t_f0 %g, Vab_t_f1 %g\np2: Vdi_t %g, Vab_t_f0 %g, Vab_t_f1 %g\nusing %g %g\n\n",
                p1.Vdi_t, p1.Vab_t_f0, p1.Vab_t_f1, p2.Vdi_t, p2.Vab_t_f0, p2.Vab_t_f1, Vdi_t_m, Vab_t_m);
        xio[0] = Vdi_t_m;
        xio[1] = Vab_t_m;
        min_size = size;

        double Vdi_t_0 = -9.17092476461480732e-01;
        double Vab_t_0 = 4.30939618666462410e-01;
        double Vdi_t_1 = -9.17092396198628257e-01;
        double Vab_t_1 = 4.30939238457476059e-01;

        Vab_t_1 = Vab_t_0 + (Vab_t_1 - Vab_t_0) / 10;
        
        Vals v1 = get_fn_fn_t (Vdi_t_0, Vab_t_0, p);
        /* just change Vdi_t */
        Vals v2 = get_fn_fn_t (Vdi_t_1, Vab_t_0, p);
        double dVdi = Vdi_t_1 - Vdi_t_0;
        printf ("df0_dVdit: %g, Δf0/ΔVdi_t: %g\n", v1.df0_dVdit, (v2.f0 - v1.f0) / dVdi);
        printf ("df1_dVdit: %g, Δf1/ΔVdi_t: %g\n", v1.df1_dVdit, (v2.f1 - v1.f1) / dVdi);
        /* just change Vab_t */
        v2 = get_fn_fn_t (Vdi_t_0, Vab_t_1, p);
        double dVab = Vab_t_1 - Vab_t_0;
        printf ("df0_dVabt: %g, Δf0/ΔVab_t: %g\n", v1.df0_dVabt, (v2.f0 - v1.f0) / dVab);
        printf ("df1_dVabt: %g, Δf1/ΔVab_t: %g\n", v1.df1_dVabt, (v2.f1 - v1.f1) / dVab);
        /* change both */
        v2 = get_fn_fn_t (Vdi_t_1, Vab_t_1, p);
        printf ("df0_dVdit * ΔVdi_t + df0_dVabt * ΔVdi_t: %g, Δf0: %g\n",  v1.df0_dVdit * dVdi + v1.df0_dVabt * dVab, v2.f0 - v1.f0);
        printf ("df1_dVdit * ΔVdi_t + df1_dVabt * ΔVdi_t: %g, Δf1: %g\n",  v1.df1_dVdit * dVdi + v1.df1_dVabt * dVab, v2.f1 - v1.f1);
          
      }
    }
    p1 = p2;
  }
}

static void
plot_cross (Params *p, char *filename)
{
  int stepcnt = 1000;
  double Vdi_t_0 = -10;
  double Vdi_t_1 =  10;
  double Vab_t_0 = -10;
  double Vab_t_1 =  10;
  double step = (Vdi_t_1 - Vdi_t_0) / stepcnt;
  FILE *f = fopen (filename, "w");
  for (double Vdi_t = Vdi_t_0; Vdi_t <= Vdi_t_1; Vdi_t += step) {
    p->Vdi_t = Vdi_t;
    double Vab_t_f0 = solve (get_f0, Vab_t_0, Vab_t_1, p);
    p->Vdi_t = Vdi_t;
    double Vab_t_f1 = solve (get_f1, Vab_t_0, Vab_t_1, p);
    //    double VL_t  = (-Vdi_t * A - Vdi_t - Vab_t_0) / A;
    fprintf (f, "%.17e %.17e %.17e %.17e\n", Vdi_t, Vab_t_f0, Vab_t_f1, Vab_t_f0 - Vab_t_f1);
  }
  fclose (f);
  printf ("set xlabel 'Vdi_t L/s'; set ylabel 'Vab_t L/s'; plot 'p' u 1:2 w l t 'f0', 'p' u 1:3 w l t 'f1'\n");
  printf ("set term png; set output 'Vab_t_vs_Vdi_t.png'; replot\n");
  
}

/* finds the roots of the functions specified in f.  Starts looking at
   xio, returns the results in xio */
static int
mrsolve_fdf (FuncSet *f, int n, double *xio, void *params)
{
  gsl_vector *x = gsl_vector_alloc (n);
  for (int i = 0; i < n; i++)
    gsl_vector_set (x, i, xio[i]);

  static gsl_multiroot_fdfsolver *s;
  if (!s)
    s = gsl_multiroot_fdfsolver_alloc (gsl_multiroot_fdfsolver_gnewton, n);
  gsl_multiroot_function_fdf fdf = {f->f, f->df, f->fdf, n, params};

  int status = gsl_multiroot_fdfsolver_set (s, &fdf, x);
  if (!isfinite (gsl_vector_get (s->f, 0)) || !isfinite (gsl_vector_get (s->f, 1))) {
    gsl_vector_free (x);
    return GSL_EBADFUNC;
  }

  FILE *of = NULL;
  if (debug)
    of = fopen ("mrsolve_fdf", "w");

  for (int iter = 0; (status = gsl_multiroot_test_residual (s->f, 1e-5)) == GSL_CONTINUE && iter < 1000; iter++) {
    if (of) fprintf (of, "%g %g -> %g %g\n", gsl_vector_get (s->x, 0), gsl_vector_get (s->x, 1), gsl_vector_get (s->f, 0), gsl_vector_get (s->f, 1));
    if ((status = gsl_multiroot_fdfsolver_iterate (s)) != GSL_SUCCESS)
      break;
    if (debug) printf ("successful iteration\n");
    if (!isfinite (gsl_vector_get (s->f, 0)) || !isfinite (gsl_vector_get (s->f, 1))) {
      gsl_vector_free (x);
      return GSL_EBADFUNC;
    }
  }
  if (of) {
    fprintf (of, "%d: %g %g -> %g %g\n", status, gsl_vector_get (s->x, 0), gsl_vector_get (s->x, 1), gsl_vector_get (s->f, 0), gsl_vector_get (s->f, 1));
    fclose (of);
  }
  
  if (status == GSL_SUCCESS)
    for (int i = 0; i < n; i++)
      xio[i] = gsl_vector_get (s->x, i);

  gsl_vector_free (x);
  return status;
}

/* given Vdi_t, return the value of an expression that should be zero, for use by solve to find the
   value of Vdi_t that make it zero, when the airway is closed */
static double
noflow (const double Vdi_t, void *params)
{
  Params *p = (Params *)params;
  double Vdi   = p->Vdi;
  double Vab   = p->Vab;
  double u     = p->u;
  double Phr_d = p->Phr_d;
  //  double Vab_t = -(A+1) * Vdi_t;
  double Vab_t = -(C1+1) * Vdi_t;

  double VL = get_VL (Vdi, Vab);
  double sigma_rc = get_sigma_rc (get_Vrc (Vdi, Vab), Vdi_t, Vab_t);
  double sigma_ab = get_sigma_ab (u, Vab, Vab_t, 0, 1);
  double sigma_di = get_sigma_di (Phr_d, Vdi, Vdi_t, 0, 1);

  double Pica = get_pica (p, sigma_di);
  double fa = get_fa (Vdi, VL);

  //  printf ("%.17g %.17g %.17g %.17g %.17g\n", VL, Vdi, sigma_di, sigma_ab, sigma_rc);
  //  return -VL / (VL + Vdi) * sigma_di + sigma_ab - sigma_rc; /* see notes091030 */
  return (fa + Fdi - 1) * sigma_di + Pica + sigma_ab - sigma_rc;
}

static inline gsl_vector *
v0 (void)
{
  static gsl_vector *v_t;
  if (!v_t) {
    v_t = gsl_vector_alloc (2);
    gsl_vector_set (v_t, 0, 0);
    gsl_vector_set (v_t, 1, 0);
  }
  return v_t;
}

static double
f_zero_Vdi (const double x, void *params, int fn)
{
  static gsl_vector *f;
  if (!f) f = gsl_vector_alloc (2);
  Params *p = (Params *)params;
  p->Vdi = x;
  v_zero_fdf (v0(), p, f, 0);  
  double fval = gsl_vector_get (f, fn);
  if (!isfinite (fval))
    error (1, 0, "fval = %g, Vrc = %g, fn = %d\n", fval, p->Vdi + get_VL (p->Vdi, p->Vab), fn);
  return fval;
}

static double
f0_zero_Vdi (const double x, void *params)
{
  return f_zero_Vdi (x, params, 0);
}

static double
f1_zero_Vdi (const double x, void *params)
{
  return f_zero_Vdi (x, params, 1);
}

static double global_Vab;

static double
get_Vdi_diff (double Vab, void *params)
{
  bool vb = Vab == 4.38902246093750481 || Vab == 5.25339231493237246 || Vab == 7.35401123046875327 || debug;
  Params *p = (Params *)params;
  double Vdi_lo = 0.1;
  double Vdi_hi = 8.6;
  double Vdi;
  //MAX4 (F (Vdi, dVdi, .1, 7.8), F (Vab, dVab, 1.376, 10.319), F (VL, dVL, .1, 7.8), F (Vrc, dVrc, 6.054, 8.9)); 

  //Vdi = Vsum - Vab - A * 6.054;           /* Vrc_lo */ if (Vdi < Vdi_hi) Vdi_hi = Vdi;
  //Vdi = Vsum - Vab - A * 8.900;           /* Vrc_hi */ if (Vdi > Vdi_lo) Vdi_lo = Vdi;

//  Vdi = Vsum - Vab - A * 1;           /* Vrc_lo */ if (Vdi < Vdi_hi) Vdi_hi = Vdi;
//  Vdi = Vsum - Vab - A * 16;           /* Vrc_hi */ if (Vdi > Vdi_lo) Vdi_lo = Vdi;

//  fprintf (stdout, "Vrc0 = %.17e, limitVrc = %.17e\n", Vrc0, limitVrc);
  double Vrc_lo = nextafter (Vrc_min, Vrc_max);
  double Vrc_hi = nextafter (Vrc_max, Vrc_min);

#define U(x) nextafter (x,  DBL_MAX)
#define D(x) nextafter (x,  -DBL_MAX)

  Vdi = D(Vsum) - U(Vab) - U(Vrc_lo) * U(C1);
  if (Vdi < Vdi_hi) Vdi_hi = Vdi;
  //  fprintf(stdout, "Vdi: %.17e\n", Vdi);
  Vdi = U(Vsum) - D(Vab) - D(Vrc_hi) * D(C1);
  if (Vdi > Vdi_lo) Vdi_lo = Vdi;
//  fprintf(stdout, "Vdi: %.17e\n", Vdi);
//  fprintf(stdout, "Vdi_lo: %.17e, Vdi_hi: %.17e\n", Vdi_lo, Vdi_hi);
//  fprintf(stdout, "Vrc_lo: %.17e, Vrc_hi: %.17e\n", Vrc_lo, Vrc_hi);

//  Vdi = (Vsum - Vab - C1 * 0.1) / (C1 + 1); /* VL_lo */ if (Vdi < Vdi_hi) Vdi_hi = Vdi;
//  Vdi = (Vsum - Vab - C1 * 7.8) / (C1 + 1); /* VL_hi */ if (Vdi > Vdi_lo) Vdi_lo = Vdi;

  double f0_lo = 0;
  double f0_hi = 0;
  double f1_lo = 0;
  double f1_hi = 0;
  double f0_Vdi = 0;
  double f1_Vdi = 0;
  double retval = NAN;
  if (Vdi_lo > Vdi_hi) {
    if (0) {
      printf ("no valid Vdi: Vdi_lo: %g, Vdi_hi: %g\n", Vdi_lo, Vdi_hi);
      printf ("[Vsum = %g, Vab = %g, C1 = %g]\n", Vsum, Vab, C1);
      printf ("%g %g\n", Vsum - Vab - C1 * 16, (Vsum - Vab - C1 * 7.8) / (C1 + 1));
    }
    goto OUT;
  }
  if (vb)  printf ("valid Vdi range at Vab = %g: %g to %g\n", Vab, Vdi_lo, Vdi_hi);
  p->Vab = Vab;

  f0_lo = f0_zero_Vdi (Vdi_lo, p);
  f0_hi = f0_zero_Vdi (Vdi_hi, p);
  if (!(f0_hi * f0_lo <= 0)) {
    if (vb)    printf ("%s line %d: no solution %g %g %g %g %g\n", __FILE__, __LINE__, Vab, Vdi_lo, f0_lo, Vdi_hi, f0_hi);
    goto OUT;
  }

  f1_lo = f1_zero_Vdi (Vdi_lo, p);
  f1_hi = f1_zero_Vdi (Vdi_hi, p);
  if (vb) printf ("f1: Vab = %g, Vdi_lo = %g, Vdi_hi = %g, f1_lo = %g, f1_hi = %g\n", Vab, Vdi_lo, Vdi_hi, f1_lo, f1_hi);
  if (!(f1_hi * f1_lo <= 0)) {
    if (vb)    printf ("%s line %d: no solution %g %g %g %g %g\n", __FILE__, __LINE__, Vab, Vdi_lo, f1_lo, Vdi_hi, f1_hi);
    goto OUT;
  }

  p->Vab = Vab;
  f0_Vdi = solve (f0_zero_Vdi, Vdi_lo, Vdi_hi, p);
  p->Vab = Vab;
  f1_Vdi = solve (f1_zero_Vdi, Vdi_lo, Vdi_hi, p);
  double Vdi_diff = f1_Vdi - f0_Vdi;
  if (!isfinite (Vdi_diff)) {
    if (vb)    printf ("%s line %d: solve failed %g %g %g %g\n", __FILE__, __LINE__, Vdi_lo, Vdi_hi, f0_Vdi, f1_Vdi);
    goto OUT;
  }
  if (fabs (Vdi_diff) < p->Vdi_diff)
    p->Vdi_diff = fabs (Vdi_diff), p->best_Vdi = (f0_Vdi + f1_Vdi) / 2;
  if (vb)  printf ("%s line %d: Vab = %g, Vdi = %g, Vdi_diff = %g\n", __FILE__, __LINE__, Vab, (f0_Vdi + f1_Vdi) / 2, Vdi_diff);
  retval = Vdi_diff;
OUT:
  if (vb)  printf ("Vab: %g, Vdi_diff: %g, f0_Vdi: %g, f1_Vdi: %g, %g %g, %g %g, %g %g\n", Vab, retval, f0_Vdi, f1_Vdi, f0_lo, f0_hi, f1_lo, f1_hi, Vdi_lo, Vdi_hi);
  if (!isfinite (retval))
    global_Vab = Vab;
  if (Vab == 5.2533923149323725)
    printf ("infinite\n");
  return retval;
}

static void
static_solution (Params *p)
{
  double Vab_lo0 =  1.376;
  double Vab_hi0 = 10.319;

  double Vab, Vab_lo = 0, Vab_hi = 0;
  double prev_Vab = 0, prev_Vdi_diff = 0;
  p->Vdi_diff = DBL_MAX;
  
  bool bracket_found = false;
  double solution_Vdi = 0;
  for (double Vab = Vab_lo0; Vab <= Vab_hi0; Vab += (Vab_hi0 - Vab_lo0) / 1000) {
    double Vdi_diff = get_Vdi_diff (Vab, p);
    if (Vdi_diff == 0) {
      if (bracket_found) {
        printf ("line %d\n", __LINE__);
        goto ERR;
      }
      bracket_found = true;
      Vab_lo = Vab_hi = Vab;
      solution_Vdi = p->best_Vdi;
    }
    if (Vdi_diff * prev_Vdi_diff < 0) {
      if (bracket_found) {
        printf ("line %d\n", __LINE__);
        goto ERR;
      }
      bracket_found = true;
      Vab_lo = prev_Vab;
      Vab_hi = Vab;
    }
    prev_Vab = Vab;
    prev_Vdi_diff = Vdi_diff;
  }
  if (!bracket_found) {
    printf ("line %d\n", __LINE__);
    goto ERR;
  }
  if (Vab_lo == Vab_hi) {
    p->Vdi = solution_Vdi;
    p->Vab = Vab_lo;
    return;
  }
  Vab = solve (get_Vdi_diff, Vab_lo, Vab_hi, p);
  printf ("at Vab = %-20.17g, get_Vdi_diff returns %-20.17g\n", Vab, get_Vdi_diff (Vab, p));
  
  p->Vdi = p->best_Vdi;
  p->Vab = Vab;
  return;
 ERR:
  printf ("no static solution: Phr_d %-20.17g, u: %-20.17g, lma: %-20.17g\n",
          p->Phr_d, p->u, p->lma);
  //  fprintf (stdout, "no static solution\n");
  p->Vdi_diff = DBL_MAX;
  
  if (0) {
    printf ("no solution for these params:\n");
    for (int i = 0; i < fitparam_count; i++)
      printf (" %-8.5g", *fitparam[i]);
    printf ("\n");
  }
  return;
}

#ifdef NEWMAIN
time_t global_last_time;
#endif


#ifdef STANDALONE
static double xio_0, xio_1;
#endif

/* calculates the time derivatives of the volumes (f, Vdi_t and Vab_t) from the volumes (y, Vdi and
   Vab), for the ode solver using a multiroot solver */
static int
dVdt (double t, const double y[], double f[], void *params)
{

  static int pass;
  pass++;

  extern time_t global_last_time;
  time_t now;
  if ((now = time (0)) > global_last_time + 1) {
    fprintf (stdout, " %d %.17g\n", pass, t * 2000);
    global_last_time = now;
  }
  
  Params *p = (Params *)params;

  p->Vdi = y[0];
  p->Vab = y[1];
  double VL = get_VL (y[0], y[1]);

  static double xio[2];         /* Vdi_t, Vab_t */

  

#ifdef STANDALONE
  if (0) {
    xio[0] = xio_0;
    xio[1] = xio_1;
  }
#endif

  double xio0[2]; xio0[0] = xio[0]; xio0[1] = xio[1];
  static double t0;

  debug = false;
  Params p0 = *p;
  //  print_params (&p0);
  int status = mrsolve_fdf (&v_zero_all, 2, xio, p);

  double Vdi_t2 = t > t0 ? (xio[0] - xio0[0]) / (t - t0) : 0;
  double Vab_t2 = t > t0 ? (xio[1] - xio0[1]) / (t - t0) : 0;
  double VL_t2 = get_VL_t (Vdi_t2, Vab_t2);

  //  if ((fabs (VL_t2) > 400 || status != GSL_SUCCESS) && isfinite (p->Rrs) && p->Rrs <= Rrs0 * 1e9) { /* p->Rrs is set by v_zero_all */
  if ((status != GSL_SUCCESS) && isfinite (p->Rrs) && p->Rrs <= Rrs0 * 1e8) { /* p->Rrs is set by v_zero_all */
    printf ("pass %d: looking for a solution, VL_t2 = %g, status = %d\n", pass, VL_t2, status);
    dynamic_solutions (p, xio, xio0, t - t0);
    debug = false;
    status = mrsolve_fdf (&v_zero_all, 2, xio, p);
    debug = false;
  }
  //  double Vrc = get_Vrc (y[0], y[1]);
  //  printf ("dVdt %d: %.17f %.17f %.17f %.17f %.17f %d %.17f %.17f %.17f\n", pass, xio0[0], xio0[1], y[0], y[1], Vrc, status, xio[0], xio[1], p->u);

  debug = false;
  
  if (status != GSL_SUCCESS && (!isfinite (p->Rrs) || p->Rrs > Rrs0 * 1e8)) { /* p->Rrs is set by v_zero_all */
    xio[0] = solve (noflow, -1, 1, p);
    xio[1] = -(C1+1) * xio[0];
    status = GSL_SUCCESS;
  }

  void cross_status (char *filename)
  {
    printf ("fail\n");
    printf ("Vrc_min: %.17f\n", Vrc_min);
    printf ("double x0 = %a; /* %.17f */\n", xio0[0], xio0[0]);
    printf ("double x1 = %a; /* %.17f */\n", xio0[1], xio0[1]);
    double Vrc = get_Vrc (p->Vdi, p->Vab);
    printf ("Vrc: %.17e, Vrc_max: %.17e\n", Vrc, Vrc_max);
    printf ("VL: %.17e\n", VL);
    double sigma_di = get_sigma_di (p->Phr_d, p->Vdi, xio[0], NULL, true);
    printf ("sigma_di: %-20.17g\n", sigma_di);
    printf ("sigma_ab: %-20.17g\n", get_sigma_ab (p->u, p->Vab, xio[1], NULL, true));
    printf ("Pab     : %-20.17g\n", get_sigma_ab (p->u, p->Vab, xio[1], NULL, true));
    printf ("sigma_rc: %-20.17g\n", get_sigma_rc (Vrc, xio[0], xio[1]));
    double sigma_L =  get_sigma_L (VL);
    printf ("sigma_L : %-20.17g\n", sigma_L);
    printf ("fa      : %-20.17g\n", get_fa (p->Vdi, VL));
    double VL_t = get_VL_t (xio[0], xio[1]);
    printf ("VL_t    : %-20.17g\n", VL_t);
    double Palv = -VL_t * p->Rrs;
    printf ("Palv    : %-20.17g\n", Palv);
    double Ppl = Palv - sigma_L;
    printf ("Ppl     : %-20.17g\n", Ppl);
    double Pica = get_pica (p, sigma_di);
    printf ("Pica    : %-20.17g\n", Pica);
    double Vrc_t = -(xio[0] + xio[1]) / C1;
    printf ("Vrc_t   : %-20.17g\n", Vrc_t);

    print_params (&p0);
    fprintf (stdout, ("  xio_0 = %a;\n"
                      "  xio_1 = %a;\n"
                      "  const double y[2] = {%a, %a};\n"
                      "  double f[2];\n"
                      "  dVdt (%a, y, f, &p);\n"),
             xio0[0], xio0[1], y[0], y[1], t);
    //   dynamic_solutions (&p0, xio, xio0, t - t0);
    plot_cross (&p0, filename);
    if (1) {
      static_solution (&p0);
      fprintf (stdout, "static solution at Vdi = %g, Vab = %g\n", p0.Vdi, p0.Vab);
      static_balance (&p0);
    }
  }

  //  printf ("at Vdi=%g and Vab=%g, p.Vdi_t = %g; p.Vab_t=%g;\n", y[0], y[1], xio[0], xio[1]);
  
  if (status != GSL_SUCCESS) {
    char *tmp;
    asprintf(&tmp,"%s%s",outPath,"p");
    cross_status (tmp);
    free(tmp);
    error (1, 0, "multiroot solver failed in pass %d at t = %.17g, y[0]: %g, y[1]: %g, VL: %g: status = (%d) %s\n",
           pass, t, y[0], y[1], VL, status, gsl_strerror (status));
  }

  p->Vdi_t = f[0] = xio[0];                /* Vdi_t */
  p->Vab_t = f[1] = xio[1];                /* Vab_t */
  
  t0 = t;
  return 0;
}

/* calculate Rohrer's constants for the larynx as a function of the
   laryngeal muscle activation */
static void
rohrer_constants (Params *p)
{
  double d0 = 10.9;             /* resting diameter of the glottis in mm, see notes100113 */
  double D = 18;                /* tracheal diameter in mm, see notes100112 */
  double d = d0 * (1 + p->lma); /* glottis diameter in mm, lma = pca-ta, ta=1 closes airway by our definition */
  if (d < 0) d = 0;
  if (d > D) d = D;
  double B = d / D;
  double K1 = .153;             /* mm²⋅cmH₂O/(L/s), see notes100114 */
  double K2 = .167;             /* cmH₂O/(L/s)², see notes100113 */
  p->k1 = K1 / (d*d*B*B);       /* see notes100106 */
  p->k2 = K2 * ((1 - B*B) / (B*B*B*B) - (1 - B*B)); /* see notes100105 */
}

static void
update_activation (Motor m, Params *p, double Sstep)
{
  /* RC delays as in Younes&Riddle p. 965 for all the efferents.  See notes100202 */
  double muscle_reaction_time = 60; /* ms, see notes100202 */
  double step_frac = 1 - exp (-Sstep / muscle_reaction_time);
  /* double bs_max_firing_rate = 100;  /\* bulbospinal mean f.r. at max activation, spikes/s/cell: see notes100201 *\/ */
  /* double Phr = m.phrenic / bs_max_firing_rate; */
  double Phr = m.phrenic;
  p->Phr_d_i += (Phr - p->Phr_d_i) * step_frac;
  p->Phr_d = p->Phr_d_i > 1 ? 1 : p->Phr_d_i;
  /* double lumbar_max_firing_rate = 20; /\* spikes/s, abdominal motor units, see Iscoe p. 4 *\/ \ */
  /* double lumbar = m.abdominal / lumbar_max_firing_rate; */
  double lumbar = m.abdominal;
  p->u_i += (lumbar - p->u_i) * step_frac;
  p->u = p->u_i > 1 ? 1 : p->u_i;

  double ic_max_firing_rate = 20;

  double inspic = m.inspic / ic_max_firing_rate;
  p->inspic_i += (inspic - p->inspic_i) * step_frac;
  p->inspic = p->inspic_i > 1 ? 1 : p->inspic_i;

  double expic = m.expic / ic_max_firing_rate;
  p->expic_i += (expic - p->expic_i) * step_frac;
  p->expic = p->expic_i > 1 ? 1 : p->expic_i;

  double lm_reaction_time = 35;     /* layrngeal muscles r.t. in ms, see Luschei p. 2 */
  double lm_step_frac = 1 - exp (-Sstep / lm_reaction_time);

#if 0
  static bool first = true;
  if (first) {
    FILE *f = fopen ("lmmax", "r");
    if (f) {
      if (fscanf (f, "%lf", &lmmfr));
      fclose (f);
    }
    printf ("lm_max_firing_rate set to %g\n", lmmfr);
    first = false;
  }
#endif
  double lm_max_firing_rate = lmmfr;
  
  double lm = (m.pca - m.ta) / lm_max_firing_rate; /* assumes net effect is difference of activations */
  p->lma = p->lma_i += (lm - p->lma_i) * lm_step_frac;
  if (p->lma >  1) p->lma = 1;
  if (p->lma < -1) p->lma = -1;
}

static void
init_ode_solver (gsl_odeiv_step **s, gsl_odeiv_control **c, gsl_odeiv_evolve **e, double *y)
{
  gsl_set_error_handler_off ();
  
  *s = gsl_odeiv_step_alloc (gsl_odeiv_step_gear1, 2);
  *c = gsl_odeiv_control_y_new (1e-6 /*abs err*/, 0.0/*rel err*/);
  *e = gsl_odeiv_evolve_alloc (2);
  init_spline ();
  paramgen ();
  y[0] = VdiFRC;                /* VdiFRC */
  y[1] = VabFRC;                /* VabFRC */
  return;
}

/* return the lung volume after a step, depending on the previous values of the "integrated"
   pre-phrenic, lumbar, and laryngeal motoneurons.  Update the integrated values before
   returning. */
State
lung (Motor m, double Sstep)
{
  /* state - can change every step */
  static Params params;
  static double y[2];  /* = {Vdi0, Vab0}; */
  static double step_size = 1e-6;
  static double t;

  /* one time setup */
  static gsl_odeiv_system sys = {dVdt, NULL/*no Jacobian*/, 2, &params};
  static gsl_odeiv_step *s;
  static gsl_odeiv_control *c;
  static gsl_odeiv_evolve *e;
  if (s == NULL) init_ode_solver (&s, &c, &e, y);
  
  static int pass;
  pass++;

  rohrer_constants (&params);
  Params *p = &params;
  if (0)printf ("Sstep: %g, t = %g\n", Sstep, t);
  for (double t1 = t + Sstep / 1000; t < t1; ) {
    if (0)
    if (step_size < 1e-6)
      error (0, 0, "step_size = %g\n", step_size);
    if (step_size == 0) {
      error_at_line (0, 0, __FILE__, __LINE__, "step size is 0");
      error (0, 0, "Vdi   = %.17g (%.17g to %1.7g)", y[0], VdiRV, VdiTLC);
      error (0, 0, "Vab   = %.17g (%.17g to %1.7g)", y[1], VabRV, VabTLC);
      error (0, 0, "u     = %.17g", p->u);
      error (0, 0, "Phr_d = %.17g", p->Phr_d);
      error (0, 0, "Rrs   = %.17g (Rrs0 = %g)", p->Rrs, Rrs0);
      error (0, 0, "VL    = %.17g (%.17g to %1.7g)", get_VL (y[0], y[1]), VLRV, VLTLC);
      error (0, 0, "Vrc   = %.17g (%.17g to %1.7g)", get_Vrc (y[0], y[1]), VrcRV, VrcTLC);
      exit (1);
    }
      
    if (0)
      printf ("p.Vdi=%g; p.Vab=%g; p.u=%g; p.Phr_d=%g; p.k1=%g; p.k2=%g;\n",
              y[0], y[1], p->u, p->Phr_d, p->k1, p->k2);
    if (step_size > Sstep)
      step_size = Sstep;
    if ((p->u || p->Phr_d) && step_size > Sstep / 10)
      step_size = Sstep / 10;
    
    if (0) printf ("t: %g, %g\n", t, step_size);

    double Vrc = get_Vrc (p->Vdi, p->Vab);
    double Vrc_t = -(p->Vdi_t + p->Vab_t) / C1;
    if (Vrc > .9 * Vrc_max && Vrc_t > 0) {
      double Prc = get_sigma_rc (Vrc, p->Vdi_t, p->Vab_t);
      double Vrc2 = Vrc_min + (Vrc_max - Vrc_min) / (1 + exp (Prc_div * (Prc + .001 - Prc_add)));
      double ss_max = (Vrc2 - Vrc) / Vrc_t;
      //      printf ("step_size: %g, ss_max = %g, Vrc = %g, Prc = %g\n", step_size, ss_max, Vrc, get_sigma_rc (Vrc, p->Vdi_t, p->Vab_t));
      if (step_size > ss_max)
        step_size = ss_max;
      /*
      double Prc_t = -Vrc_t * (1 / (Vrc_max - Vrc) + 1 / (Vrc - Vrc_min)) / Prc_div;
      double ss_max = .01 / Prc_t;
      if (Prc_t)
        printf ("Prc_t = %g, step_size: %g, ss_max = %g, Vrc = %g, Prc = %g\n", Prc_t, step_size, ss_max, Vrc, get_sigma_rc (Vrc));
      if (Prc_t != 0 && step_size > ss_max)
        step_size = ss_max;
      */
    }
    if (gsl_odeiv_evolve_apply (e, c, s, &sys, &t, t1, &step_size, y) != GSL_SUCCESS)
      error (1, 0, "%s", "ODE solver failed");
    
  }

  update_activation (m, &params, Sstep);

  //  error (0, 0, "%d: %8.5f %8.5f %8.5f %11.8f", pass, y[0], y[1], get_VL (y[0], y[1]), step_size);

  if (pass % 200 == 200)
    printf ("%g: Phr: %g, Abd: %g, Vol: %g\n", t, p->Phr_d, p->u, get_VL (y[0], y[1]));

  double VL = get_VL (y[0], y[1]);
  double VL_t = get_VL_t (e->dydt_out[0], e->dydt_out[1]);
  double sigma_di = get_sigma_di (params.Phr_d, y[0], e->dydt_out[0], 0, 1);
  double sigma_ab = get_sigma_ab (params.u, y[1], e->dydt_out[1], 0, 1);
  double sigma_L = get_sigma_L (VL);
  State st;
  st.pressure = (sigma_ab + sigma_L - sigma_di) / 2.78; /* cmH2O, 2.78 scales for a cat - see notes100304 */
  st.volume = (VL - VL0) / VC * 100;           /* %VC relative to RV */
  st.flow = -VL_t / VC * 100;                   /* %VC / sec */

  st.Phr_d = params.Phr_d_i;
  st.u = params.u_i;
  st.lma = params.lma_i;
  st.Vdi = y[0];
  st.Vab = y[1];
  st.Vdi_t = e->dydt_out[0];
  st.Vab_t = e->dydt_out[1];
  st.Pdi = sigma_di;
  st.Pab = sigma_ab;
  st.PL = sigma_L;

  double Pica = get_pica (p, sigma_di);
#ifdef INTERCOSTALS
  Pica = Pdirc * p->inspic + intic_boost * Pabrc * p->expic;
#endif
  double Vrc_t = -(st.Vdi_t + st.Vab_t) / C1;
  double Palv = sigma_ab + sigma_L - sigma_di;
  
  if (0)
  printf ("%10d, Phr_d: %8.6f, u: %8.6f, lma: %9.6f, Vdi: %7.5f, Vab: %7.5f, Vrc: %7.5f, Vdi_t: %9.6f, Vab_t: %9.6f, Vrc_t: %9.6f, Pdi: %7.5f, Palv: %7.5f, Prc: %7.5f, Pica: %8.5f, Pab: %8.5f, PL: %7.5f, VL: %7.5f, VL_t: %8.5f, Rrs: %g\n",
          pass, st.Phr_d, st.u, st.lma, st.Vdi, st.Vab, get_Vrc (st.Vdi, st.Vab), st.Vdi_t, st.Vab_t, Vrc_t, st.Pdi, Palv, get_sigma_rc (get_Vrc (st.Vdi, st.Vab), st.Vdi_t, st.Vab_t), Pica, st.Pab, st.PL, VL, VL_t, p->Rrs);

  return st;
}

#ifdef STANDALONE

static void
Vdi_vs_Vab (double Vab_0)
{
  int N = 100;
  double Vab_1 = 10.3191;
  double dVab = (Vab_1 - Vab_0) / N; /* liters */
  double Vdi_0 = 1.411;
  double Vdi_1 = 4.644;
  double dVdi = (Vdi_1 - Vdi_0) / N;

  if (0) {
    Vdi_0 = 7;
    Vdi_1 = 8;
    Vab_0 = 3;
    Vab_1 = 4;
    dVab = (Vab_1 - Vab_0) / N; /* liters */
    dVdi = (Vdi_1 - Vdi_0) / N;
    //The solution is at about 7.58317, 3.86168
    //  double factor = MAX4 (F (Vdi, dVdi, .1, 7.8), F (Vab, dVab, 1.376, 10.319), F (VL, dVL, .1, 7.8), F (Vrc, dVrc, 6.054, 8.9)); 
  }

  Params p;
  p.lma = 0;
  rohrer_constants (&p);
  p.u = 1;
  p.Phr_d = 0;
  p.Rrs = Rrs0;

  if (0) {
    gsl_vector *x = gsl_vector_alloc (2);
    gsl_vector *f = gsl_vector_alloc (2);
    gsl_vector_set (x, 0, 0);
    gsl_vector_set (x, 1, 0);
    for (double Vab = Vab_0; Vab <= Vab_1; Vab += dVab) {
      for (double Vdi = Vdi_0; Vdi <= Vdi_1; Vdi += dVdi) {
        p.Vdi = Vdi;
        p.Vab = Vab;
        v_zero_fdf (x, &p, f, 0);
        double f0 = gsl_vector_get (f, 0);
        double f1 = gsl_vector_get (f, 1);
        printf ("%g %g %g\n", Vdi, Vab, fabs (f0) + fabs (f1));
      }
    }
    exit (0);
  }

  if (0) {
    double Vdi_start = 0x1.6af383f0bae89p+3;
    double Vdi_stop  = 0x1.6af383f0bae8ap+3;
    p.Vab = 3.635774;
    for (double Vdi = Vdi_start; Vdi <= Vdi_stop; Vdi += (Vdi_stop - Vdi_start))
      printf ("Vdi: %-20.17g (%-20a), Vab: %-11g, f0: %-11g\n", Vdi, Vdi, p.Vab, f0_zero_Vdi (Vdi, &p));
    exit (0);

    double Vdi_a = solve (f0_zero_Vdi, 11.3421, 11.3423, &p);
    p.Vab = 3.63636 ; double Vdi_b = solve (f0_zero_Vdi,  8.5985,  8.5987, &p);
    printf ("%g %g\n", Vdi_a, Vdi_b);

    exit (0);
  }

  for (double Vab = Vab_0; Vab <= Vab_1; Vab += dVab) {
    p.Vab = Vab;
    double f0_Vdi = solve (f0_zero_Vdi, VdiFRC - 1, VdiFRC, &p);
    double f1_Vdi = solve (f1_zero_Vdi, VdiFRC - 1, VdiFRC, &p);
    printf ("%g %g %g\n", Vab, f0_Vdi, f1_Vdi);
  }
  fprintf (stdout, "set xlabel \"Vab\"; plot \"a\" u 1:2 w l t \"f0_Vdi\", \"a\" u 1:3 w l t \"f1_Vdi\", 0\n");
  exit (0);
}

static double
static_VL (Params *p)
{
  static_solution (p);
  if (p->Vdi_diff > 1e-5)
    return 20;
  return get_VL (p->Vdi, p->Vab);
}

static double
get_TLC (double *Vrcp)
{
  Params p;

  p.lma = 0;
  rohrer_constants (&p);
  p.u = 0       ; p.Phr_d = 1       ; double VL0 = static_VL (&p);
  if (Vrcp)
    *Vrcp = VL0 + p.Vdi;
  if (0) {
    double VL;
    printf ("TLC: Vdi: %7.5f, VL: %7.5f, Vrc: %7.5f, Vab: %7.5f, ", p.Vdi, VL = get_VL (p.Vdi, p.Vab), get_Vrc (p.Vdi, p.Vab), p.Vab);
    printf ("Pab: %8.4f, DPab: %8.4f, Pdi: %8.4f, PL: %8.4f, Prc: %8.4f\n", get_sigma_ab (0, p.Vab, 0, NULL, true) + DPab, DPab, get_sigma_di (1, p.Vdi, 0, NULL, true), get_sigma_L (VL), get_sigma_rc (get_Vrc (p.Vdi, p.Vab), 0, 0));
  }
  //  return VL0;
  p.u = 0.000001; p.Phr_d = 1       ; double VLu = static_VL (&p);
  p.u = 0       ; p.Phr_d = 0.999999; double VLp = static_VL (&p);
  if (1)
    if (VLu >= VL0 || VLp >= VL0)
      error (1, 0, "%s", "TLC max is not at corner");
  return VL0;
}

static double
get_RV (double *Vrcp)
{
  Params p;

  p.lma = 0;
  rohrer_constants (&p);
  p.u = 1       ; p.Phr_d = 0       ; double VL0 = static_VL (&p);
  if (Vrcp)
    *Vrcp = VL0 + p.Vdi;
  if (0) {
    double VL;
    printf (" RV: Vdi: %7.5f, VL: %7.5f, Vrc: %7.5f, Vab: %7.5f, ", p.Vdi, VL = get_VL (p.Vdi, p.Vab), get_Vrc (p.Vdi, p.Vab), p.Vab);
    printf ("Pab: %8.4f, DPab: %8.4f, Pdi: %8.4f, PL: %8.4f, Prc: %8.4f\n", get_sigma_ab (1, p.Vab, 0, NULL, true) + DPab, DPab, get_sigma_di (0, p.Vdi, 0, NULL, true), get_sigma_L (VL), get_sigma_rc (get_Vrc (p.Vdi, p.Vab), 0, 0));
  }
  //  return VL0;
  //  printf ("Vdi: %g, VL: %g, Vrc: %g, Vab: %g\n", p.Vdi, get_VL (p.Vdi, p.Vab), p.Vdi + get_VL (p.Vdi, p.Vab), p.Vab);
  p.u = 0.999999; p.Phr_d = 0       ; double VLu = static_VL (&p);
  p.u = 1       ; p.Phr_d = 0.000001; double VLp = static_VL (&p);
  //  error (0, 0, "RV min is not at corner: VL0 = %.17e, VLu = %.17e, VLp = %1.7e", VL0, VLu, VLp);
  if (1)
    if (VLu <= VL0 || VLp <= VL0)
      error (1, 0, "RV min is not at corner: VL0 = %.17e, VLu = %.17e, VLp = %.17e", VL0, VLu, VLp);
  return VL0;
}

static double
get_FRC (double *Vrcp, double *PLp)
{
  Params p;

  p.lma = 0;
  rohrer_constants (&p);
  p.u = 0       ; p.Phr_d = 0       ; double VL0 = static_VL (&p);
  if (Vrcp)
    *Vrcp = VL0 + p.Vdi;
  if (PLp)
    *PLp = get_sigma_L (get_VL (p.Vdi, p.Vab));
  if (0) {
    double VL;
    printf ("FRC: Vdi: %7.5f, VL: %7.5f, Vrc: %7.5f, Vab: %7.5f, ", p.Vdi, VL = get_VL (p.Vdi, p.Vab), get_Vrc (p.Vdi, p.Vab), p.Vab);
    printf ("Pab: %8.4f, DPab: %8.4f, Pdi: %8.4f, PL: %8.4f, Prc: %8.4f\n", get_sigma_ab (0, p.Vab, 0, NULL, true) + DPab, DPab, get_sigma_di (0, p.Vdi, 0, NULL, true), get_sigma_L (VL), get_sigma_rc (get_Vrc (p.Vdi, p.Vab), 0, 0));
  }

  return VL0;
}

typedef struct
{
  double ab;
  double rc;
  double p;
} KM;

KM km[] = {
  {-16.9053, 0.594322, 2.06992},
  {5.19841, 0.0954277, 0.674665},
  {-16.5922, 11.1745, 1.48868},
  {-0.819001, 11.1593, 0.70795},
  {5.00382, 10.2336, 0.32042},
  {-16.6551, 23.2755, 0.938408},
  {-2.03091, 22.7518, 0.461199},
  {6.12394, 22.4888, 0.306365},
  {-15.8066, 0.454873, 1.68241},
  {-15.8396, 12.882, 1.34192},
  {3.8579, 11.5665, 0.609882},
  {-15.5705, 36.9903, 1.13015},
  {5.14628, 36.0162, 0.263409},
  {-16.4577, -0.0338133, 2.27192},
  {-8.12899, -0.164875, 1.75787},
  {-16.6406, 10.5461, 2.09714},
  {-8.00307, 10.3792, 1.39856},
  {-0.172183, 10.1336, 0.864575},
  {-16.342, 24.6648, 2.03118},
  {-7.79991, 25.0425, 1.21826},
  {-0.0913384, 23.8556, 0.623069},
  {-20.0983, -0.032006, 2.46399},
  {-9.39287, -0.895013, 1.75568},
  {-20.1177, 19.2304, 2.06268},
  {-9.37503, 18.7777, 1.24606},
  {0.324264, 18.548, 0.70842},
  {-20.055, 38.9264, 1.61531},
  {-8.98782, 38.3543, 0.914375},
  {0.420388, 38.1147, 0.491225}
};

static double
grassino (double VC)
{
  Params p;
  p.lma = 0;
  rohrer_constants (&p);
  p.u = 0;
  p.Phr_d = 0;
  static_solution (&p);
  if (p.Vdi_diff > 1e-5)
    return 20;
  double VdiFRC = p.Vdi;
  double VabFRC = p.Vab;
  double VLFRC = get_VL (p.Vdi, p.Vab);
  double VrcFRC = VdiFRC + VLFRC;
  double p0 = get_sigma_di (.3, VdiFRC, 0, 0, 1);
  double sum = 0;
  for (int i = 0; i < 29; i++) {
    double Vab = VabFRC + km[i].ab / 100 * VC;
    double Vrc = VrcFRC + km[i].rc * A / (A + 1) / 100 * VC;
    double Vdi = Vsum - Vrc * A - Vab;
    double pr = get_sigma_di (.3, Vdi, 0, 0, 1) / p0;
    sum += pow (pr - km[i].p, 2);
  }
  return sum;
}

typedef struct
{
  int fitparam_count;
  double **fitparam;
  double (*ferr) (double **, int);
} FitParam;

static double minsum = DBL_MAX;
static double deviation[sizeof fitparam / sizeof fitparam[0] + 6];
static double grassino_sum, opt_RV, opt_VC, opt_VrcRV, opt_VrcTLC, opt_FRC, opt_TLC, opt_VrcFRC, opt_PL;

double
ferr (double **param, int param_count)
{
  double VrcRV, VrcTLC, VrcFRC, PL;
  double TLC = get_TLC (&VrcTLC);
  double RV = get_RV (&VrcRV);
  double FRC = get_FRC (&VrcFRC, &PL);
  double VC = TLC - RV;
  int di = 0;
  double dev[sizeof fitparam / sizeof fitparam[0] + 6];
  for (di = 0; di < fitparam_count; di++)
    dev[di] = (*param[di] - initparam[di]) / (.1 * initparam[di]);
  if (0) {
    dev[di++] = (RV - 1.41) /  .141;
    dev[di++] = (VC - 5.37) /  .0537;
    dev[di++] = (VrcRV - 6.054) /  .6054;
  }
  dev[di++] = (FRC - 2.29) /  .229;
  dev[di++] = (TLC - 6.78) /  .678;
  dev[di++] = (VrcFRC - 6.723) /  .6723;
  dev[di++] = (VrcTLC - 8.191) /  .8191;
  dev[di++] = (PL - 3) /  .3;

  dev[Fdi_] = 0;
  dev[A_] = 0;
  dev[Vsum_] = 0;
  dev[limitVrc_] = 0;
  dev[Cab_] = 0;
  dev[Ldi_min_] = 0;


  //  di = Vrc0_; dev[di] = (*param[di] - initparam[di]) / (.05 * initparam[di]);

  double sum = 0;
  for (int i = 0; i < fitparam_count + 5; i++)
    sum += pow (dev[i], 2);
  double gsum = grassino (VC);
  sum += gsum;

  if (sum < minsum) {
    minsum = sum;
    memcpy (deviation, dev, sizeof deviation);
    grassino_sum = gsum;
    opt_RV = RV;
    opt_VC = VC;
    opt_VrcRV = VrcRV;
    opt_VrcTLC = VrcTLC;
    opt_FRC = FRC;
    opt_TLC = TLC;
    opt_VrcFRC = VrcFRC;
    opt_PL = PL;
    //    printf ("sum: %g\n", sum);
  }
  return sum;
}

double
fit_error (const gsl_vector *v, void *params)
{
  FitParam *p = (FitParam *)params;
  for (int i = 0; i < v->size; i++)
    *p->fitparam[i] = gsl_vector_get (v, i);
  return p->ferr (p->fitparam, p->fitparam_count);
}

static int
multimin (double (*ferr) (double **, int), double **fitparam, int fitparam_count)
{
  FitParam p = {fitparam_count, fitparam, ferr};

  gsl_vector *x0 = gsl_vector_alloc (fitparam_count);
  for (int i = 0; i < fitparam_count; i++)
    gsl_vector_set (x0, i, *fitparam[i]);

  gsl_vector *dx = gsl_vector_alloc (fitparam_count);
  for (int i = 0; i < fitparam_count; i++)
    gsl_vector_set (dx, i, initparam[i] / 5);

  gsl_multimin_function minex_func;
  minex_func.n = fitparam_count;
  minex_func.f = fit_error;
  minex_func.params = (void *)&p;

  gsl_multimin_fminimizer *s = gsl_multimin_fminimizer_alloc (gsl_multimin_fminimizer_nmsimplex, fitparam_count);
  gsl_multimin_fminimizer_set (s, &minex_func, x0, dx);
  int status;
  int iter;
  for (iter = 0; (status = gsl_multimin_test_size (s->size, 1e-5)) == GSL_CONTINUE && iter < 20000; iter++) {
    if ((status = gsl_multimin_fminimizer_iterate (s)) != GSL_SUCCESS)
      break;
    if (iter % 100 == 0)
      printf ("%d size: %g, ferr: %g\n", iter, s->size, gsl_multimin_fminimizer_minimum (s));
  }
  printf ("iter: %d\n", iter);

  if (status != GSL_SUCCESS)
    error (1, 0, "multimin failed: %s", gsl_strerror (status));

  for (int i = 0; i < fitparam_count; i++)
    *fitparam[i] = gsl_vector_get (s->x, i);

  gsl_vector_free(x0);
  gsl_vector_free(dx);
  gsl_multimin_fminimizer_free (s);

  return iter;
}

int
main (void)
{
  double Vab_0 = 0;
  if (0) {
    for (int i = 0; i < fitparam_count; i++)
      initparam[i] = *fitparam[i];
    Vab_0 = init_spline ();
    fprintf (stdout, "start\n");
    paramgen ();
  }

  Motor motor (double t) {
    Motor m;
    if (t < 5) {
      m.phrenic = 1;
      m.abdominal = 0;
      m.pca = 1;
      m.ta = 0;
    }
    else if (t < 10) {
      m.phrenic = 1;
      m.abdominal = 0;
      m.pca = 0;
      m.ta = 1;
    }
    else if (t < 15) {
      m.phrenic = 0;
      m.abdominal = 1;
      m.pca = 0;
      m.ta = 1;
    }
    else {
      m.phrenic = 0;
      m.abdominal = 1;
      m.pca = 1;
      m.ta = 0;
    }
  }
  for (double t = 0; t < 20; t++) {
    State s = lung (motor (t), .5);
    printf ("%g %g\n", s.flow, s.volume);
  }
  exit (0);

  while (true)
    lung ((Motor) {0,0,0,0}, .5);
    //lung ((Motor) {1,20,0,0}, .5);
  //lung ((Motor) {100,0.1,0,0}, .5);

  if (1)
    {
      gsl_vector *x = gsl_vector_alloc (2);
      gsl_vector *f = gsl_vector_alloc (2);
      gsl_matrix *j = gsl_matrix_alloc (2, 2);

      double x0 = 0x1.452fc2bcb2195p+0; /* 1.27026002032116492 */
      double x1 = -0x1.21ee5697d5978p+0; /* -1.13254300314801348 */


      gsl_vector_set (x, 0, x0);
      gsl_vector_set (x, 1, x1);

      Params p;

      p.Vdi   = 0x1.a663e607e186ep+1 ; /* 3.2999236620408317     */
      p.Vab   = 0x1.0362d29ca4021p+1 ; /* 2.0264533295485454     */
      p.u     = 0x1.f9f8751c3ebeap-1 ; /* 0.98822370500363133    */
      p.Phr_d = 0x0p+0               ; /* 0                      */
      p.lma   = 0x0p+0               ; /* 0                      */
      p.k1    = 0x1.cc4ca32b5c82ep-9 ; /* 0.0035118054562923917  */
      p.k2    = 0x1.5c8cd47b4220fp-1 ; /* 0.68076194767587761    */
      p.Rrs   = 0x1.4860eeb1e4ecp+1  ; /* 2.5654581421493674     */
      p.Vdi_t = 0x1.452fc2bcb2195p+0 ; /* 1.2702600203211649     */
      p.Vab_t = -0x1.21ee5697d5978p+0; /* -1.1325430031480135    */

      v_zero_fdf (x, &p, f, j);
      double f0 = gsl_vector_get (f, 0);
      double f1 = gsl_vector_get (f, 1);
      double df0_dVdit = gsl_matrix_get (j, 0, 0);
      double df0_dVabt = gsl_matrix_get (j, 0, 1);
      double df1_dVdit = gsl_matrix_get (j, 1, 0);
      double df1_dVabt = gsl_matrix_get (j, 1, 1);
      printf ("f0 = %.17e\n", f0);
      printf ("f1 = %.17e\n", f1);
      printf ("df0_dVdit = %.17e\n", df0_dVdit);
      printf ("df0_dVabt = %.17e\n", df0_dVabt);
      printf ("df1_dVdit = %.17e\n", df1_dVdit);
      printf ("df1_dVabt = %.17e\n", df1_dVabt);
      if (1) {
        double dVdit = (f1*df0_dVabt-f0*df1_dVabt) /(df0_dVdit*df1_dVabt-df0_dVabt*df1_dVdit);
        double dVabt = -(f1*df0_dVdit-f0*df1_dVdit) /(df0_dVdit*df1_dVabt-df0_dVabt*df1_dVdit);
        gsl_vector_set (x, 0, x0 + dVdit);
        gsl_vector_set (x, 1, x1 + dVabt);
        printf ("new Vdi_t: %.17e\n", x0 + dVdit);
        printf ("new Vab_t: %.17e\n", x1 + dVabt);
        v_zero_fdf (x, &p, f, j);
        double f0 = gsl_vector_get (f, 0);
        double f1 = gsl_vector_get (f, 1);
        printf ("f0 = %.17e\n", f0);
        printf ("f1 = %.17e\n", f1);
        exit (0);
      }
      double dxt = .00000001;
      fprintf (stdout, "dVdit = %g:\n", dxt);
      gsl_vector_set (x, 0, x0 + dxt);
      v_zero_fdf (x, &p, f, j);
      double f0d = gsl_vector_get (f, 0);
      double f1d = gsl_vector_get (f, 1);
      fprintf (stdout, ("df0_dVdit = %.17f\n"
                        "         vs %.17f\n"), (f0d - f0) / dxt, df0_dVdit);
      fprintf (stdout, ("df1_dVdit = %.17f\n"
                        "         vs %.17f\n"), (f1d - f1) / dxt, df1_dVdit);

      fprintf (stdout, "dVabt = %g:\n", dxt);
      gsl_vector_set (x, 0, x0);
      gsl_vector_set (x, 1, x1 + dxt);
      v_zero_fdf (x, &p, f, j);
      double f0a = gsl_vector_get (f, 0);
      double f1a = gsl_vector_get (f, 1);
      fprintf (stdout, ("df0_dVabt = %.17f\n"
                        "         vs %.17f\n"), (f0a - f0) / dxt, df0_dVabt);
      fprintf (stdout, ("df1_dVabt = %.17f\n"
                        "         vs %.17f\n"), (f1a - f1) / dxt, df1_dVabt);
    
      exit (0);
    }


  if (0) {
    printf ("default params:\n");
    for (int i = 0; i < fitparam_count; i++)
      printf (" %-8.5g", *fitparam[i]);
    printf ("\n");
  }

  if (0) {
    double bad[] = {266.48,6.5336,17.035,34.321,16.593,6.133,1.0308,35.319,0.043548,24.191,0.1852,1.3093,0.10112,7.7156,0.3963,14.948};

    //  double bad[] = {254.43, 6.5257, 16.898, 33.458, 17.382, 5.9212, 1.0383, 35.071, 0.043983, 24.727, 0.18808, 1.3475, 0.10095, 7.6179, 0.40101, 14.978};
    //  double good[] = {252.61, 6.4712, 16.939, 33.456, 17.472, 5.916, 1.0328, 35.101, 0.043981, 24.991, 0.1887, 1.3484, 0.1012, 7.6304, 0.40245, 14.907};
    //double bad[] =  {252.61, 6.4712, 16.939, 33.456, 17.4520048828125, 5.916, 1.0328, 35.101, 0.043981, 24.991, 0.1887, 1.3484, 0.1012, 7.6304, 0.40245, 14.907};
    //    double bad[] =  {252.61, 6.4712, 16.939, 33.456, 17.4519609375, 5.916, 1.0328, 35.101, 0.043981, 24.991, 0.1887, 1.3484, 0.1012, 7.6304, 0.40245, 14.907};
    //      double bad[] = {254.43, 6.5257, 16.898, 33.458, 17.382, 5.9212, 1.0383, 35.071, 0.043983, 24.727, 0.18808, 1.3475, 0.1012, 7.6304, 0.40245, 14.907};
    for (int i = 0; i < fitparam_count; i++)
      *fitparam[i] = bad[i];
    fprintf (stdout, "RV: %g\n", get_RV (0));
    //Vdi_vs_Vab (Vab_0);
    return 0;
  }

  //  double good[] = {265.78, 6.7507, 16.71, 34.513, 19.296, 6.2152, 1.0379, 34.847, 0.040117, 27.435, 0.21592, 1.432, 0.11902, 7.9843, 0.32446, 15.275};
  //  double good[] = {264.61, 6.7509, 16.695, 34.424, 19.24, 6.1813, 1.0377, 34.688, 0.040216, 27.576, 0.21745, 1.432, 0.1194, 7.976, 0.32357, 15.252};
  //  double good[] = {267.23, 6.7448, 16.689, 34.546, 19.599, 6.2431, 1.0581, 34.73, 0.041946, 27.635, 0.223, 1.4333, 0.11891, 7.9344, 0.33696, 15.447};
  //  double good[] = {267.23, 6.7447, 16.688, 34.549, 19.599, 6.2432, 1.0581, 34.729, 0.041941, 27.637, 0.22301, 1.4336, 0.1189, 7.9347, 0.33691, 15.447};
  //  double good[] = {261.26, 6.8717, 16.648, 35.05, 19.431, 6.1401, 1.0729, 34.49, 0.043999, 27.687, 0.22228, 1.432, 0.11707, 7.8912, 0.33904, 15.634};
  //  double good[] = {262.51, 6.8882, 16.562, 35.162, 19.224, 6.2029, 1.0589, 34.82, 0.043602, 27.696, 0.22283, 1.438, 0.11885, 7.9685, 0.35478, 15.742};
  //  double good[] = {262.98, 6.8889, 16.613, 35.05, 19.271, 6.2118, 1.0596, 34.586, 0.043734, 27.659, 0.22266, 1.4224, 0.11845, 7.9848, 0.35275, 15.74};
  //  double good[] = {263.02, 6.888, 16.648, 35.045, 19.269, 6.2057, 1.0597, 34.583, 0.043707, 27.705, 0.2227, 1.4226, 0.11845, 7.9755, 0.35277, 15.741};
  //  double good[] = {248.55, 6.7211, 16.877, 35.153, 18.226, 6.0649, 1.0652, 34.7, 0.045384, 27.507, 0.21288, 1.4347, 0.11203, 8.0295, 0.37356, 15.784};
  //  double good[] = {238.09, 5.7165, 17.105, 34.822, 17.783, 6.0554, 1.0552, 34.7, 0.046274, 27.507, 0.20782, 1.4361, 0.10708, 8.1087, 0.40062, 14.945};
  //  double good[] = {238.1, 5.7163, 17.104, 34.822, 17.783, 6.0553, 1.0552, 34.7, 0.046275, 27.507, 0.20783, 1.4361, 0.10708, 8.1083, 0.40063, 14.945};
  //  double good[] = {255.46, 5.3388, 16.989, 34.368, 19.331, 6.1622, 1.0414, 34.7, 0.043642, 27.587, 0.21848, 1.427, 0.11361, 7.6181, 0.38202, 14.325};
  //  double good[] = {242.82, 4.425, 17.101, 34.387, 18.433, 6.1715, 1.0432, 2.2000, 0.045556, 27.625, 0.212, 1.4259, 0.10801, 7.5499, 0.41047, 13.631};
  //  double good[] = {255.34, 5.0007, 17.103, 33.604, 17.675, 5.4604, 1.0181, 2.2413, 0.037981, 26.887, 0.23864, 1.4074, 0.10669, 9.414, 0.38326, 16.022};
  //  double good[] = {255.34, 5.0007, 0, 33.604, 17.675, 5.4604, 1.0181, 2.2413, 0.037981, 26.887, 0.23864, 1.4074, 0.10669, 9.414, 0.38326, 16.022};
  //               Pdimax   Vdi0   Ldi_min  FCEmax   LCE0    Vab0     th   limitVrc   Cab      DPab     CL      VL0     Crc     Vrc0      A      Vsum Fdi      RV       VC       VrcRV    VrcTLC   grassino
  //  double good[] = {232.25, 6.2645, 0.81042, 33.521, 18.064, 5.3211, 1.0152, 2.0296, 0.046071, 26.692, 0.20988, 1.2799, 0.1024, 7.8435, 0.43544, 13.39, 0};
  //  double good[] = {229.87175, 6.29193, 0.31718, 33.00765, 19.24986, 5.53896, 1.00290, 29.76621, 0.08195, 26.91791, 0.20717, 1.49841, 0.10255, 6.76216, 0.65782, 16.03696, 2.32122};
  //  double good[] = {229.87175, 6.29193, 0.31718, 33.00765, 19.24986, 5.53896, 1.00290, 29.76621, 0.08195, 26.91791, 0.20717, 1.49841, 0.10255, 6.76216, 0.65782, 16.03696, 2.32122};
  double good[] = {0x1.caf836ac97245p+7, 0x1.95224503963efp+2, 0x1.1a8bd1b1205ep-2, 0x1.07aa740e67e5ap+5, 0x1.35d96797eb258p+4, 0x1.682474a0fa5dap+2, 0x1.fddee8fdfd73fp-1, 0x1.329dce34d4a44p+5, 0x1.6962a2ac8d313p-4, 0x1.b18f36c13ef15p+4, 0x1.a5cfe55bfa235p-3, 0x1.7f6f86834596cp+0, 0x1.a3fe6fc3a2d28p-4, 0x1.b84dba8bec71bp+2, 0x1.bd7966cf6c26ap-1, 0x1.1e2b1f2d6c975p+4, 0x1.87437437c3c32p+1};

  if (1)
    {
      //    double good[] = {0x1.09c68214091e9p+8,0x1.b00b2456a49bcp+2,0x1.0b5ba55fd372cp+4,0x1.141ab0fa24846p+5,0x1.34be00912ba6p+4,0x1.8dc6190c66eccp+2,0x1.09b2725e5a737p+0,0x1.16c6c4a4e147bp+5,0x1.48a3d24e9c776p-5,0x1.b6f4c5d4d64e5p+4,0x1.ba343b5623d6p-3,0x1.6e98f69b1ac8cp+0,0x1.e77c955f75279p-4,0x1.feff5fe2d298ep+2,0x1.4c3fbaabf3f3cp-2,0x1.e8cb95f26e632p+3};
      for (int i = 0; i < fitparam_count; i++)
        *fitparam[i] = good[i];
    }


  if (1) {
    double Vrc;
    get_RV (0);
    get_FRC (0, 0);
    get_TLC (0);
    return 0;
    printf ("RV: %g\n", get_RV (&Vrc));
    printf ("VrcRV: %g\n", Vrc);
    printf ("TLC: %g\n", get_TLC (&Vrc));
    printf ("VrcTLC: %g\n", Vrc);
    return 0;

    Vdi_vs_Vab (Vab_0);
    return 0;
  }

  if (0) {
    printf ("TLC: %g\n", get_TLC (0));
  }

  while (multimin (ferr, fitparam, fitparam_count) > 0) {
    printf ("sum: %g\n", ferr (fitparam, fitparam_count));

    for (int i = 0; i < fitparam_count; i++)
      printf (" %-8s", paramname[i]);
    printf (" %-8s %-8s %-8s %-8s %-8s %-8s\n", "FRC", "TLC", "VrcFRC", "VrcTLC", "PL", "grassino");

    for (int i = 0; i < fitparam_count; i++)
      printf (" %-8.5g", initparam[i]);
    //    printf (" %-8.5g %-8.5g %-8.5g %-8.5g\n", 1.41, 5.37, 6.054, 8.191);
    printf (" %-8.5g %-8.5g %-8.5g %-8.5g\n", 2.29, 6.78, 6.723, 8.191);

    for (int i = 0; i < fitparam_count; i++)
      printf (" %-8.5f", *fitparam[i]);
    printf (" %-8.5f %-8.5f %-8.5f %-8.5f %-8.5f\n", opt_FRC, opt_TLC, opt_VrcFRC, opt_VrcTLC, opt_PL);

    for (int i = 0; i < fitparam_count; i++)
      printf (" %-8.5f", good[i]);
    printf ("\n");

    for (int i = 0; i < fitparam_count + 5; i++)
      printf (" %-8.5f", deviation[i]);
    printf (" %-8.5f\n", sqrt (grassino_sum / 29));

    for (int i = 0; i < fitparam_count; i++)
      printf (" %a", *fitparam[i]);
    printf ("\n");

  }
  return 0;


  printf ("sum: %g\n", ferr (fitparam, fitparam_count));
  return 0;
  if (0) {
    Vdi_vs_Vab (Vab_0);
    return 0;
  }

  double val0 = 0x1.b00b2456a4abp+2;
  double valn = 0x1.b00b2456a4ab1p+2;

  //  double val0 = 0x1.b00b2456a4a9dp+2;
  //  double valn = 0x1.b00b2456a4b7ep+2;

  *fitparam[1] = val0; printf ("RV: %g\n", get_RV (0));
  *fitparam[1] = valn; printf ("RV: %g\n", get_RV (0));
  return 0;
  *fitparam[1] = val0; printf ("RV: %g, TLC: %g\n", get_RV (0), get_TLC (0));
  *fitparam[1] = valn; printf ("RV: %g, TLC: %g\n", get_RV (0), get_TLC (0));

  for (double val = val0; val < valn; val = nextafter (val, val + 1)) {
    *fitparam[1] = val;
    printf ("%a %.17e: %g\n", val, val, ferr (fitparam, fitparam_count));
  }
  return 0;

  printf ("exact Vdi0: %.17e\n", *fitparam[1]);
  for (int i = 0; i < fitparam_count; i++)
    printf (" %-8.5g", *fitparam[i]);
  printf ("\n");
  printf ("exact sum: %g\n", ferr (fitparam, fitparam_count));

  *fitparam[1] = good[1];

  for (int i = 0; i < fitparam_count; i++)
    printf (" %-8.5g", *fitparam[i]);
  printf ("\n");
  printf ("8.5g  sum: %g\n", ferr (fitparam, fitparam_count));
  return 0;
  
  if (0)
    {
      double t[] = {-.471, -.205, .000, .491, 1.044};
      double y[] = {15.86, 20.71, 24.84, 44.36, 60.23};
      for (int i = 0; i < 5; i++) {
        double P = get_sigma_di (.3, t[i] + Vdi0, 0, 0, 1);
        printf ("calc: %g, meas: %g, err: %g%%\n", P, y[i], (P - y[i]) / y[i] * 100);
      }
      return 0;
    }

  while (0) {
    State s = lung ((Motor) {0,0,0,0}, .5);
    printf ("%g\n", s.volume);
  }
  
  Params p;

  p.lma = 0;
  rohrer_constants (&p);
  for (p.u = 0; p.u <= 1; p.u += .0625)
    for (p.Phr_d = 0; p.Phr_d <= 1; p.Phr_d += .0625) {
      p.Vdi   = VdiFRC;
      p.Vab   = VabFRC;
      static_solution (&p);
    }
  exit (0);

  if (0) {
    double dsdi_Vdit0;
    gsl_vector *f = gsl_vector_alloc (2);
    double sdi0 = get_sigma_di (0x0p+0, 0x1.55d5333aa098fp+1, -0x1.9cfd790c2378ep+4, &dsdi_Vdit0, f);

    double dxt = 1;
    double dsdi_Vdit;
    double sdi = get_sigma_di (0x0p+0, 0x1.55d5333aa098fp+1, -0x1.9cfd790c2378ep+4 + dxt, &dsdi_Vdit, f);
    fprintf (stdout, ("dsdi_Vdit = %11.7f\n"
                      "         vs %11.7f\n"), (sdi - sdi0) / dxt, dsdi_Vdit0);
    
    exit (0);
  }

  if (0) {
    double dsab_Vabt0;
    gsl_vector *f = gsl_vector_alloc (2);
    double sab0 = get_sigma_ab (-0x1.6d9c3191611ddp-9, 0x1.0c35b02f32496p+2, -0x1.329010be447f8p+9, &dsab_Vabt0, f);
    fprintf (stdout, "dsab_Vabt0: %g\n", dsab_Vabt0);
    exit (0);
    double dxt = 1;
    double dsab_Vabt;
    double sab = get_sigma_ab (-0x1.6d9c3191611ddp-9, 0x1.0c35b06b8b8afp+2, 0x1.0d8132204d8aap+5 + dxt, &dsab_Vabt, f);
    fprintf (stdout, ("dsab_dVabt = %11.7f\n"
                      "          vs %11.7f\n"), (sab - sab0) / dxt, dsab_Vabt0);
    
    exit (0);
  }

  p.Vdi   = 0x1.363525886becap+0 ; /* 1.2117484529552933     */
  p.Vab   = 0x1.7fe4fe1706985p+2 ; /* 5.9983515953885034     */
  p.u     = 0x1.5e41b81b4d958p-5 ; /* 0.042755946715646587   */
  p.Phr_d = 0x1.8439552a36234p-2 ; /* 0.37912495679987335    */
  p.k1    = 0x1.016e83f82c24ap+39; /* 552830368790.07153     */
  p.k2    = 0x1.639ff618b12a7p+47; /* 195506828236949.22     */

  if (0) {
    printf ("sigma_L = %g\n", get_sigma_L (get_VL (p.Vdi, p.Vab)));
    printf ("sigma_ab = %g\n", get_sigma_ab (p.u, p.Vab, 0, 0, 1));
    printf ("sigma_di = %g\n", get_sigma_di (p.Phr_d, p.Vdi, 0, 0, 1));
    printf ("VL: %g\n", get_VL (p.Vdi, p.Vab));
    static_solution (&p);
    exit (0);
  }
  if (0) {
    char *tmp = asprintf("%s%s",outPath,"p");
    FILE *f = fopen (tmp, "w");
    free(tmp);
    double Vdi_t = 0;
    for (double Vab_t = 0; Vab_t >= -1; Vab_t -= .001) {
      double VL_t  = (-Vdi_t * A - Vdi_t - Vab_t) / A;
      fprintf (f, "%g %g %g\n",
               Vab_t, get_sigma_ab (p.u, p.Vab, Vab_t, 0, 1), airway_resistance (VL_t, &p, 0));
    }
    fclose (f);

    printf ("set xlabel 'Vab_t'; set ylabel 'f1'; plot 'p' u 1:2 w l t 'sigma_ab', 'p' u 1:3 w l t 'Rrs'\n");
    printf ("set term png; set output 'sigma_ab_Rrs_vs_Vab_t.png'; replot\n");
    exit (0);
  }
  if (0) {
    char *tmp = asprintf("%s%s",outPath,"p");
    FILE *f = fopen (tmp, "w");
    free(tmp);
    double Vab_t = 0;
    for (double Vdi_t = 0; Vdi_t <= 1; Vdi_t += .001) {
      double VL_t  = (-Vdi_t * A - Vdi_t - Vab_t) / A;
      fprintf (f, "%g %g %g\n",
               Vdi_t, get_sigma_di (p.Phr_d, p.Vdi, Vdi_t, 0, 1), airway_resistance (VL_t, &p, 0));
    }
    fclose (f);

    printf ("set xlabel 'Vdi_t'; set ylabel 'f1'; plot 'p' u 1:2 w l t 'sigma_di', 'p' u 1:3 w l t 'Rrs'\n");
    printf ("set term png; set output 'sigma_di_Rrs_vs_Vdi_t.png'; replot\n");
    exit (0);
  }
  

  if (0) {
    plot_cross (&p, "p");
    exit (0);
  }

  xio_0 = 0x1.4225ef99787b3p-8;
  xio_1 = -0x1.a225fa1278086p-8;
  const double y[2] = {0x1.36351de432a8ap+0, 0x1.7fe50091d2027p+2};
  double f[2];
  dVdt (0x1.ap+3, y, f, &p);
  printf ("solution: %a %a\n", f[0], f[1]);
  printf ("          %20g %20g\n", f[0], f[1]);
  exit (0);

  p.Rrs = INFINITY;
  p.Vdi = 1.7902405501407859; p.Vab = 5.4692277659172595; p.u = -0.0103667857580422556; p.Phr_d = -0.14746157843843841; p.lma = -2.1380684423820346;
  rohrer_constants (&p);

  double start = -1.05, end = -0.95, inc = .00001;
  start = -1.0, end = 4, inc = .001;

  printf ("%.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g\n", p.Vdi, p.Vab, p.u, p.Phr_d, p.Rrs, p.lma, p.k1, p.k2, p.Vdi_t, p.Vab_t);
  printf ("%g %g\n", start, noflow (start, &p));
  printf ("%g %g\n", start, noflow (start, &p));
  printf ("%g %g\n", start, noflow (start, &p));
  printf ("%.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g\n", p.Vdi, p.Vab, p.u, p.Phr_d, p.Rrs, p.lma, p.k1, p.k2, p.Vdi_t, p.Vab_t);
  exit (0);

  for (double Vdi_t = start; Vdi_t <= end; Vdi_t += inc)
    printf ("%g %g\n", Vdi_t, noflow (Vdi_t, &p));
  printf ("\n\n");
  p.Vdi = 1.7902400694951561; p.Vab = 5.4692283897952869; p.u = -0.0103667857580422556; p.Phr_d = -0.14746157843843841; p.lma = -2.1380684423820346;
  rohrer_constants (&p);
  for (double Vdi_t = start; Vdi_t <= end; Vdi_t += inc)
    printf ("%g %g\n", Vdi_t, noflow (Vdi_t, &p));
  error (0, 0, "%s", "plot \"t1\" i 0 w l t \"1\", \"t1\" i 1 w l t \"2\"\n");
  return 0;
  double rt = 1 / gsl_spline_eval (spline, 5.58636, acc);
  printf ("rt = %g, rs = %g\n", rt, rs_rt (rt));
  printf ("volume: %.17g\n", volume (rs_rt (rt), rt, ct) * 1000);
  printf ("volume: %.17g\n", volume (0.621984, 0.214640, ct) * 1000);
  for (double rt = .17; rt < 1; rt += .01)
    printf ("%g %g: %g\n", rs_rt (rt), rt, volume (rs_rt (rt), rt, ct) * 1000);
  return 0;
}
#endif

#ifdef NEWMAIN

int
main (void)
{
  State s;
  double phr = 0.05;
  double phr0 = 0.5;
  double lum = .573434;
  double lum0 = 0.065337;
  Motor motor (double t) {
    Motor m;
    if (t < 5) {
      m.phrenic = phr0;
      m.abdominal = lum0;
      m.pca = 40;
      m.ta = 0;
    }
    else if (t < 10) {
      m.phrenic = phr;
      m.abdominal = lum0;
      m.pca = 0;
      m.ta = 40;
    }
    else if (t < 15) {
      m.phrenic = 0;
      m.abdominal = lum;
      m.pca = 0;
      m.ta = 40;
    }
    else {
      m.phrenic = 0;
      m.abdominal = lum;
      m.pca = 40;
      m.ta = 0;
    }
    return m;
  }
  printf("s.volume: %g\n", s.lma);
  FILE *wfile = 0;
  wfile = fopen ("lung_out.txt", "w");
//            fprintf (wfile, "%12d %f\n", nrecs, S.step);
            
  fprintf(wfile, "Phrenic activation, Abdominal activation, %VC/s, %VC, Pressure (cmH20), timestep, lma (0-1), Pab, Pdi, PL, Pressure-PL\n");            
  //fprintf(wfile, "%VC/s, %VC, Pressure (cmH20), timestep, lma (0-1), Pab, Pdi, PL, Pressure-PL\n");            
  for (double t = 0; t < 20; t += .0005) {
    Motor mValue = motor (t);
    s = lung (mValue, .5);
    fprintf (wfile, "%g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g\n", mValue.phrenic, mValue.abdominal, s.flow, s.volume, s.pressure * 2.78, t, s.lma, s.Pab, s.Pdi, s.PL, s.pressure * 2.78 - s.PL);
//    fprintf (wfile, "%g, %g, %g, %g, %g, %g, %g, %g, %g\n", s.flow, s.volume, s.pressure * 2.78, t, s.lma, s.Pab, s.Pdi, s.PL, s.pressure * 2.78 - s.PL);
    
  }
  fclose(wfile);
  return 0;
}

#endif
