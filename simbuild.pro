#-------------------------------------------------
#
# Project created by QtCreator 2017-11-14T15:14:39
#
#-------------------------------------------------

QT += core gui printsupport network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = simbuild
TEMPLATE = app

CONFIG -= debug_and_release
CONFIG +=c++17

QMAKE_CXXFLAGS += -Wall -Wno-strict-aliasing -std=c++17
QMAKE_CFLAGS += -Wall -Wno-strict-aliasing
linux {
QMAKE_LFLAGS += -Wl,--wrap=getline
}

wasm {
QMAKE_LFLAGS += -Wl,--wrap=getline --emrun
}

win32 {
   QMAKE_CXXFLAGS += -O2 -D__USE_MINGW_ANSI_STDIO -D_GNU_SOURCE -I/usr/local/include
    QMAKE_CFLAGS += -O2 -D__USE_MINGW_ANSI_STDIO -D_GNU_SOURCE
   LIBS += -lwinpthread
   CONFIG -= debug
}

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES += \
   affmodel.cpp \
        simmain.cpp \
        simwin.cpp \
    sim_impl.cpp \
    simscene.cpp \
    simscene_export.cpp \
    simscene_draw.cpp \
    fileio.c \
    swap.c \
    util.c \
    launchwindow.cpp \
    launch_impl.cpp \
    launch_model.cpp \
    simview.cpp \
    c_globals.c \
    colormap.cpp \
    node_mgr.c \
    sim2build.cpp \
    helpbox.cpp \
    selectaxonsyn.cpp \
    simnodes.cpp \
    sim_hash.c \
    sim_proc.cpp \
    build_model.cpp \
    synview.cpp \
    build_hash.c \
    chglog.cpp \
    lin2ms.c \
    wavemarkers.c \
    finddialog.cpp \
    slope_spin.cpp

HEADERS += \
   affmodel.h \
        simwin.h \
    simscene.h \
    sim2build.h \
    simulator.h \
    simulator_hash.h \
    util.h \
    inode.h \
    inode_hash.h \
    old_inode.h \
    fileio.h \
    swap.h \
    inode_2.h \
    hash.h \
    launchwindow.h \
    launch_model.h \
    colormap.h \
    simview.h \
    helpbox.h \
    selectaxonsyn.h \
    simnodes.h \
    sim_proc.h \
    build_model.h \
    synview.h \
    chglog.h \
    lin2ms.h \
    wavemarkers.h \
    finddialog.h \
    common_def.h \
    slope_spin.h

FORMS += \
    simwin.ui \
    launchwindow.ui \
    helpbox.ui \
    selectaxonsyn.ui \
    finddialog.ui


win32 {
  OBJECTS_DIR = mswin
  MAKEFILE = Makefile_simbuild_win.qt
  RC_ICONS = simbuild.ico
}


linux {
  MAKEFILE=Makefile_simbuild.qt
  CONFIG += debug
}

wasm {
  OBJECTS_DIR = webasm
  MAKEFILE=Makefile_simbuild_web514.qt
}

#DISTFILES += \
#    simwin.o \
#    qseltablewidget.o

RESOURCES += \
    simbuild.qrc



