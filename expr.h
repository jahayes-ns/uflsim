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

typedef struct expr Expr;

// see the HOW_TO_BUILD for what this is all about
#if defined WIN32
#undef _UNICODE
#define MUPARSERLIB_EXPORTS // to get unmangled muparser names
#endif

#include "muParserDLL.h"

#ifdef __cplusplus
extern "C" {
#endif

  Expr *xp_set (char *txt);
  double xp_eval (Expr *e, double val);
  void xp_free (Expr *e);

#ifdef __cplusplus
}
#endif

