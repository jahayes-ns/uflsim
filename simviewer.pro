#-------------------------------------------------
#
# Project created by QtCreator 2018-05-18T12:45:27
#
#-------------------------------------------------

QT += core gui printsupport network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = simviewer
TEMPLATE = app
CONFIG -= debug_and_release
CONFIG +=c++17
#CONFIG += warn_off

linux {
QMAKE_LFLAGS += -Wl,--wrap=getline
}

QMAKE_CXXFLAGS += -Wall -Wno-strict-aliasing -std=c++17
QMAKE_CFLAGS += -Wall -Wno-strict-aliasing

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES += \
    simviewer.cpp \
    simviewermain.cpp \
    simviewer_impl.cpp \
    lin2ms.c \
    wavemarkers.c 

HEADERS += \
    simviewer.h \
    lin2ms.h \
    wavemarkers.c 

FORMS += \
        simviewer.ui

#DISTFILES += \
#    simview.o

linux {
   MAKEFILE=Makefile_simviewer.qt
   CONFIG += debug
}

wasm {
  OBJECTS_DIR = webasm
  MAKEFILE=Makefile_simviewer_web514.qt
}

win32 {
  QMAKE_CXXFLAGS += -O2 -D__USE_MINGW_ANSI_STDIO -D_GNU_SOURCE
  QMAKE_CLAGS += -O2 -D__USE_MINGW_ANSI_STDIO -D_GNU_SOURCE
  OBJECTS_DIR = mswin
  LIBS += -lwinpthread
  CONFIG -= debug
  MAKEFILE=Makefile_simviewer_win.qt
  RC_ICONS = simviewer.ico
}

RESOURCES += \
    simviewer.qrc

DISTFILES +=



