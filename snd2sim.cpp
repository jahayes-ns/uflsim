// JAH: 221209

#include <getopt.h>
#include <QApplication>
#include <getopt.h>
#include "simwin.h"
#include <iostream>

bool Debug = false;

static void parse_args(int, char **);

QString inputSndFilename = "";
QString outputSimFilename("");
int step_count = 0;

int main(int argc, char *argv[])
{
   parse_args(argc,argv);
   QApplication a(argc, argv);
   SimWin w(0);
   if (Debug)
      w.printMsg("Debug output turned on");
   
   w.loadSnd(&inputSndFilename);
   std::cerr << "Loaded: " << inputSndFilename.toStdString() << "\n";

   if (step_count != 0) {
      w.setStepCount(step_count);
      std::cerr << "#steps: " << step_count << "\n";
   }
   w.saveSim(&outputSimFilename);
   std::cerr << "Saved:  " << outputSimFilename.toStdString() << "\n";
}

static void parse_args(int argc, char *argv[])
{
   int cmd;
   static struct option opts[] = 
   {
      {"i", required_argument, NULL, 'i'},
      {"o", required_argument, NULL, 'o'},
      {"s", required_argument, NULL, 's'},
      {"d", no_argument, NULL, 'd'},
      {"h", no_argument, NULL, 'h'},
      { 0,0,0,0} 
   };
   while ((cmd = getopt_long(argc, argv, "di:o:hs:", opts, NULL )) != -1)
   {
      switch (cmd)
      {
         case 'd':
               Debug = true;
               break;
         case 'i':
               inputSndFilename = optarg;
               break;
         case 'o':
               outputSimFilename = optarg;
               break;
         case 's':
               step_count = atoi(optarg);
               break;
         case 'h':
         case '?':
         default:
            std::cerr << "\n   usage:\n\tsnd2sim -i [input_snd_file] -o [output_sim_file] -s [total # of simulation time steps]\n\n";
            std::cerr << "        Notes:\n";
            std::cerr << "            [input_snd_file] and [output_sim_file] are necessary and\n";
            std::cerr << "            [output_sim_file] is the filename relative to the [input_snd_filename]'s directory.\n\n";
            exit(1);
            break;
      }
   }
}
