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


#include <QApplication>
#include <getopt.h>
#include "simwin.h"

bool Debug = false;
extern int winWidth;
extern int winHeight;

static void parse_args(int, char **);

int main(int argc, char *argv[])
{
   parse_args(argc,argv);
   QApplication a(argc, argv);
   SimWin w;
   if (Debug)
      w.printMsg("Debug output turned on");
   w.show();
   return a.exec();
}

static void parse_args(int argc, char *argv[])
{
   int cmd;
   static struct option opts[] = 
   {
      {"g", required_argument, NULL, 'g'},
      {"d", no_argument, NULL, 'd'},
      { 0,0,0,0} 
   };
   while ((cmd = getopt_long(argc, argv, "dg:", opts, NULL )) != -1)
   {
      switch (cmd)
      {
         case 'd':
               Debug = true;
               break;
         case 'g':
               sscanf(optarg, "%dx%d", &winWidth,&winHeight);
               break;
         case '?':
         default:
           break;
      }
   }
}
