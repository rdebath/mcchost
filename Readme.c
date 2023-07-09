#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wcomment"
#endif

/*HELP readme
This is a Minecraft classic server with CPE (Classic Protocol Extensions)

See /cmds for a command list and /help <cmd> for each command.

See "/help todo" for build notes.
TODO: /help install
Other notes
    /help usecases
    /help todo cmds
*/

/*HELP todo
 +) Message blocks and portals don't need physics process.

 +) Improve the /setvar command, more help, option lists.
    -- /setvar user username var value ?
    -- /setvar with no section to level and system sections.

    -- command aliases
       -- /map /env /blockprops /os

 +) /tree -- Only Notch variant ?
 +) /fill
 +) /undo, /redo
 +) /sendcmd

 +) Command line to send HUP to server; for non-Linux updates.
    -- Or just time on exe ?
    -- Time on system.ini ?

 +) Update walkthrough for default server setup. (Readme.md)
    -- Including setting user to Admin with remote GUI.
    -- Include mini_chat for localhost processing.

 +) Exponential backups. (Delete old backups using +50% rule)
    for i in 2^k ; if file$i and file$(i*1.5) exist rm file $i

 +) UserType() { return group.Blocks[Block.Bedrock] ? 100 : 0; }

 +) /newlvl, more "nice" levels?
    -- Perlin based nature, with caves.
    -- Sub-themes, fcraft?

 +) Run external command ...
    -- stdin == /dev/null
    -- stdout to '/place', '/cuboid' command interpreter.
    -- stderr sent to client
    -- Map gen? Import? Other scripting?
    -- Theme --> Script file.

 +) BulkBlockUpdate -- Hmmm.
    -- 8bit:  2048 --> 1282     8,  Min 161, pk 187
    -- 10bit: 2304 --> 1346     9,  Min 150, pk 166
    -- 8bit:  2560 --> 1286 web 10, Min 129, pk 150
    -- 10bit: 2816 --> 1350 web 11, Min 123, pk 136

    -- pk= sets in a 1500 byte packet
    -- Min= sets for bulk to be smaller

 +) TextHotKey
 +) EntityProperty -- model rotations
 +) VelocityControl -- Change movement now; /slap ?
 +) CustomParticles -- Make sparkles
 +) CustomModels -- Weird bots etc.

 +) Record data rates for server bytes/s I/O

 +) Lowercase the uppercase CP437 extras? These: ÇÆÅÉÑÄÖÜ also Σσ and Φφ

 +) Team chat, less primitive chatroom than level chat.

 +) Block/User history records.
    -- Stored in a ${level} file normally.

    -- block-log directory.
    -- Has ${level}.${UTCDate}.23-59-59.blk{,.gz} files.
    -- UTCDate in ISO format.
    -- Datetime is last time in file.
    -- File created on first modification within timespan.
    -- Current file is renamed if level size changes; to current time.
    -- Max file size (approx?) limit also renames file.
    -- Old files are gzipped after midnight.
    -- Level unload does not touch file.
    -- Level deletes renames file to current time.
    -- Level rename leaves this behind like backups.
    -- Level copy leaves this behind like backups.

    -- Point in time restore ...
    -- File contains both before and after so can go forwards or backwards.
	(use forward scan mode though)

    -- Format same as MCG ??
	NO -- Physics blocks have IDs >767; I can support 16bit blocks.

	MCG_CHUNK_SZ 16
	int32 Player ID
	int32 Timestamp
	int32 Block offset
	int:10 Oldblock
	int:10 Newblock
	int:12 Update type --> 11 types.

	For MCC format place a 64 bit timestamp at start of file.
	Timestamps within file are 16.4 bits (one day).
	Block offset must be uint32

	int32 PlayerID
	uint32 Block Offset
	int16 Oldblock
	int16 Newblock
	int:17 Timestamp
	int:4 Update type --> 15 types

	int:11 Todo



 +) Permissions
    -- Need enum of permit flags.
    -- Bitmap of allow/deny commands and blocks.
    -- Routine to construct bitmap from "nice" permissions.
    -- Routine takes care of "if has_build_perm(level)"
    -- Permission for level owner (users, everyone)
    -- Permission for level readonly for "user" but modifiable for "level"
    -- Permission for /goto (in .ini file)

 +) IRC Chat bridge
    -- Default server and channel sourced from website (Option for none).
    -- Web site failure --> what sort of fallback? --> /etc/irc/servers ?
    -- http://www.ulduzsoft.com/libircclient/ ?
    -- Should this be an IRC server?

 +) NAME: (*) MCCHost

 +) INI file: Better way to preserve comments?
 +) lmdb appears with Debian jessie, do I want a fallback ?

 +) Crash catcher restarts server process without dropping connection.
    -- Don't work with web client (unless proxy).

All files in subdirectories.
    // Real data files.
    system.ini
    system.${port}.ini
    model.ini
    cmdset.ini
    map/${level}.cw
    help/${helpname}.txt
    user/${username}.ini
    secret/${username}.ini
    texture/*.zip
    ini/*.ini -- save level config, blocks etc

    // Backup and expiring data files (cleanup needed?)
    log/YYYY-MM-DD.log
    backup/${level}.${id}.cw

    // Semi temp; may have recent unique data.
    system/userdb32.mdb
    system/userdb32.mdb-lock
    level/${level}.*

    // Temp files
    system/*.lock
    system/chat.queue
    system/cmd.queue
    system/system.dat
    system/userlevels.dat
    level/${level}.${id}.* -- backup unpack

    // Todo (expiring?)
    blockdb/${level}.${id}.bdb -- id is rollover, day/week/megabyte.
    blockdb/${level}.${id}.bdb.gz

Features:
    -- CW file also contains pending physics operations.
    -- edlin ? to edit help files and blockdef ini files?

Physics:

Physics queue is a level/${level}.physics file.

Basic
    Grass ✓, Slabs ✓
    Falling sand/gravel ✓
    Finite? Water/Lava
    Sponge (Lava Sponge?)

Extra
    Leaves removed if !Log
    Shrub grows to tree --> /tree command
    Slow falling sand/gravel

Advanced
    Lava(Sand) -> Glass
    Water(Lava) -> Stone
    Fire(TNT) -> Explosion
    Fire(...) -> Fire
    Time(Fire) -> Air
    Deaths -> send to spawn, hell level or kick?

Backup while physics running --> Copy level.* then freeze physics.
    Copy to backup level/.. files then save.
    Copy can more easily do patchups.

Physics with no users.
    Only run for a limited time even if level stays loaded?
    Multiple levels take turns?

Other possible components
    -- physics process does non-trivial physics.
    -- physics save to *.cw ? -- locking modes ?
    -- physics save to *.phy file beside *.cw file ?
    -- Tickless physics ? Users make events.

On Block queue, if user count < 2 && no-physics don't use queue ?
    --> *256 blocks packet need queue?
*/

