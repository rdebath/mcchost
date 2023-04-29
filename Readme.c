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
 +) Add walkthrough for default server setup. (Readme.md)
    -- Including setting user to Admin with remote GUI.
    -- Include mini_chat ?

 +) Message blocks and portals don't need physics process.

 +) Improve the /setvar command, more help, option lists.
    -- /setvar user username var value ?
    -- /setvar with no section to level and system sections.

    -- command aliases
       -- /map /env /blockprops /os

 +) On /defblk of block used for edge or horizon need to redefine those.

 +) /sendcmd
 +) /clones
 +) /undo, /redo
 +) /fill
 +) /tree -- Only Notch variant ?

 +) Command line to send HUP to server; for non-Linux updates.
    -- Or just time on exe ?
    -- Time on system.ini ?

 +) Exponential backups. (Delete old backups using +50% rule)
    for i in 2^k ; if file$i and file$(i*1.5) exist rm file $i

 +) UserType() { return group.Blocks[Block.Bedrock] ? 100 : 0; }

 +) /newlvl, more "nice" levels?
    -- Perlin based nature, with caves.
    -- Sub-themes, fcraft?
    -- Fork off a background process to run the generate?

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
    -- Only saved into a compressed file on level unload
       -- NoUnload ? -- Or no users on level ?

 +) Permissions
    -- Need enum of permit flags.
    -- Bitmap of allow/deny commands and blocks.
    -- Routine to construct bitmap from "nice" permissions.
    -- Routine takes care of "if has_build_perm(level)"
    -- Permission for level owner (users, everyone)
    -- Permission for level readonly for "user" but modifiable for "level"
    -- Permission for /goto (in .ini file)

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

 +) Don't save a clean loaded map on delete.
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
 +) Slow map loads; do I need to poll client chat?
    -- Background _load_ of maps as well as saves?
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

/*HELP todo_cmds
Blocks      : Yes -- Special help pages (Dynamic ?)
Center      : Likely -- Drawing
Clones      : Yes -- List on same IP
Delete      : Yes -- Delete mode (physics blocks)
DoNotMark   : Yes -- Click to mark toggle (/dm)
Ignore      : Yes -- Remove chat's from some users
Loaded      : Yes -- Listing levels
Map         : Yes -- Map creation etc
Me          : Yes -- Chat hacks
MessageBlock: Yes
Top         : Likely
ViewRanks   : Likely
WhoNick     : Likely

Inbox       : Maybe -- Email like thing
LastCmd     : Maybe -- Userstats
Location    : Maybe -- http://ipinfo.io/" + ip + "/geo  -- Returns json
Redo        : Maybe -- BlockDB
Undo        : Maybe -- BlockDB
Whisper     : Maybe -- Chat hacks; All chat is @user

8ball       : Maybe -- Random ***
AdminChat   : Maybe -- Talk only to "Admin" aka SuperOP
Back        : Maybe -- Reverse last teleport
BanInfo     : Maybe -- Kick and ban records.
Calculate   : Maybe -- Very primitive calculator.
Color       : Maybe -- Sets Nick colour of player
Eat         : Maybe -- Random ***
Emote       : Maybe -- "Enables or disables emoticon parsing"
High5       : Maybe -- Chat hacks
Hug         : Maybe -- Chat hacks
LoginMessage: Maybe -- Chat hacks
MyNotes     : Maybe -- Kick and ban records.
OpChat      : Maybe -- Talk only to "Operator"
OpRules     : Maybe -- Text rules for "Operator"
OpStats     : Maybe -- Stats on bans/kicks
Report      : Maybe -- Kick/ban request mesg
Roll        : Maybe -- Random ***
Seen        : Maybe -- Player records 1st/last seen
Send        : Maybe -- Email like thing
TPA         : Maybe -- Teleport
Warp        : Maybe -- Named TP locations
ZoneList    : Maybe -- Permission zones
ZoneMark    : Maybe -- Permission zones
ZoneTest    : Maybe -- Permission zones

AFK         : Done
Abort       : Done
About       : Done
Clear       : Done
Commands    : Done
CrashServer : Done
FAQ         : Done
Goto        : Done -- Need fuzzy (strcase437str()) match
Hacks       : Done
Help        : Done
Levels      : Done -- Listing levels
Main        : Done -- need change main level
MapInfo     : Done -- Map creation etc
Mark        : Done -- For extra drawing etc.
Measure     : Done -- Nice stats
Mode        : Done -- Non-selectable blocks
Model       : Done -- CPE Extension control
Museum      : Done
News        : Done
Nick        : Done
PClients    : Done -- User stats.
Paint       : Done
Ping        : Done
Place       : Done (Needs block names)
Players     : Done -- Server side user list "/who"
Quit        : Done
RageQuit    : Done
Reload      : Done
Rules       : Done
ServerInfo  : Done
SetSpawn    : Done -- Levels
Skin        : Done -- CPE Extension control
Spawn       : Done -- Levels
TP          : Done -- Teleport
Time        : Done -- Random ***
Title       : Done
View        : Done -- Add auto list??
WhoIs       : Done -- /Info

AKA         : No -- Zombie Game
Awards      : No -- Games
Balance     : No -- Money system
Buy         : No -- Money system
CTF         : No -- CTF Game
CountDown   : No -- Countdown Game
Delay       : No -- Runs in message box only.
Economy     : No -- Money system
Fly         : No -- server side fly hack
HasIRC      : No -- IRC Bridge
LavaSurvival: No -- Lava Game
Pay         : No -- Money system
Ride        : No -- Physics block
Store       : No -- Money system
Team        : No -- Games
TntWars     : No -- TNT Game
Vote        : No -- Lava Game (&Zombie?)
ZombieSurvi : No -- Zombie Game

*/

// vim:set syntax=none: */
