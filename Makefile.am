#(Copyright 2005-2020 Kendall F. Morris
#
# This file is part of the USF Neural Simulator suite.
#
#     The Neural Simulator suite is free software: you can redistribute
#     it and/or modify it under the terms of the GNU General Public
#     License as published by the Free Software Foundation, either
#     version 3 of the License, or (at your option) any later version.
#
#     The suite is distributed in the hope that it will be useful,
#     but WITHOUT ANY WARRANTY; without even the implied warranty of
#     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#     GNU General Public License for more details.
#
#     You should have received a copy of the GNU General Public License
#     along with the suite.  If not, see <https://www.gnu.org/licenses/>.
#

AUTOMAKE_OPTIONS= -Wno-portability
MSWIN_DIR=mswin

LDADD = -lm $(LIBOBJS) -lgsl -lgslcblas
simspectrum_LDADD = -lm -lfftw3f
simspectrum_LDFLAGS = $(FFTWLIBDIR)
simrun_LDADD = $(LDADD) -lmuparser -lson64 -lpthread

#DEBUG_OR_NOT= -ggdb3
DEBUG_OR_NOT= -O2

#AM_CXXFLAGS = $(DEBUG_OR_NOT) -Wall -fPIC
#AM_CXXFLAGS = $(DEBUG_OR_NOT) -Wall -std=c++17 -fPIC -Wno-unused-value -Wno-deprecated-copy -DVERSION=\"$(VERSION)\"
AM_CXXFLAGS = $(DEBUG_OR_NOT) -Wall -std=c++17 -fPIC -Wno-unused-value -Wno-unused-result -Wno-deprecated-copy
#AM_CPPFLAGS = $(DEBUG_OR_NOT) -Wno-unused-value -Wno-deprecated-copy -DVERSION=\"$(VERSION)\"
AM_CPPFLAGS = $(DEBUG_OR_NOT) -DVERSION=\"$(VERSION)\"
AM_CFLAGS = $(DEBUG_OR_NOT) -Wall --std=c11 -Wno-unused-value -Wno-unused-result

bin_PROGRAMS = snd2sim.exe simbuild.exe simrun.exe simviewer.exe simmsg.exe edt2spike2.exe \
              makesine.exe wave2daq.exe \
              snd2sim simbuild simrun simviewer simmsg edt2spike2 wave2daq \
              simpickwave simpickedt simtxt2flt simmerge makesine rplssimc_p


if COND_FFTW
  bin_PROGRAMS += simspectrum
endif

dist_bin_SCRIPTS = simpower_spectrum.sh simrenumber

simpickwave_SOURCES = simpickwave.c
simpickedt_SOURCES = simpickedt.c
simspectrum_SOURCES = simspectrum.c util.c
simtxt2flt_SOURCES = simtxt2flt.c
simmerge_SOURCES = simmerge.c

BUILT_SOURCES = build_hash.c sim_hash.c inode_hash.h simulator_hash.h

simrun_SOURCES = read_sim.c build_network.c simloop.c sim.c update.c \
simulator.h fileio.c sim_hash.c util.c util.h \
build_hash.c fileio.h hash.h simulator_hash.h \
inode.h old_inode.h inode_2.h sample_cells.c sample_cells.h lung.c lung.h expr.cpp \
lin2ms.c lin2ms.h expr.h wavemarkers.c wavemarkers.h simrun_wrap.cpp simrun_wrap.h \
common_def.h add_IandE.cpp
simrun_exe_SOURCES = $(simrun_SOURCES) simrun.pro


simbuild_BUILT_SOURCES= ui_simwin.h ui_launchwindow.h moc_simwin.cpp moc_simscene.cpp \
 moc_launchwindow.cpp moc_launch_model.cpp moc_simview.cpp \
 qrc_simbuild.cpp moc_helpbox.cpp ui_helpbox.h moc_selectaxonsyn.cpp \
 moc_sim_proc.cpp moc_build_model.cpp moc_synview.cpp ui_selectaxonsyn.h \
 moc_affmodel.cpp moc_finddialog.cpp ui_finddialog.h moc_slope_spin.cpp

BUILT_SOURCES += $(simbuild_BUILT_SOURCES)

