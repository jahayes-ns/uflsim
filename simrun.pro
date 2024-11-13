#-------------------------------------------------
#
# Project created by QtCreator 2017-11-14T15:14:39
#
#-------------------------------------------------

QT += widgets
TARGET = simrun
TEMPLATE = app

CONFIG -= debug_and_release
CONFIG += warn_off
CONFIG += console
CONFIG +=c++17
QT -= gui


QMAKE_CXXFLAGS += -Wall -Wno-strict-aliasing -std=c++17
QMAKE_CFLAGS += -Wall -Wno-strict-aliasing

LIBS += -lm -lgsl -lgslcblas -lmuparser -lfftw3f

linux {
   DEFINES += __linux__
   QMAKE_LFLAGS += -Wl,--wrap=getline
   MAKEFILE=Makefile_simrun.qt
   LIBS += -lJudy -lson64
   CONFIG += debug
}

win32 {
   DEFINES -= UNICODE
   DEFINES -= _UNICODE
   DEFINES += S64_NOTDLL 
   DEFINES -= WIN32
   DEFINES += WIN64 
   QMAKE_CXXFLAGS += -O2 -static -D__USE_MINGW_ANSI_STDIO -D_GNU_SOURCE -std=c++17 -I/usr/local/include 
   QMAKE_CLAGS += -static -O2 -D__USE_MINGW_ANSI_STDIO -D_GNU_SOURCE
   QMAKE_LFLAGS += -static
   OBJECTS_DIR = mswin
   LIBS += -lson64 -lwinpthread -lgnurx
   CONFIG -= debug
   MAKEFILE=Makefile_simrun_win.qt
}


SOURCES += read_sim.c \
           build_network.c \
           simloop.c \
           sim.c update.c \
           fileio.c \
           sim_hash.c \
           util.c \
           build_hash.c \
           sample_cells.c \
           lung.c \
           expr.cpp \
           lin2ms.c \
           wavemarkers.c \
           simrun_wrap.cpp \
           add_IandE.cpp

HEADERS += simulator.h \
           util.h \
           fileio.h \
           hash.h \
           simulator_hash.h \
           inode.h \
           sample_cells.h \
           lung.h \
           expr.h \
           lin2ms.h \
           wavemarkers.h \
           simrun_wrap.h \
           common_def.h


