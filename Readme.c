#pragma GCC diagnostic ignored "-Wcomment"

/*HELP readme
This is a Minecraft classic server with CPE (Classic Protocol Extensions)
See "/help todo" for notes.
*/

/*HELP welcome
&3Hi, this server is a very alpha application written in C.
&3It's likely to crash (especially if you &3use &T/crash&3)
&3and so far has rather limited features.
 
     &4♥ ♥ ♥ &dPlease try to crash it!&4 ♥ ♥ ♥
*/

/*HELP todo

 +) BulkBlockUpdate -- Hmmm.
    -- 8bit:  2048 --> 1282     8,  Min 161, pk 187
    -- 10bit: 2304 --> 1346     9,  Min 150, pk 166
    -- 8bit:  2560 --> 1286 web 10, Min 129, pk 150
    -- 10bit: 2816 --> 1350 web 11, Min 123, pk 136

    -- pk= sets in a 1500 byte packet
    -- Min= sets for bulk to be smaller

 +) ExtPlayerList
 +) TextHotKey
 +) SelectionCuboid -- Zone highlights

 +) EntityProperty -- model rotations
 +) TwoWayPing -- Standard ping is sufficient
 +) SetSpawnpoint

 +) VelocityControl -- Change movement now; /slap ?
 +) CustomParticles -- Make sparkles
 +) CustomModels -- Weird bots etc.

 +) Don't update backup time on delete of loaded map?

 +) Move more global variables into ini_settings structure.

 +) Record data rates for server bytes/s I/O

 +) Slow map loads; do I need to poll client chat?
    -- Background _load_ of maps as well as saves?

 +) INI file preserve (some?) comments.

 +) User name spaces.
    -- Do we place an affix onto display names ?
    -- Have #123 alias for any user, name collision workaround?

    -- "unauthenticated" for no salt public internet
       --> Do we want to merge into one user for all ?
       --> Use IP (People dislike seeing actual IPs) as user ?
       --> One/multiple connects per IP ?
       --> Use hash(IP) as real user and allow nicknames?

    -- "localnet" for connections from local network
    -- "secret_001" for a particular "salt" TTP.

    -- Normally "local" and "secret" would be the same namespace.

 +) Multiple servers in one directory ...
    -- Only have port to determine distinct config file.
    -- Distinct file is not saved, manual config only.
    -- One heartbeat server per config
    -- Config options suitable for distinct file are not loaded into system.dat

 +) Web server for texture files.
    -- Internal server or generic external inetd server?

 +) Dump/load userDB as file.
    -- Also single user and delete "username".
    -- Use mdb_dump and mdb_load ?

 +) Lowercase the uppercase CP437 extras? These: ÇÆÅÉÑÄÖÜ also Σσ and Φφ

 +) Improve the /set command, more help, option lists.
 +) reset_hotbar_on_mapload is a level option in the CW file ?
 +) Merge ini and nbt processing ?

 +) Config paths for "backup" and "map" directories ?
    -- Symlinks work now. Enough?

 +) /sendcmd command
 +) /summon command
 +) /info command
 +) /kick command
 +) /clones command

 +) More /cuboid variants ?

 +) Level limited chat -- primitive chatroom.
 +) Team chat, less primitive chatroom.

 +) /resizelvl, /copylvl, /savelvl (to mapdir)
 +) /backup & /restore
 +) /import -- download *.cw file from web.
    -- Rename file to strict % form, don't overwrite.
    -- Output of curl import command to user ?

 +) /newlvl, generate "nice" levels.

 +) Block/User history records.

 +) Permissions -- currently "client_trusted"
    -- Need enum of permit flags.
    -- Bitmap of allow/deny.
    -- Routine to construct bitmap from "nice" permissions.
    -- Routine takes care of "if has_build_perm(level)"
    -- /newlvl; No create, create personal, create any.

 +) NAME: (*) MCCHost

 +) Command to save all help texts as real files.
        no overwrite -- save to *.bak in this case?

 +) Run external command with stdout/err sent to client (</dev/null)
    -- Map gen? Import?
    -- Sent to level chat ? -- Physics?

 +) Scripting language interface
    -- Lua ?
    -- Python ?
    -- https://github.com/tweag/nickel ?

    -- External command that sends commands and queries as if it's
       the user? Map access ?

    -- External command that acts like a client?

 +) lmdb appears with Debian jessie, do I want a fallback ?
 +) Switch to using libcurl ?

 +) Safe /nick -- approximatly matches real name
    -- Match case insensitive, common substring.
    -- How to descibe "too different" to user?

Features:
    -- CW file also contains pending physics operations.
    -- edlin ? to edit help files and blockdef ini files?

All files in subdirectories.
    system.ini  --> move?
    model.cw    --> move?
    model.ini   --> move?
    map/${level}.cw
    system/chat.queue
    system/system.dat
    system/userlevels.dat
    level/${level}.*
    help/${helpname}.txt
    log/YYYY-MM-DD.log
    backup/${level}.${id}.cw

    ini/*.ini -- save level config
    level/${level}.${id}.* -- backup unpack
    blockdb/levelname.bdb

Physics:

Physics queue is a level/${level}.physics file.

Basic
    Grass, Slabs
    Falling sand/gravel
    Finite? Water/Lava
    Sponge (Lava Sponge?)

Extra
    Leaves removed if !Log
    Shrub grows to tree --> /tree command

Advanced
    Lava(Sand) -> Glass
    Water(Lava) -> Stone
    Fire(TNT) -> Explosion
    Fire(...) -> Fire
    Time(Fire) -> Air

Backup while physics running --> Copy level.* then freeze physics.
    Copy to backup level/.. files then save.
    Copy can more easily do patchups.

Physics with no users.
    Only run for a limited time; perhaps 15 minutes by default.

Implementation:

Other components
    -- physics process does non-trivial physics.
    -- crash catcher restarts server process without dropping connection.
    -- physics save to *.cw -- locking modes ?
    -- No Tickless physics ? Users make events.

Make sure text anti-spam is in place.
    (Only echo duplicates back to self?)
    Ghost the player? (for a short time?)

On Block queue, if user count < 2 && no-physics don't use queue ?
    --> *256 blocks packet need queue?
*/

/*
 Rejects ...

 +) inetd-tcp-wait mode; passes listening socket to process when there
    is a connection. The process must do an "accept()" on the socket
    to get the actual inetd style socket file descriptor. The process
    must also continue to do more accept() calls on the socket to allow
    later connections.
    -- Options --inetd and --tcp at same time?
    -- No advantage over --net --> Runs register every 30 seconds.
    -- No advantage over normal inetd mode --> connections are rare.

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

    -- Should -inetd imply (force?) -no-detach ?

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
