#*(Copyright 2005-2020 Kendall F. Morris
#
# This file is part of the USF Neural Simulator suite.
#
#   The Neural Simulator suite is free software: you can redistribute
#   it and/or modify it under the terms of the GNU General Public
#   License as published by the Free Software Foundation, either
#   version 3 of the License, or (at your option) any later version.
#
#   The suite is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with the suite.  If not, see <https://www.gnu.org/licenses/>.



TARGET = wave2daq
TEMPLATE = app

win32 {
   CONFIG -= debug_and_release
   CONFIG += warn_off
   CONFIG += console
   CONFIG +=c++17
   DEFINES -= UNICODE
   DEFINES -= _UNICODE
   QT -= gui

   SOURCES += wave2daq.cpp

   DEFINES += S64_NOTDLL
   CONFIG -= debug
   QMAKE_CXXFLAGS += -D__USE_MINGW_ANSI_STDIO -D_GNU_SOURCE -static
   QMAKE_LFLAGS += -static
   OBJECTS_DIR = mswin
   LIBS += -lwinpthread
   CONFIG -= debug
   MAKEFILE=Makefile_wave2daq_win.qt
}



