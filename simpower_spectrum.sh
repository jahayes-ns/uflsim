#!/bin/sh

PLOT_DATA_FILE=power_spectrum_plot_data_$1
GNUPLOT_CMD_FILE=gnuplot_cmd_$1
true > $PLOT_DATA_FILE

simpickedt --analog_id 1 --offset 2048 --starttime spawn$1.$3 \
    | simtxt2flt \
    | simspectrum --detrend --window --period .512 --stepsize $2 --spawnnum $1\
    >> $PLOT_DATA_FILE

simmerge spawn$1.$3 ie$1.edt > merge$1.edt


if [ -x `which gnuplot` ] ; then
    echo "plot '$PLOT_DATA_FILE' with lines title 'POWER SPECTRUM'" > $GNUPLOT_CMD_FILE
    echo "pause mouse close" >> $GNUPLOT_CMD_FILE
    gnuplot -p $GNUPLOT_CMD_FILE
fi


