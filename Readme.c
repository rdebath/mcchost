#pragma GCC diagnostic ignored "-Wcomment"

/*HELP readme
This is a Minecraft classic server with CPE (Classic Protocol Extensions)
Many of the CPE extensions are not implemented yet.
See "/help todo" for notes.

*/

/*HELP todo
 +) ini file format allowed for *.cw file -- trivial level create.
 +) Sort map names!
 +) Use link(2) for backup iff overwritten map/*.c is old.

 +) INI files, fields with integer values as fixed point *32, *256 etc

 +) Command that sets level properties using ini file loader. (inprogress)
    -- List of options.
    -- Unsafe fields; x,y,z
    -- automatic refresh.

 +) Command line option to restart server and unload levels.
    -- So we know this version will run! Find pid by port no.
    -- SIGALRM: Command line routine to "unload" level file

 +) load/save level to *.cw and use for backups, restores and "unload".
    -- Load/unload working

 +) Is adapting the queue length to the download size a good idea?
    Perhaps the loaded CW file size would be better.

 +) Multiple Levels. (/newlvl, /goto, /main, /levels) (inprogress)

 +) Block/User history records.
    -- Combined history needs user id numbers --> user file/table.

 +) NAME: (*) MCCHost

 +) User prefix/suffix for multiple heartbeat servers.
 +) Backup while physics running --> Copylvl then freeze physics.
    Copylvl can more easily do patchups.

 +) /spawn command
 +) /tp command
 +) /afk command (and auto)
 +) /mode command (Grass, bedrock, water etc)
 +) /info command
 +) /about command

 +) Add command to save all help texts as real files. (no overwrite)

 +) Run external command with stdout/err sent to client (</dev/null)
    -- Map gen?

 +) Maybe embed called commands: curl and gdb(stacktrace)
 +) Maybe exec($0, ...) on accept()

Features:
    -- CW file also contains pending physics operations.
    -- edlin ? to edit help files and blockdef prop files?

When are cw files saved?
    Part 1 -- Unload level.
	Save new cw file to maps/levelname.tmp
	Rename to maps/levelname.cw
	Unload level: remove uncompressed level file.

    Part 2 -- backup level
	copy maps/levelname.cw to backups/levelname.tmp
	Rename to backups/levelname/...

All files in subdirectories.
    system.ini  --> move?
    model.cw    --> move?
    map/${level}.cw
    system/{...}
    level/${level}.*
    tmp/unpack.level.id.* -- museum unpacks?
    help/${helpname}.txt
    blockdb/levelname.bdb
    log/YYYY-MM-DD.log
    backup/${level}.${id}.cw -- museum
    recycle-bin/${level}.cw

/Help command -- Should the default files be created ?

Physics:

Physics queue is a level/${level}.* file which can be extended at any time.

Basic
    Grass, Slabs
    Falling sand/gravel
    Finite? Water/Lava
    Sponge (Lava Sponge?)

Extra
    Leaves removed if !Log
    Shrub grows to tree

Advanced
    Lava(Sand) -> Glass
    Water(Lava) -> Stone
    Fire(TNT) -> Explosion
    Fire(...) -> Fire
    Time(Fire) -> Air



Implementation:

Other components
    -- physics process does non-trivial physics.
    -- crash catcher restarts server process without dropping connection.
    -- physics save to *.cw -- locking modes ?

Make sure text anti-spam is in place.
    (Only echo duplicates back to self?)
    Ghost the player? (for a short time?)

On Block queue, if user count < 2 && no-physics don't use queue ?
    --> *256 blocks packet need queue?

More blocks modified than will fit in queue?
    --> Can only see for "me" so generation number will wrap and reload.
    Mass change can pre-supress block queue and force generation rollover.

Logging:
    -- Automatic log file cleanup?
    -- Log user commands ?

Map filenames/level names: Use %XX encoding all CP437 characters are case sensitive.  A search will be done for case insensitive and partial matches if the exact name fails.


// vim:set syntax=none: */
