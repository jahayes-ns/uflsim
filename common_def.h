#ifndef COMMON_DEF_H
#define COMMON_DEF_H

// some items shared by simrun and simbuild, maybe simviewer

#define MAX_AFFERENT_PROB 32 /* this many afferent probabilities */


// std_filber is one less than lung vars, -1 to -16
// -18 is an external afferent signal plot with just the events
// -19 is an afferent with signal 
// -20 is an affernt with both signal and event
// -21 is sum of all fibers in 1 tick
// -22 is sum of all fibers in user defined # of tick
// Note: some lookup codes wants LAST_FIBER to be just that.
enum CellType {STD_CELL, BURSTER_CELL, PSR_CELL, LUNG_CELL, 
               STD_FIBER=-17, 
               AFFERENT_EVENT = -18, 
               AFFERENT_SIGNAL = -19, 
               AFFERENT_BOTH = -20,
               AFFERENT_INST = -21,
               AFFERENT_BIN = -22,
               LAST_FIBER = -22, // keep this up to date when adding new types
               FIRST_CELL_UNUSED = -100 // room for more fiber types, this not used
                                        // but should be if we add new cell plot types
             };
#endif
