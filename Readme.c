#pragma GCC diagnostic ignored "-Wcomment"

/*HELP readme
This is a Minecraft classic server with CPE (Classic Protocol Extensions)
Many of the CPE extensions are not implemented yet.
See "/help todo" for notes.
*/

/*HELP welcome
&3Hi, this server is a very alpha application written in C.
&3It's likely to crash (especially if you &3use &T/crash&3)
&3and so far has rather limited features.
 
     &4♥ ♥ ♥ &dPlease try to crash it!&4 ♥ ♥ ♥
 
&eSource is at https://github.com/rdebath/mcchost
*/

/*HELP todo
 +) Command that sets level properties using ini file loader. (inprogress)
    -- List of options.

 +) Command line option to restart server and unload levels.
    -- So we know this version will run! Find pid by port no.
    -- SIGALRM: Command line routine to "unload" level file

 +) Multiple Levels. (/newlvl, /goto, /main, /maps) (inprogress)
 +) /goto + ... goto my map, bypass map existence check. 
 +) /resize
 +) /restore & /museum
 +) newlvl, create ini file.
 +) Import, download *.cw file from web.

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

 +) Add command to save all help texts as real files.
	no overwrite -- create *.bak in this case?

 +) Run external command with stdout/err sent to client (</dev/null)
    -- Map gen?

 +) Maybe embed called commands: curl and gdb(stacktrace)
 +) Maybe exec($0, ...) on accept()

Features:
    -- CW file also contains pending physics operations.
    -- edlin ? to edit help files and blockdef prop files?

All files in subdirectories.
    system.ini  --> move?
    model.cw    --> move?
    *.ini --> ?
    map/${level}.cw
    system/...
    level/${level}.*
    level/${level}.${id}.* -- museum unpack
    help/${helpname}.txt
    blockdb/levelname.bdb
    log/YYYY-MM-DD.log
    backup/${level}.${id}.cw -- museum
    backup/${level}.cw -- current backup
    recycle-bin/${level}.cw

    ${level} uses %2E for '.' in level name.


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

// vim:set syntax=none: */
