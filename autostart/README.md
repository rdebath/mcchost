Starting from a script
---

The deamon can be started from any script or the command line; it will detach from the current session and stay active until terminated.

`mcchost-server -port 25565 -net -detach`

The `-detach` argument is essential to ensure that the script is not blocked. If the port is already in use the startup will be paused for a few seconds before a retry, but if the retry fails it will give a prompt failure exit.


Starting from inittab
---

Add a line to /etc/inittab of the form:

`P2:23:respawn:/usr/local/bin/mcchost-server -no-detach -tcp -port 25565`

Signal init to start it with `kill -HUP 1`

The `-no-detach` argument is essential to ensure that init doesn't try to start multiple copies. This will start the service as root, so it will switch to the user "games" and store data in `/var/games/mcchost/`.

You can add the argument `-user myuser` or `-user myuser,mygroup` to select the user to use. In this case the working directory will be in that user's home directory.

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
