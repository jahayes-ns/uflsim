<h1>Installation Tips for Linux</h1>

- install qt5-base (Qt5Core, Qt5Gui, Qt5Widgets Qt5Network Qt5OpenGL and Qt5PrintSupport modules). ./configure uses the following to test that pkg-config can find them:
   	pkg-config --short-errors --print-errors --cflags --libs "Qt5Core, Qt5Gui, Qt5Widgets Qt5Network Qt5OpenGL Qt5PrintSupport" 2>&1
- install gperf (needed for build_hash.c)
- install muparser-dev (needed when muParserDLL.h is called for)
- install gsl
- install boost
- install local-son64-1.0.3
- install pdflatex (if you want the docs, otherwise 'make' fails after finishing the normal binaries like simbuild, simrun, snd2sim)

In .bashrc or .bash_profile
- export LD_LIBRARY_PATH should contain all the external .so files from the above installations (easiest was to copy most of the non-Qt5 ones into the uflsim directory [hence the '.' in LD_LIBRARY_PATH])
e.g., export LD_LIBRARY_PATH=/home/hayesjohn/anaconda3/lib:.
- export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/home/hayesjohn/anaconda3/lib/pkgconfig/
should point to the directory of the above libQt5*.so files before running ./configure
- export the PATH variable so that simbuild picks up simrun when executing it in a separate process
e.g., export PATH=$PATH:.

- had to modify the Makefile.in to set the DEFAULT_INCLUDES to include
DEFAULT_INCLUDES = -I. -I/home/hayesjohn/anaconda3/include -I/home/hayesjohn/src/muparser_v2_2_3/include -I/home/hayesjohn/src/local-son64-1.0.3 -I/home/hayesjohn/src/boost_1_81_0
run ./configure before 'make' after setting these

<h2>JAH Additional Notes:</h2>

- configure does not seem to respect the CPPFLAGS environment variable or --includedir directive, so I had to manually change the muparser include dir to /home/hayesjohn/src/muparser_v2_2_3/include in my Makefile on the following line:
   sim_CPPFLAGS = $(DEBUG_OR_NOT) -I/usr/include/muParser
to:
   sim_CPPFLAGS = $(DEBUG_OR_NOT) -I/home/hayesjohn/src/muparser_v2_2_3/include

- configure/Makefile DOES seem to respect the LD_LIBRARY_PATH environment variable to link the muParser library in with the following defined in .basrc:
   LD_LIBRARY_PATH=/home/hayesjohn/src/muparser_v2_2_3/lib/
  
<h2>JAH Debugging:</h2>
From Dale's email 10/12/22: 

"If you want to debug this or just step through the code, change the DEBUG_OR_NOT variable to --gdb3. You'll need to do a make clean, then make lin, to generate the debug info. You can use qtcreator as the GUI front end for debugging."
