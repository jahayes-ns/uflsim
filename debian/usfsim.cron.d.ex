#
# Regular cron jobs for the usfsim package
#
0 4	* * *	root	[ -x /usr/bin/usfsim_maintenance ] && /usr/bin/usfsim_maintenance
