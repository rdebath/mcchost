#pragma GCC diagnostic ignored "-Wcomment"
// This file will be removed when it gets small.

/*HELP todo
 +) Command that sets level properties using ini file loader. (inprogress)

 +) load/save level to *.cw and use for backups, restores and "unload".
 +) /load level ini file from curl pipe ?

 +) Is adapting the queue length to the download size a good idea?
    Perhaps the loaded CW file size would be better.

 +) Multiple Levels. (/newlvl, /goto, /main, /levels) (inprogress)
 +) Block/User history records.
 +) /edlin for editing text files and virtual text files (blockdefs)

 +) Maybe embed called commands: gzip, curl and gdb(stacktrace)
 +) Maybe exec($0, ...) on accept()
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
 +) /fly command


Features:

Everything in level prop file into *.cw file. (base-done)
    -- CW file also contains pending physics operations.
    -- Command line routines to convert level files <--> cw file

    -- Virtual edlin ?
    -- Should automatic backup export server/level properties?

When are cw files saved?
    Part 1 -- Unload level. -- backup level (done)
	Save new cw file to maps/levelname.tmp
	Rename to maps/levelname.cw
	Unload level: remove uncompressed level file.

    Part 2 -- backup level
	copy maps/levelname.cw to backups/levelname.tmp
	Rename to backups/levelname/...

All files in subdirectories.
    system.ini	(TODO?)
    map/*.cw
    system/*.*
    level/levelname.*
    tmp/unpack.level.id.* -- museum unpacks
    conf/*.txt
    help/*.txt
    model.cw
    blockdb/levelname.bdb
    log/YYYY-MM-DD.log
    backup/...
    deleted ?

/Help command
    Search through source for "/*HELP name" and "/*HELPCMD name" comments.
    Place these in a C source file.
    Only use if matching file does not exist.
    ? Should the files be created ?

Physics:

Physics queue is a *.level.* file which can be extended at any time.

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
    Fire(...) -> Fire
    Time(Fire) -> Air



Implementation:

Other components
    -- physics process does non-trivial physics.
    -- crash catcher restarts server process without dropping connection.
    -- command to save level to *.cw file.
	-- All lock or copy/update modes.

Make sure text anti-spam is in place.
    (Only echo duplicates back to self?)
    Ghost the player? (for a short time?)

On Block queue, if user count < 2 && no-physics don't use queue ?
    --> *256 blocks packet need queue? Clear queue on overload?

More blocks modified than will fit in queue?
    --> Can only see for "me" so generation number will wrap and reload.
    Mass change can pre-supress block queue and force generation rollover.

Logging:
    -- Automatic log file cleanup?
    -- Log run commands ?
*/
