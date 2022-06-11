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
 +) backup/ on different filesystem
 +) Aliases in /set command
 +) Allow for extend of .queue file without triggering reloads.
 +) Move server_t structure to shared memory. (Multiple ports?)

 +) /mode command.
 +) /about command
 +) /z and /m commands with setblock capture.

 +) Colour definitions for &S,&W etc.
 +) Level limited chat -- primitive chatroom.
 +) Team chat, less primitive chatroom.

 +) Create player detail "struct user" (for saving in SQL? blob store.)
    -- SQLite3 ? Multiple keys ? Just user names?
    -- https://github.com/symisc/unqlite  ??

 +) Command that sets level properties using ini file loader. (inprogress)
    -- List of options.

 +) Command line option to restart server and unload levels.
    -- So we know this version will run! Find pid by port no.
    -- SIGALRM: Command line routine to "unload" level file
    -- Precheck so levels can be unloaded by old version.
    -- Precheck so users processes can be restarted too?

 +) Multiple Levels. (/newlvl, /goto, /main, /maps) (inprogress)
 +) /resize, /copylvl, /deletelvl, /save(backup)
 +) /restore & /museum
 +) /newlvl, create cw file in ini file format.
 +) Import -- download *.cw file from web.

 +) Block/User history records.
    -- Combined history needs user id numbers --> user file/table.

 +) Permissions -- currently "client_ipv4_localhost"
    -- Need enum of permit flags.
    -- Bitmap of allow/deny.
    -- Routine to construct bitmap from "nice" permissions.
    -- Routine takes care of "if has_build_perm(level)"

 +) NAME: (*) MCCHost

 +) User prefix/suffix/namespace for multiple heartbeat servers.
 +) Backup while physics running --> Copy level.* then freeze physics.
    Copy can more easily do patchups.

 +) /spawn command
 +) /tp command
 +) /afk command (and auto)
 +) /info command

 +) Add command to save all help texts as real files.
	no overwrite -- save to *.bak in this case?

 +) Run external command with stdout/err sent to client (</dev/null)
    -- Map gen? Import?

 +) Log user commands ?

Features:
    -- CW file also contains pending physics operations.
    -- edlin ? to edit help files and blockdef prop files?

All files in subdirectories.
    system.ini  --> move?
    model.cw    --> move?
    model.ini   --> move?
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
    Shrub grows to tree --> /tree command

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

// vim:set syntax=none: */
