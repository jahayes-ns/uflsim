#
# Regular cron jobs for the simulator package
#
0 4	* * *	root	[ -x /usr/bin/simulator_maintenance ] && /usr/bin/simulator_maintenance
