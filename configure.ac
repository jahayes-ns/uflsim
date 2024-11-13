# (Copyright 2005-2020 Kendall F. Morris
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
#

AC_PREREQ([2.69])
AC_INIT([usfsim],[1.0.36],[hayesjohn@ufl.edu])
AC_CONFIG_AUX_DIR([.])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([simwin.cpp])

AC_USE_SYSTEM_EXTENSIONS

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AM_PROG_CC_C_O

# Checks for libraries.
AC_PATH_XTRA
AC_CHECK_LIB(fftw3f, fftwf_execute)
AC_CHECK_LIB(Judy, JudyLIns)

if test $ac_cv_lib_fftw3f_fftwf_execute = no; then
   SAVED_LDFLAGS=$LDFLAGS
   LDFLAGS="$LDFLAGS -L/usr/local/lib"
   AC_CHECK_LIB(fftw3f, fftwf_execute)
   if test $ac_cv_lib_fftw3f_fftwf_execute = yes; then
      AC_SUBST(FFTWLIBDIR,[-L/usr/local/lib])
   fi
   LDFLAGS=$SAVED_LDFLAGS
fi

AM_CONDITIONAL([COND_FFTW], [test $ac_cv_lib_fftw3f_fftwf_execute = yes])

# Checks for header files.
AC_CHECK_HEADERS([float.h limits.h stdlib.h string.h unistd.h /usr/include/malloc.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_CHECK_SIZEOF(size_t)

#AC_CHECK_DECLS([getline],,[AC_LIBOBJ(getline)])

# Checks for library functions.
#AC_CHECK_FUNC([malloc_usable_size],,[AC_LIBOBJ(malloc)])

AC_CHECK_FUNCS(strchr argp_parse)
AM_CONDITIONAL([COND_ARGP], [test "$ac_cv_func_argp_parse" = no])

#AC_REPLACE_FUNCS([asprintf])

CPPFLAGS=-I/home/hayesjohn/anaconda3/include -I/home/hayesjohn/src/muparser_v2_2_3/include -I/home/hayesjohn/src/local-son64-1.0.3 -I/home/hayesjohn/src/boost_1_81_0
CXXFLAGS=""
CFLAGS=""

# Check for Qt libraries
PKG_CHECK_MODULES(QT, [Qt5Core, Qt5Gui, Qt5Widgets Qt5Network Qt5OpenGL Qt5PrintSupport], [], [AC_MSG_ERROR([Qt libraries are required.])])

# Retrieve Qt compilation and linker flags
CPPFLAGS="`$PKG_CONFIG --cflags-only-I Qt5Core Qt5Gui Qt5Widgets Qt5Network Qt5PrintSupport ` $CPPFLAGS"
LDFLAGS="`$PKG_CONFIG --libs-only-L Qt5Core Qt5Gui Qt5Widgets Qt5Network Qt5PrintSupport` $LDFLAGS"
LIBS="`$PKG_CONFIG --libs-only-l Qt5Core Qt5Gui Qt5Widgets Qt5Network Qt5PrintSupport`  $LIBS"

if ! `$PKG_CONFIG --atleast-version=5.9.0 Qt5Core`; then
   AC_MSG_ERROR([Qt 5.9.0 or greater is required.])
fi

AC_CHECK_PROGS(MOC, [moc])
AC_CHECK_PROGS(UIC, [uic])
AC_CHECK_PROGS(RCC, [rcc])

if test -z "$MOC" || test -z "$UIC" || test -z "$RCC"; then 
   AC_MSG_ERROR([Qt utility programs moc, uic, and rcc are required.])
fi

AC_CHECK_PROGS([MXE_QMAKE],[x86_64-w64-mingw32.static-qmake-qt5])

if test -z "$MXE_QMAKE"; then
   AC_MSG_WARN([The MXE cross development environment is required to build the MS Windows version of usfsim packate (not fatal).  Consult the HOWTO_BUILD_SIM_FOR_WIN document included in this package.])
else
   AC_MSG_NOTICE([The MXE cross development environment is available.])
fi
AM_CONDITIONAL([MXE_QMAKE],[test -z "$MXE_QMAKE"])


AC_CONFIG_FILES([Makefile])
AC_OUTPUT