simbuild_code = simmain.cpp simwin.cpp sim_impl.cpp simview.cpp simview.h simscene.cpp \
 sim2build.cpp fileio.c swap.c swap.h util.c build_hash.c \
 sim_hash.c simwin.ui sim2build.h simscene_export.cpp simscene_draw.cpp \
 launchwindow.cpp launchwindow.ui simwin.h affmodel.cpp affmodel.h launchwindow.h \
 launch_impl.cpp \
 launch_model.cpp launch_model.h simbuild.qrc simbuild.png c_globals.c c_globals.h \
 colormap.cpp colormap.h node_mgr.c helpbox.cpp helpbox.h helpbox.ui \
 simnodes.h simnodes.cpp selectaxonsyn.h selectaxonsyn.cpp selectaxonsyn.ui \
 sim_proc.h sim_proc.cpp build_model.h build_model.cpp synview.h synview.cpp \
 chglog.cpp chglog.h build_syn.png cell_icon.png conn_icon.png simscene.h \
 delete_icon.png fiber_icon.png launch_icon.png quit_icon.png globals_icon.png \
 lin2ms.c lin2ms.h simbuild.ico import_icon.png export_icon.png \
 wavemarkers.c wavemarkers.h finddialog.cpp finddialog.h finddialog.ui \
 slope_spin.cpp slope_spin.h \
 common_def.h leftarrow.png rightarrow.png uparrow.png downarrow.png \
 inode_hash.h


simbuild_SOURCES = $(simbuild_BUILT_SOURCES) $(simbuild_code)
simbuild_exe_SOURCES = $(simbuild_SOURCES) simbuild.pro

snd2sim_code = snd2sim.cpp simwin.cpp sim_impl.cpp simview.cpp simview.h simscene.cpp \
 sim2build.cpp fileio.c swap.c swap.h util.c build_hash.c \
 sim_hash.c simwin.ui sim2build.h simscene_export.cpp simscene_draw.cpp \
 launchwindow.cpp launchwindow.ui simwin.h affmodel.cpp affmodel.h launchwindow.h \
 launch_impl.cpp \
 launch_model.cpp launch_model.h simbuild.qrc simbuild.png c_globals.c c_globals.h \
 colormap.cpp colormap.h node_mgr.c helpbox.cpp helpbox.h helpbox.ui \
 simnodes.h simnodes.cpp selectaxonsyn.h selectaxonsyn.cpp selectaxonsyn.ui \
 sim_proc.h sim_proc.cpp build_model.h build_model.cpp synview.h synview.cpp \
 chglog.cpp chglog.h build_syn.png cell_icon.png conn_icon.png simscene.h \
 delete_icon.png fiber_icon.png launch_icon.png quit_icon.png globals_icon.png \
 lin2ms.c lin2ms.h simbuild.ico import_icon.png export_icon.png \
 wavemarkers.c wavemarkers.h finddialog.cpp finddialog.h finddialog.ui \
 slope_spin.cpp slope_spin.h \
 common_def.h leftarrow.png rightarrow.png uparrow.png downarrow.png \
 inode_hash.h


snd2sim_SOURCES = $(simbuild_BUILT_SOURCES) $(snd2sim_code)
snd2sim_exe_SOURCES = $(snd2sim_SOURCES) snd2sim.pro


simviewer_BUILT_SOURCES = moc_simviewer.cpp ui_simviewer.h qrc_simviewer.cpp
simviewer_code= simviewer.cpp simviewer.h simviewer_impl.cpp simviewermain.cpp \
					 simviewer.ui fittoscreen_icon.png quit_icon.png \
                savetopdf_icon.png simviewer.png simviewer.qrc \
                lin2ms.c lin2ms.h simviewer.ico chdir_icon.png restart_icon.png\
					 wavemarkers.c wavemarkers.h simviewert.png
simviewer_SOURCES = $(simviewer_BUILT_SOURCES) $(simviewer_code)
simviewer_exe_SOURCES = $(simviewer_SOURCES) simviewer.pro

BUILT_SOURCES += $(simviewer_BUILT_SOURCES)

simmsg_BUILT_SOURCES = moc_simmsg.cpp ui_simmsg.h
simmsg_SOURCES = simmsg.cpp simmsg.h simmsg.ui $(simmsg_BUILT_SOURCES)
simmsg_exe_SOURCES = $(simmsg_SOURCES) simmsg.pro

BUILT_SOURCES += $(simmsg_BUILT_SOURCES)

edt2spike2_SOURCES = edt2spike2.cpp
edt2spike2_exe_SOURCES = $(edt2spike2_SOURCES) edt2spike2_win.pro

makesine_SOURCES = makesine.cpp
makesine_exe_SOURCES = $(makesine_SOURCES) makesine_win.pro

rplssimc_p_SOURCES = rplssimc_p.cpp


wave2daq_SOURCES = wave2daq.cpp
wave2daq_exe_SOURCES = $(wave2daq_SOURCES) wave2daq_win.pro

