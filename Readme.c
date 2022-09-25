#pragma GCC diagnostic ignored "-Wcomment"

/*HELP readme
This is a Minecraft classic server with CPE (Classic Protocol Extensions)
See "/help todo" for notes.
*/

/*HELP todo
 +) Fix "will create directories before erroring on arguments".

 +) Option AllowUserLevel

 +) Move "salt" to it's own file?
    -- Put in secret/ directory? (with 0600)
    -- Option for not saving salt to file
    -- Option for file location ?

 +) How to define default map theme and size.
    -- From model.ini ?
    -- If model.ini how to default seed.
    -- Copy model.cw for default newlvl ?
    -- If theme needs seed use mtime of file as default?

 +) curl https://ipapi.co/40.89.244.232/utc_offset
    -- user.utc_offset & user.utc_user_offset

 +) /backup, /restore and /undo
    -- Add "backup now" command.
        -- Save and *copy* to backup.
        -- Save with no last backup date? Or just a "manual backup" flag.

 +) Permission for level modifiable by anyone (with /set etc).

 +) Add VERSION computation to "/help license".

 +) Add max sessions from IP
 +) Add connect delay/fail for IPs recently seen.
 +) Antispam (Block, text, commands ?)
    -- If text is identical to previous do a special?

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

 +) Web server for texture files.
    -- Internal server or generic external inetd server?

 +) Delete player.
 +) Regenerate userDB from ini files. (Due to flaky format of lmdb files)

 +) Lowercase the uppercase CP437 extras? These: ÇÆÅÉÑÄÖÜ also Σσ and Φφ

 +) Improve the /set command, more help, option lists.

 +) Config paths for "backup" and "map" directories ?
    -- Symlinks work now. Enough?

 +) /sendcmd command
 +) /summon command
 +) /info /sinfo /minfo commands
 +) /kick command
 +) /clones command

 +) More /cuboid variants ?

 +) Level limited chat -- primitive chatroom.
 +) Team chat, less primitive chatroom.

 +) /resizelvl, /copylvl, /savelvl (to mapdir)
    -- Should resizelvl backup on shrink?

 +) /import -- download *.cw file from web.
    -- Rename file to strict name form, don't overwrite.
    -- Output of curl import command to user ?

 +) /newlvl, more "nice" levels?
    -- Perlin based nature, with caves.
    -- Sub-themes, fcraft?

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

 +) Tinybox mode (proxy?)
    Create a map on a small VPS and send logs, backups etc to external storage.
    -- One map mode?
    -- Use symlinks and sshfs ?

 +) Scripting language interface
    -- Lua ?
    -- Python ?
    -- https://github.com/tweag/nickel ?

    -- External command that sends commands and queries as if it's
       the user? Map access ?

    -- External command that acts like a client?

 +) INI file: Better way to preserve comments?
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
    system.${port}.ini  --> move?
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

 +) Don't update backup time on delete of loaded map?

 +) Slow map loads; do I need to poll client chat?
    -- Background _load_ of maps as well as saves?

 +) Merge ini and nbt processing ?
    -- Names are too random
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
