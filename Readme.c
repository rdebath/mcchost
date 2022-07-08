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
*/

/*HELP todo
 +) /afk command (and auto)
    -- Use to enhance crashed connection cleanup.

 +) Split -cron into curl and cleanup.
    -- Don't check any timeouts, run when requested.

 +) List of levels in backup directory.
    -- With backup number ranges.

 +) Use fixname for usernames too.

 +) Record data rates for server bytes/s I/O

 +) Slow map loads, do I need to poll client chat?

 +) INI file preserve (some?) comments.

 +) Decode Json return from cc.net, place in log.
    -- Or just record if different ?

 +) lmdb appears with Debian jessie, do I want a fallback ?

 +) /set readonly t/f -- does chmod ? Force save ?
    -- New Use case: save current and preserve.
    -- To preserve currently saved use a backup command.

 +) Key rotation, don't share real secret with TTP host ...
    -- use one secret but Hash(secret+postURL+time) for posted secret
    ++ Secret format is not limited by URL characters.

    -- The hash is used as a mixing function, the intermediates cannot
    -- be externally viewed or controlled so prefix/suffix attacks are
    -- not important. (?)

    -- Suffix attacks are a known plaintext style attack.
    -- Prefix attack is a HMAC extension attack.

    --> Use secret prefix layout as known plaintext _may_ be used to
        attack the hash algorithm.
    -- base32(MD5(Secret+PostURL+TIMECHUNK))

    -- Timechunk is time(0)\3600 (eg) for hourly changed keys,
       check 2/3 timechunks and each url.
    -- Check real secret too for users with access to server.ini file.
    -- cf. user namespaces.

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

 +) Multiple servers in one directory; which items should (not)be per port.
    -- Need port-12345.ini file ?
        --> Only record if different from system.ini values.

    -- "level & map & backup" need to be shared as a set due to locking.
        --> Declared only by system.ini

    -- Private flag? --> Heatbeat.
    -- CPE Disable? OPFlag?
    -- Localnet? --> Add bind address ?
    -- Salt? --> No, salt is not port specific.

    -- Heartbeat (Enable/disable, URL, URL-List? )
       --> Cross product between URLs and ports. (Secret is URL specific)
       --> host/port is (mostly) the unique key used by TTP.
       --> Heartbeat includes port number, so will need per port calls.
       --> URL should NOT need to be duplicated.
       --> Should sent salt include port number ? ... No.
       --> Private Flag (per heartbeat, per hearbeat+port?)

 +) We use MSG_PEEK so can pass the socket to a filtering process.
    -- Websocket, SSL, Web server.
    -- https://github.com/vi/websocat ?
    -- https login and web client download.

    -- websockify is 53MB (python, openssl)
    -- gdb is 115MB (python, openssl, readline)
    -- both are 150MB (python, openssl, readline)

 +) Dump/load userDB as ini file.
    -- Also single user and delete "username".
    -- Use mdb_dump and mdb_load ?

 +) Lowercase the uppercase CP437 extras? These: ÇÆÅÉÑÄÖÜ also Σσ and Φφ

 +) Add backup setting in server.ini, force backup of all map files before "date"

 +) Fix player detail "struct user"
    -- Time spent on server
    -- Kick
    -- IP address/clones

 +) Aliases in /set command, more help, option lists.
 +) reset_hotbar_on_mapload is a level option in the CW file ?
 +) Merge ini and nbt processing ?

 +) Config paths for "backup" and "map" directories ?
    -- Symlinks work now. Enough?

 +) /sendcmd command
 +) /about command
 +) /z and /m commands with setblock capture.

 +) /spawn command
 +) /tp command
 +) /summon command
 +) /info command

 +) Colour definitions for &S,&W etc.
    -- System level

 +) Level limited chat -- primitive chatroom.
 +) Team chat, less primitive chatroom.

 +) Command line option to restart server and unload levels.
    -- So we know this version will run!
    -- Find pid by port no.
    -- SIGALRM: Command line routine to "unload" level file
    -- Precheck so levels can be unloaded (saved) by old version.
    -- Precheck so users processes can be restarted too?
        -- Needs state save and reload for extn_* variables.

 +) /resizelvl, /copylvl, /deletelvl, /savelvl (to mapdir)
    -- deletelvl moves current into backup (not copy)
 +) /backup, /restore & /museum
 +) /import -- download *.cw file from web.
    -- Rename file to strict % form, don't overwrite.
    -- Output of curl import command to user ?

 +) /newlvl Perms; No create, create personal, create any.

 +) /newlvl, generate "nice" levels.

 +) Block/User history records.

 +) Permissions -- currently "client_trusted"
    -- Need enum of permit flags.
    -- Bitmap of allow/deny.
    -- Routine to construct bitmap from "nice" permissions.
    -- Routine takes care of "if has_build_perm(level)"

 +) NAME: (*) MCCHost

 +) Backup while physics running --> Copy level.* then freeze physics.
    Copy can more easily do patchups.

 +) Command to save all help texts as real files.
        no overwrite -- save to *.bak in this case?

 +) Run external command with stdout/err sent to client (</dev/null)
    -- Map gen? Import?
    -- Sent to level chat ? -- Physics?

 +) Log user commands ?
    -- Logging options; system level.

 +) Scripting language interface
    -- Lua ?
    -- Python ?
    -- https://github.com/tweag/nickel ?

    -- External command that sends commands and queries as if it's
       the user? Map access ?

    -- External command that acts like a client?

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
    backup/${level}.${id}.cw -- "museum"

    *.ini --> ?
    level/${level}.${id}.* -- museum unpack
    blockdb/levelname.bdb

    ${level} uses %2E for '.' in level name.

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

// vim:set syntax=none: */

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
    -- Need cron without cc.net poll?

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