/*
 Rejects ...

 +) Allow more commands to be run without /pass set?
    -- /goto ?
 +) inetd-tcp-wait mode; passes listening socket to process when there
    is a connection. The process must do an "accept()" on the socket
    to get the actual inetd style socket file descriptor. The process
    must also continue to do more accept() calls on the socket to allow
    later connections.
    -- Options --inetd and --tcp at same time?
    -- No advantage over --net --> Runs register every 30 seconds.
    -- No advantage over normal inetd mode --> connections are rare.
 +) Merge ini and nbt processing ?
    -- Names are too random
 +) Config paths for "backup" and "map" directories ?
    -- Symlinks work now. Enough?
 +) Tinybox mode (proxy?)
    Create a map on a small VPS and send logs, backups etc to external storage.
    -- Use symlinks and sshfs.
 +) /import -- download *.cw file from web.
    -- Rename file to strict name form, don't overwrite.
    -- Output of curl import command to user ?
 +) First user is admin -- no, public server could be anyone.
*/

/*HELP usecases
mcchost-server -tcp [-detach]
    TCP server, if "-detach" is given backgrounds self after opening socket.
    The server process also schedules background tasks.

mcchost-server -net [-detach]
    TCP server with classicube.net registration. (Also -detach & tasks)
    The frequent registration is a background task.

mcchost-server -inetd
    Run from inetd; stdin & stdout are socket (Also used for systemd)
    On disconnect will usually unload maps.

    eg: socat TCP-LISTEN:25565,fork EXEC:'mcchost-server -inetd',nofork

mcchost-server -cron
    Run periodic tasks, map unloads, backups and classicube.net poll.
    Option --register is only the cc.net poll
    Option --cleanup is everything else.

mcchost-server -runonce
    Opens port and accepts one connection, without calling fork()
    Will not detach. Will not create logger process.
    -- Physics process?

mcchost-server -pipe
    Client starts process directly like -inetd, but with connection created
    by socketpair(AF_UNIX,) or two unnamed pipes.
    Mostly identical to -inetd, but -pipe is assumed to be trusted.

    eg: socat TCP-LISTEN:25565,reuseaddr EXEC:'mcchost-server -pipe',pipes

*/

// vim:set syntax=none: */
