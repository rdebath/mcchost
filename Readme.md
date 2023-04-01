MCCHost is a **ClassiCube Server Software**

**Setup**
---
Clone the latest MCCHost from https://github.com/rdebath/mcchost

Compile the sources using the `make` command.

On a Debian or Ubuntu based system (Any CPU) you may need to install dependancies using the command
```
apt install build-essential liblmdb-dev zlib1g-dev
```

To run on Windows, install [Microsoft WSL 2](https://learn.microsoft.com/en-us/windows/wsl/install) and choose a Debian or Ubuntu distribution.

Alternatively, the `Dockerfile` can be used to create an image of the repository on either Linux or Windows hosts.

Other Unix derived OS's should be possible if the dependancies (ZLib, LMDB) are available. Note if `pthread_mutex.c` fails to compile or gives the error `Lockfile "system/system.lock" failed with EINVAL`, add the option `PTHREAD=` or delete `pthread_mutex.c`, recompiled and try again. (For example FreeBSD's pthread locking does not work)

If LMDB is not available (and disabled with `make DEFS='-DDISABLE_LMDB' LIBLMDB=`) certain command will fail to work correctly.

By default the server supports 10bit block numbers to reduce the uncompressed map size, and client memory allocation, this can be limited to 8 bits by the define `EIGHTBITMAP` add this in the `DEFS` variable in the form `make DEFS='-DEIGHTBITMAP'`.

<!--
Normal
$ make install
$ mcchost-server -tcp -detach -saveconf
$ mcchost-server

FreeBSD
$ gmake

macosx 11.4.2
$ make DEFS='-DDISABLE_LMDB' LIBLMDB= WARN=-w PTHREAD= -j install
-->

Starting your server
---
If you run `./server -tcp -logstderr` the server will be available for direct connection on the standard port 25565.
By default anyone connecting from localhost will be considered and admin. As such you can use `/setrank youruser admin` to make this work from any IP. If you can't arrange a connection from localhost the file `user/youruser.ini` is a text file where the line `UserGroup = 1` can be changed to `UserGroup = 0` to set admin.

The files for the server are normally stored in `~/.mcchost` but if it's started as root it'll change it's user id to `games` and use `/var/games/mcchost`. This directory can be configured by using the `-cwd` or `-dir [directory]` options.

To run the server in the background omit the `-logstderr` option and add `-detach`.
If you instead add a `-saveconf` option a default configuration file will be saved and the server will exit. Other options can be combined with `-savedconf` and their effect will be saved into the conf file.

Several options can be set from the command line (see: `./server -help`) or more in the `server.ini` file.
Only the `-dir` option (and `-port` if you are using multiple servers) need to be configured from the command line.

