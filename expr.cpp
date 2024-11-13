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
#include <new>
#include <stdlib.h>
#include "expr.h"
#include <muParser.h>

using namespace std;
using namespace mu;

struct expr
{
  double val;
  Parser *p;
};

Expr *
xp_set (char *txt)
{
  Expr *e = new (nothrow) Expr;
  if (!e)
    return e;
  e->val = 0;
  e->p = new (nothrow) Parser;
  if (!e->p) {
    delete e;
    return NULL;
  }
  try {
    e->p->DefineVar("V", &e->val); 
    e->p->SetExpr (txt);
  }
  catch (Parser::exception_type &err) {
    std::cout << err.GetMsg() << std::endl;
    delete e->p;
    delete e;
    return NULL;
  }
  return e;
}

double
xp_eval (Expr *e, double arg)
{
  e->val = arg;
  double val;
  try {
    val = e->p->Eval();
  }
  catch (Parser::exception_type &err) {
    std::cout << err.GetMsg() << endl;
    abort ();
  }
  return val;
}

void
xp_free (Expr *e)
{
  delete e->p;
}