BUILT_SOURCES += Makefile_simbuild.qt Makefile_simviewer.qt Makefile_simmsg.qt \
					  Makefile_simrun.qt

if MXE_QMAKE
BUILT_SOURCES += Makefile_simbuild_win.qt Makefile_simviewer_win.qt \
                 Makefile_simmsg_win.qt Makefile_snd2sim_win.qt Makefile_simrun_win.qt Makefile_edt2spike2_win.qt \
                 Makefile_wave2daq_win.qt Makefile_makesine_win.qt
endif

#BUILT_SOURCES += simbuild_plugin_import.cpp simmsg_plugin_import.cpp \
#					  simviewer_plugin_import.cpp


CLEANFILES = ${BUILT_SOURCES} \
             $(MSWIN_DIR)/*.o

EXTRA_DIST = gen_gperf.pl tag.pl inode_choose.c gen_hash.sh \
sim.tex usfsim.tex paramgen.pdf references.bib debian \
simbuild.pro simmsg.pro simviewer.pro snd2sim.pro simrun.pro \
HOWTO_BUILD_SIM_FOR_WIN


dist_doc_DATA=sim.pdf usfsim.pdf COPYING LICENSE README README_FOR_WINDOWS \
				  COPYRIGHTS ReleaseNotes.odt ReleaseNotes.pdf ChangeLog

LINUX_FILES=simbuild.desktop simviewer.desktop example_model.snd example_model.ols OConnor2012.snd \
				ADDUCTOR-51.v6.snd ADDUCTOR-51.v6.ols learn_demo.snd learn_demo.ols
WIN_FILES=example_modelWIN.snd example_modelWIN.ols OConnor2012.snd \
				ADDUCTOR-51.v6.win.snd ADDUCTOR-51.v6.win.ols \
				learn_demoWIN.snd learn_demoWIN.ols
LW_FILES=def_cell.sned def_fiber.sned def_syn.sned globals.sned \
simbuild.png simviewer.png

dist_pkgdata_DATA= $(LINUX_FILES) $(LW_FILES) $(WIN_FILES) usfsim_win_$(VERSION).zip

checkin_pD=        $(LINUX_FILES) $(LW_FILES) $(WIN_FILES)

WINDOWS_FILES=simbuild.exe snd2sim.exe simrun.exe simviewer.exe simmsg.exe edt2spike2.exe \
				wave2daq.exe makesine.exe $(WIN_FILES) $(LW_FILES) $(dist_doc_DATA)

zipit:
	make usfsim_win_$(VERSION).zip

usfsim_win_$(VERSION).zip: $(WINDOWS_FILES)
	zip usfsim_win_$(VERSION).zip $(WINDOWS_FILES)

# First pass generates files, second pass uses them.
usfsim.pdf: usfsim.tex references.bib Makefile
	pdflatex usfsim
	latexmk -r latexmkrc.tex -shell-escape -gg -pdf usfsim


#	latexmk -pdf usfsim.tex
#	latexmk -gg -pdf usfsim.tex
#	latexmk -f -gg -pdf usfsim.tex

sim.pdf: sim.tex references.bib
	latexmk -f -gg -pdf sim.tex

sim_CPPFLAGS = $(DEBUG_OR_NOT) $(CPPFLAGS)

simbuild_CXXFLAGS = $(AM_CXXFLAGS) `pkg-config --cflags Qt5Gui Qt5Network Qt5Core Qt5Widgets ` -m64 -pipe -W -D_REENTRANT -fPIC ${DEFINES} $(CPPFLAGS)
simbuild_LDFLAGS = `pkg-config --libs Qt5Gui Qt5Core Qt5Widgets Qt5Network  Qt5PrintSupport  `  -lpthread -Wl,--wrap=getline
simviewer_CXXFLAGS = $(AM_CXXFLAGS) `pkg-config --cflags Qt5Gui Qt5Core Qt5Widgets Qt5PrintSupport` -m64 -pipe -W -D_REENTRANT -fPIC ${DEFINES} $(CPPFLAGS)
simviewer_LDFLAGS = `pkg-config --libs Qt5Gui Qt5Core Qt5Widgets Qt5PrintSupport  `  -lpthread -Wl,--wrap=getline
#simrun_CXXFLAGS = $(AM_CXXFLAGS) `pkg-config --cflags Qt5Gui Qt5Core Qt5Widgets ` -m64 -pipe -W -D_REENTRANT -fPIC -DS64_NOTDLL ${DEFINES}
simrun_CXXFLAGS = $(AM_CXXFLAGS)  -m64 -pipe -W -D_REENTRANT -fPIC -DS64_NOTDLL ${DEFINES} $(CPPFLAGS)
#simrun_LDFLAGS = `pkg-config --libs Qt5Gui Qt5Core Qt5Widgets  ` -lpthread -Wl,--wrap=getline
simrun_LDFLAGS =  -lpthread -Wl,--wrap=getline
edt2spike2_CXXFLAGS = $(AM_CXXFLAGS) -pipe -W -D_REENTRANT -fPIC
edt2spike2_LDADD = -lson64 -lpthread

makesine_CXXFLAGS = $(AM_CXXFLAGS) -pipe -W -D_REENTRANT -fPIC
makesine_LDADD = -lson64 -lpthread

rplssimc_p_CXXFLAGS = $(AM_CXXFLAGS) -pipe -W -D_REENTRANT -fPIC


wave2daq_CXXFLAGS = $(AM_CXXFLAGS) -pipe -W -D_REENTRANT -fPIC

# these build .c files with a lookup tables for structs so we can save and
# read files that are implementation independent (holes in structs, variations
# between compiler versions, etc.)
sim_hash.c: simulator.h gen_gperf.pl Makefile.am tag.pl gen_hash.sh simulator_CPPFLAGS
	./gen_hash.sh simulator.h > sim_hash.c
	test -s sim_hash.c || rm sim_hash.c

inode_hash.h: build_hash.c

build_hash.c: inode.h  gen_gperf.pl Makefile.am tag.pl gen_hash.sh
	./gen_hash.sh inode.h > build_hash.c
	test -s build_hash.c || rm build_hash.c

simfiles: $(sim_SOURCES) Makefile.am
	ls $(sim_SOURCES) > simfiles

simulator_CPPFLAGS: Makefile.am	#for use by gen_hash.sh
	echo $(sim_CPPFLAGS) > simulator_CPPFLAGS

lin: $(BUILT_SOURCES)
	make simbuild
	make simrun
	make snd2sim
	make simviewer
	make simmsg
	make edt2spike2
	make wave2daq


win: $(BUILT_SOURCES)
	make mswin
	make simrun.exe
	make snd2sim.exe
	make simbuild.exe
	make simviewer.exe
	make simmsg.exe
	make edt2spike2.exe
	make wave2daq.exe
	make zipit

mswin:
	@mkdir -p $(MSWIN_DIR)

simrun.exe$(EXEEXT): mswin Makefile_simrun_win.qt $(simrun_SOURCES)
 ifdef MXE_QMAKE
	make -f Makefile_simrun_win.qt
 else
	@echo MXE environment not installed, windows program not built
 endif

snd2sim.exe$(EXEEXT): mswin Makefile_snd2sim_win.qt $(snd2sim_SOURCES)
 ifdef MXE_QMAKE
	make -f Makefile_snd2sim_win.qt
 else
	@echo MXE environment not installed, windows program not built
 endif

simbuild.exe$(EXEEXT): mswin simbuild.pro Makefile_simbuild_win.qt $(simbuild_SOURCES)
 ifdef MXE_QMAKE
	make -f Makefile_simbuild_win.qt
 else
	@echo MXE environment not installed, windows program not built
 endif

simviewer.exe$(EXEEXT): mswin Makefile_simviewer_win.qt $(simviewer_SOURCES)
 ifdef MXE_QMAKE
	make -f Makefile_simviewer_win.qt
 else
	@echo MXE environment not installed, windows program not built
 endif

simmsg.exe$(EXEEXT): mswin Makefile_simmsg_win.qt $(simmsg_SOURCES)
 ifdef MXE_QMAKE
	make -f Makefile_simmsg_win.qt
 else
	@echo MXE environment not installed, windows program not built
 endif

edt2spike2.exe$(EXEEXT): mswin Makefile_edt2spike2_win.qt $(edt2spike2_exe_SOURCES)
 ifdef MXE_QMAKE
	make -f Makefile_edt2spike2_win.qt
 else
	@echo MXE environment not installed, windows program not built
 endif


Makefile_edt2spike2_win.qt: edt2spike2_win.pro
 ifdef MXE_QMAKE
	x86_64-w64-mingw32.static-qmake-qt5 edt2spike2_win.pro -r -spec win32-g++ 'DEFINES+=VERSION=\\\"$(VERSION)\\\"'
 endif

makesine.exe$(EXEEXT): mswin Makefile_makesine_win.qt $(makesine_exe_SOURCES)
 ifdef MXE_QMAKE
	make -f Makefile_makesine_win.qt
 else
	@echo MXE environment not installed, windows program not built
 endif

Makefile_makesine_win.qt: makesine_win.pro
 ifdef MXE_QMAKE
	x86_64-w64-mingw32.static-qmake-qt5 makesine_win.pro -r -spec win32-g++ 'DEFINES+=VERSION=\\\"$(VERSION)\\\"'
 endif

wave2daq.exe$(EXEEXT): mswin Makefile_wave2daq_win.qt $(wave2daq_exe_SOURCES)
 ifdef MXE_QMAKE
	make -f Makefile_wave2daq_win.qt
 else
	@echo MXE environment not installed, windows program not built
 endif

Makefile_wave2daq_win.qt: wave2daq_win.pro
 ifdef MXE_QMAKE
	x86_64-w64-mingw32.static-qmake-qt5 wave2daq_win.pro -r -spec win32-g++ 'DEFINES+=VERSION=\\\"$(VERSION)\\\"'
 endif

Makefile_simbuild.qt: simbuild.pro Makefile.am configure.ac
	qmake simbuild.pro -r 'DEFINES+=VERSION=\\\"$(VERSION)\\\"' 'DEFINES+=DEBUG_OR_NOT=\\\"$(DEBUG_OR_NOT)\\\"'


Makefile_simviewer.qt: simviewer.pro Makefile.am configure.ac
	qmake simviewer.pro -r 'DEFINES+=VERSION=\\\"$(VERSION)\\\"' 'DEFINES+=DEBUG_OR_NOT=\\\"$(DEBUG_OR_NOT)\\\"'

Makefile_simrun.qt: simrun.pro Makefile.am
	qmake simrun.pro -r 'DEFINES+=VERSION=\\\"$(VERSION)\\\"' 'DEFINES+=DEBUG_OR_NOT=\\\"$(DEBUG_OR_NOT)\\\"'

Makefile_simmsg.qt: simmsg.pro Makefile.am configure.ac
	@echo $(BUILT_SOURCES)
	qmake simmsg.pro -r 'DEFINES+=VERSION=\\\"$(VERSION)\\\"' 'DEFINES+=DEBUG_OR_NOT=\\\"$(DEBUG_OR_NOT)\\\"'

Makefile_simbuild_win.qt: simbuild.pro Makefile.am
 ifdef MXE_QMAKE
	x86_64-w64-mingw32.static-qmake-qt5 simbuild.pro -r -spec win32-g++ 'DEFINES+=VERSION=\\\"$(VERSION)\\\"'
 endif

Makefile_simmsg_win.qt:  Makefile.am simmsg.pro
 ifdef MXE_QMAKE
	x86_64-w64-mingw32.static-qmake-qt5 simmsg.pro -r -spec win32-g++ 'DEFINES+=VERSION=\\\"$(VERSION)\\\"'
 endif

Makefile_simviewer_win.qt: simviewer.pro Makefile.am
 ifdef MXE_QMAKE
	x86_64-w64-mingw32.static-qmake-qt5 simviewer.pro -r -spec win32-g++ 'DEFINES+=VERSION=\\\"$(VERSION)\\\"'
 endif

Makefile_simrun_win.qt: simrun.pro Makefile.am
 ifdef MXE_QMAKE
	x86_64-w64-mingw32.static-qmake-qt5 simrun.pro -r -spec win32-g++ 'DEFINES+=VERSION=\\\"$(VERSION)\\\"'
 endif

Makefile_snd2sim_win.qt: snd2sim.pro Makefile.am
 ifdef MXE_QMAKE
	x86_64-w64-mingw32.static-qmake-qt5 snd2sim.pro -r -spec win32-g++ 'DEFINES+=VERSION=\\\"$(VERSION)\\\"'
 endif

moc_%.cpp: %.h
	@MOC@ -o $@ $<

ui_%.h: %.ui
	@UIC@ -o $@ $<

qrc_%.cpp: %.qrc
	@RCC@ -o $@ $<

checkin_files = $(simbuild_code) $(simviewer_code) $(dist_doc_DATA) $(checkin_pD) $(simrun_exe_SOURCES) $(EXTRA_DIST) Makefile.am configure.ac

checkin_release:
	git add $(checkin_files) && git commit -uno -S -m "Release files for version $(VERSION)"

checkpoint:
	git add $(checkin_files) && git commit -uno -S -q -m "checkpoint files for version $(VERSION)"

checkpoint_withcomment:
	git add $(checkin_files) && git commit -uno -S -q

deb:
	@echo 'Making debian packages'
	make distdir &&\
	cd $(distdir) &&\
	dpkg-buildpackage -Zgzip -uc -us && \
	cd .. && \
	rm -rf $(distdir)

