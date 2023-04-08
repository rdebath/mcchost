Starting from inittab
---

Add a line to /etc/inittab of the form:

`P2:23:respawn:/usr/local/bin/mcchost-server -no-detach -tcp -port 25565`

Signal init to start it with `kill -HUP 1`

The `-no-detach` argument is essential to ensure that init doesn't try to
start multiple copies. This will start the service as root, so it will
switch to the user "games" and store data in `/var/games/mcchost/`.

Starting from FreeBSD /etc/ttys
---

This works like inittab but with a slightly different syntax and no
"runlevels" feature so add a line of the form:

`25565   "/usr/local/bin/mcchost-server -no-detach -port" vt100       on secure`

Signal init to start it with `kill -HUP 1`

Starting with Systemd
---

See the [systemd](systemd) subdirectory

Starting with Systemd socket activation
---

See the [systemd-socket](systemd-socket) subdirectory

Starting in Microsoft WSL2 on Windows
---

See the [wsl2](wsl2) subdirectory
