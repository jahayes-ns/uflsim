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
#include <cstdlib>
#include <getopt.h>
#include <unistd.h>
#include "simviewer.h"
#include <QApplication>


using namespace std;

// the only explict exit call is when asprintf can't get any memory.
// it is unlikely, but tell the user if this happens.
static bool normal_exit = false;
static void atexit_handler()
{
   if (!normal_exit)
      cout << "Out of memory, exiting program" << endl;
}

static int read_file;
static int use_socket;
const static struct option options[] =
{
   {"l",required_argument,0,'l'},
   {"port",required_argument,0,'p'},
   {"file",no_argument,&read_file,1},
   {"socket",no_argument,&use_socket,1},
   {"host",required_argument,0,'h'},
   {0,0,0,0}
};


int main(int argc, char *argv[])
{
   int launch = -1;
   int port = 0;
   int c;
   QString hostname;

   while (1)
   {
      int option_index = 0;
      c = getopt_long(argc, argv, "l:p:",options,&option_index);
      if (c == -1)
         break;
      switch(c)
      {
         case 'p':
            if (optarg)
               sscanf(optarg, "%d",&port);
            break;
         case 'l':
            if (optarg)
               sscanf(optarg, "%d",&launch);
            break;
        case 'h':
           if (optarg)
             hostname = optarg;
           cout << "VIEWER: h arg is " << hostname.toLatin1().data() << endl;
           break;
 
         default:
            break;
      }
   }
   if (launch == -1)
   {
      cerr << "usage: " << argv[0] << " [-l launch number] [-p port number] [--file | --socket]"
         << endl 
         << "This program is usually managed by simbuild. It will connect an instance" << endl
         << "of this program with simrun. This typically will not generate wave files." << endl
         << "The simbuild, simrun, and simviewer programs use network sockets" << endl
         << "for direct communication between the programs." << endl
         << "The simbuild program has an option that selects which method to use." << endl
         << "If you wish to view the simulation result at a later time, create wave files." << endl
         << "Otherwise, use direct communication."
         << "To view saved wave files, run simviewer with the --file option." << endl
         << endl;
   }
   if (!read_file && !use_socket) // assuming running stand-alone, only --file makes sense
   {
      cerr << "No --file or --socket flag present, defaulting to reading wave files." << endl;
      read_file = true;
   }
   else if (read_file && use_socket) // one or the other
   {
      cerr << "WARNING: You can select only one of --file and --socket. Exiting program..." << endl;
      exit(1);
   }
   if (launch == -1)
   {
      cerr << "Launch number not provided, defaulting to 0." << endl;
      launch = 0;
   }
   cout << "SIMVIEWER: launch " << launch << " simbuild port " << port << endl;
   atexit(atexit_handler);
   QApplication a(argc, argv);
   SimViewer w(nullptr,launch,port,use_socket,read_file,hostname);
   w.show();

   int ret = a.exec();
   normal_exit = true; 
   return ret;
}




