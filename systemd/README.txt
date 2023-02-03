
The "unit file" mcchost.service sets up systemd to restart mcchost
in a similar fashion to inittab.

The files in the socket subdirectory allow systemd to use socket
activation in the style of inetd and cron.

