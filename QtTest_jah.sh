#!/bin/sh
pkg-config --short-errors --print-errors --cflags --libs "Qt5Core, Qt5Gui, Qt5Widgets Qt5Network Qt5OpenGL Qt5PrintSupport" 2>&1
