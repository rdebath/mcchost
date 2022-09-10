
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include <sys/time.h>

#include "client_list.h"

/* Note: I've made the choice to have one table for all users, not one
 * per level. That sets the maximum allowed number of users to 255.
 *
 * In the event that this is increased the user lists sent to the clients
 * will have to be renumbered.
 */

#if INTERFACE
#include <time.h>
#include <sys/types.h>

#define MAX_USER	255
#define MAX_LEVEL	255
#define MAGIC_USR	0x0021FF7E

typedef struct client_entry_t client_entry_t;
struct client_entry_t {
    nbtstr_t name;
    nbtstr_t client_software;
    int on_level;
    int level_bkp_id;
    xyzhv_t posn;
    uint8_t active;
    uint8_t visible;
    uint8_t ip_dup;
    uint32_t ip_address;
    time_t last_move;
    uint8_t is_afk;
    uint8_t client_proto_ver;
    uint8_t client_cpe;
    pid_t session_id;
};

struct client_level_t {
    int loaded;
    int backup_id;
    int no_unload;
    int force_unload;
    int delete_on_unload;
    nbtstr_t level;
};

typedef struct client_data_t client_data_t;
struct client_data_t {
    int magic1;
    uint32_t generation;
    uint32_t cleanup_generation;
    client_entry_t user[MAX_USER];
    client_level_t levels[MAX_LEVEL];
    int magic2;
};
#endif

int my_user_no = -1;
static client_entry_t myuser[MAX_USER];
static struct timeval last_check;

xyzhv_t player_posn = {0};
block_t player_held_block = -1;
time_t player_last_move;

void
check_user()
{
    if (my_user_no < 0 || my_user_no >= MAX_USER || !shdat.client) return;

    //TODO: Attempt a session upgrade -- call exec().
    if (shdat.client->magic1 != MAGIC_USR || shdat.client->magic2 != MAGIC_USR)
	fatal("Session upgrade failure, please reconnect");

    if (!shdat.client->user[my_user_no].active) {
	shdat.client->user[my_user_no].session_id = 0;
	logout("(Connecting on new session)");
    }

    struct timeval now;
    gettimeofday(&now, 0);
    if (last_check.tv_sec == 0) last_check = now;

    uint32_t msnow = now.tv_sec * 1000 + now.tv_usec / 1000;
    uint32_t mslast = last_check.tv_sec * 1000 + last_check.tv_usec / 1000;
    if (msnow-mslast<100 || msnow==mslast)
	return;
    last_check = now;

    int my_level = shdat.client->user[my_user_no].on_level;
    if (my_level == -1) my_level = -2; //NOPE

    for(int i=0; i<MAX_USER; i++)
    {
	if (i == my_user_no) continue; // Me
	// Note: slurp it so a torn struct is unlikely.
	// But don't lock as we're not too worried.
	client_entry_t c = shdat.client->user[i];

	// Is this user visible.
	c.visible = (c.active && c.on_level == my_level && c.level_bkp_id >= 0);

	if (extn_extplayerlist) {
	    if (c.active && (!myuser[i].active ||
		    myuser[i].on_level != c.on_level ||
		    myuser[i].is_afk != c.is_afk) ) {
		char buf[256] = "";
		char buf2[256] = "";
		if (c.on_level >= 0 && c.on_level < MAX_LEVEL) {
		    int l = c.on_level;
		    nbtstr_t n = shdat.client->levels[l].level;
		    if (shdat.client->levels[l].loaded) {
			if (shdat.client->levels[l].backup_id == 0)
			    snprintf(buf, sizeof(buf), "On %s", n.c);
			else if (shdat.client->levels[l].backup_id > 0)
			    snprintf(buf, sizeof(buf), "Museum %d %s", shdat.client->levels[l].backup_id, n.c);
			else
			    snprintf(buf, sizeof(buf), "Nowhere");
		    }
		}
		snprintf(buf2, sizeof(buf2), "&e%s%s", c.name.c, c.is_afk?" &7(AFK)":"");
		send_addplayername_pkt(i, c.name.c, buf2, buf, 0);

		if (c.visible == myuser[i].visible)
		    myuser[i] = c;
	    } else if (!c.active && myuser[i].active) {
		send_removeplayername_pkt(i);
		myuser[i].active = 0;
	    }
	}
	if (c.visible && !myuser[i].visible) {
	    // New user.
	    send_addentity_pkt(i, c.name.c, c.name.c, c.posn);
	    myuser[i] = c;
	} else
	if (!c.visible && myuser[i].visible) {
	    // User gone.
	    send_despawn_pkt(i);
	    myuser[i].visible = 0;
	} else if (c.visible) {
	    // Update user
	    send_posn_pkt(i, &myuser[i].posn, c.posn);
	}
    }

    if (server->afk_kick_interval > 60)
	if (player_last_move + server->afk_kick_interval < now.tv_sec) {
	    my_user.dirty = 1;
	    my_user.kick_count++;
	    logout("Auto-kick, AFK");
	}

    if (!myuser[my_user_no].is_afk && server->afk_interval >= 60) {
	if (player_last_move + server->afk_interval < now.tv_sec) {
	    myuser[my_user_no].is_afk = 1;
	    shdat.client->user[my_user_no].is_afk = 1;

	    printf_chat("@&S-&7%s&S- &Sis AFK auto", user_id);
	}
    }

    if (my_level >= 0 && my_level < MAX_LEVEL) {
	int go_main = 0;
	if (!shdat.client->levels[my_level].loaded) go_main = 1;
	else if (shdat.client->levels[my_level].force_unload) go_main = 1;

	if (go_main) {
	    printf_chat("You are being moved to main as %s was unloaded",
		shdat.client->levels[my_level].level.c);
	    cmd_main(0,0);
	    if (alarm_handler_pid != 0)
		kill(alarm_handler_pid, SIGALRM);
	}
    }
}

int check_level(char * levelname, char * UNUSED(levelfile))
{
    for(int i=0; i<MAX_LEVEL; i++) {
	if (!shdat.client->levels[i].loaded) continue;
	nbtstr_t n = shdat.client->levels[i].level;
	if (strcmp(n.c, levelname) == 0 && shdat.client->levels[i].backup_id == 0) {
	    if (shdat.client->levels[i].force_unload)
		return 0;
	    return 1;
	}
    }
    return 1;
}

void
reset_player_list()
{
    for(int i=0; i<MAX_USER; i++) {
	if (myuser[i].visible)
	    send_despawn_pkt(i);
	myuser[i].visible = myuser[i].active = 0;
    }

    if (level_prop) {
	send_addentity_pkt(255, user_id, user_id, level_prop->spawn);
    }
    if (player_posn.valid)
	send_posn_pkt(255, 0, player_posn);
    else {
	player_posn = level_prop->spawn;
	myuser[my_user_no].posn = level_prop->spawn;
	if (shdat.client)
	    shdat.client->user[my_user_no].posn = level_prop->spawn;
    }

    last_check.tv_sec = 1;
}

void
update_player_pos(pkt_player_posn pkt)
{
    player_posn = pkt.pos;
    if (my_user_no < 0 || my_user_no >= MAX_USER) return;

    player_held_block = pkt.held_block;

    // No movement, ignore
    xyzhv_t p1 = pkt.pos;
    xyzhv_t p2 = myuser[my_user_no].posn;
    if (p1.x == p2.x && p1.y == p2.y && p1.z == p2.z &&
        p1.v == p2.v && p1.h == p2.h)
	return;

    myuser[my_user_no].posn = pkt.pos;
    if (shdat.client)
	shdat.client->user[my_user_no].posn = pkt.pos;

    // Allow for pushing
    if ( abs(p1.x-p2.x)>2 || abs(p1.y-p2.y)>32 || abs(p1.z-p2.z)>2 ||
	abs(p1.v-p2.v)>1 || abs(p1.h-p2.h)>1) {

	update_player_move_time();
    }
}

void
update_player_move_time()
{
    if (my_user_no < 0 || my_user_no >= MAX_USER) return;
    player_last_move = time(0);
    if (!shdat.client) return;
    shdat.client->user[my_user_no].last_move = player_last_move;
    shdat.client->user[my_user_no].is_afk = 0;

    if (myuser[my_user_no].is_afk) {
	printf_chat("@&a-&7%s&a- &Sis no longer AFK", user_id);
	myuser[my_user_no].is_afk = 0;
    }
}

void
update_player_software(char * sw)
{
    if (my_user_no < 0 || my_user_no >= MAX_USER) return;
    if (!shdat.client) return;
    nbtstr_t b = {0};
    strcpy(b.c, sw);
    shdat.client->user[my_user_no].client_software = b;
}

void
start_user()
{
    int new_one = -1, kicked = 0;

    lock_fn(system_lock);
    if (!shdat.client) fatal("Connection failed");

    for(int i=0; i<MAX_USER; i++)
    {
	if (shdat.client->user[i].active != 1) {
	    if (new_one == -1 && shdat.client->user[i].session_id == 0)
		new_one = i;
	    continue;
	}
	nbtstr_t u = shdat.client->user[i].name;
	if (strcmp(user_id, u.c) == 0) {
	    if (shdat.client->user[i].session_id == 0 ||
		(kill(shdat.client->user[i].session_id, 0) < 0 && errno != EPERM))
	    {
		// Must have died.
		printf_chat("&SNote: Wiped old session.");
		shdat.client->generation++;
		shdat.client->user[i].active = 0;
		shdat.client->user[i].session_id = 0;
		if (new_one == -1) new_one = i;
		continue;
	    }
	    if (!user_authenticated)
		fatal("Already logged in!");
	    shdat.client->user[i].active = 0;
	    kicked++;
	}
    }

    if (new_one < 0 || new_one >= MAX_USER) {
	unlock_fn(system_lock);
	if (kicked)
	    fatal("Too many sessions, please try again");
	else
	    fatal("Too many sessions already connected");
    }

    my_user_no = new_one;
    client_entry_t t = {0};
    strcpy(t.name.c, user_id);
    t.active = 1;
    t.session_id = getpid();
    t.client_software = client_software;
    t.client_proto_ver = protocol_base_version;
    t.client_cpe = cpe_enabled;
    t.on_level = -1;
    t.level_bkp_id = -1;
    t.ip_address = client_ipv4_addr;
    shdat.client->user[my_user_no] = t;
    shdat.client->generation++;

    myuser[my_user_no] = shdat.client->user[my_user_no];
    player_last_move = time(0);
    unlock_fn(system_lock);
}

void
start_level(char * levelname, char * levelfile, int backup_id)
{
    nbtstr_t level = {0};
    int level_id = -1;

    strcpy(level.c, levelname);
    strcpy(current_level_name, levelname);
    strcpy(current_level_fname, levelfile);
    current_level_backup_id = backup_id;
    player_posn.valid = 0;

    lock_fn(system_lock);

    for(int i=0; i<MAX_LEVEL; i++) {
	if (!shdat.client->levels[i].loaded) {
	    if (level_id == -1) level_id = i;
	    continue;
	}
	nbtstr_t n = shdat.client->levels[i].level;
	if (strcmp(n.c, level.c) == 0 && shdat.client->levels[i].backup_id == backup_id) {
	    level_id = i;
	    break;
	}
    }

    if (level_id<0)
	fatal("Too many levels loaded");

    if (!shdat.client->levels[level_id].loaded) {
	client_level_t t = {0};
	t.level = level;
	t.loaded = 1;
	t.backup_id = backup_id;
	shdat.client->levels[level_id] = t;
    }

    shdat.client->user[my_user_no].on_level = level_id;
    shdat.client->user[my_user_no].level_bkp_id = backup_id;
    shdat.client->generation++;

    unlock_fn(system_lock);

    if (extn_extplayerlist) {
	char buf[256] = "";
	if (current_level_backup_id == 0)
	    snprintf(buf, sizeof(buf), "On %s", current_level_name);
	else if (current_level_backup_id > 0)
	    snprintf(buf, sizeof(buf), "Museum %d %s", current_level_backup_id, current_level_name);
	else
	    strcpy(buf, "Nowhere");
	send_addplayername_pkt(255, user_id, user_id, buf, 0);
    }
}

void
stop_user()
{
    if (my_user.user_logged_in)
	write_current_user(2);
    my_user.user_logged_in = 0;

    if (my_user_no < 0 || my_user_no >= MAX_USER) return;
    if (!shdat.client) return;

    if (shdat.client->user[my_user_no].active) {
	shdat.client->generation++;
	shdat.client->user[my_user_no].active = 0;
	shdat.client->user[my_user_no].session_id = 0;
    }
}

int
delete_session_id(int pid, char * killed_user, int len)
{
    int cleaned = 0;
    open_client_list();
    if (!shdat.client) return 0;
    for(int i=0; i<MAX_USER; i++)
    {
	int wipe_this = 0;
	if (pid > 0 && shdat.client->user[i].active == 1 &&
		       shdat.client->user[i].session_id == pid)
	    wipe_this = 1;

	if (pid <= 0 && shdat.client->user[i].session_id > 0) {
	    if (kill(shdat.client->user[i].session_id, 0) < 0 && errno != EPERM)
		wipe_this = 1;
	}

	if (wipe_this)
	{
	    // Increment kick count, no lock
	    userrec_t user_rec;
	    if (read_userrec(&user_rec, shdat.client->user[i].name.c, 0) == 0) {
		user_rec.kick_count++;
		user_rec.dirty = 1;
		write_userrec(&user_rec, 0);
	    }

	    cleaned ++;
	    if (killed_user)
		snprintf(killed_user, len, "%s", shdat.client->user[i].name.c);
	    printlog("Wiped session %d (%.32s)", i, shdat.client->user[i].name.c);
	    shdat.client->generation++;
	    shdat.client->user[i].session_id = 0;
	    shdat.client->user[i].active = 0;
	    if (pid>0) break;
	}
    }
    stop_client_list();
    return cleaned;
}

int
current_user_count()
{
    int flg = (shdat.client == 0);
    if(flg) open_client_list();
    if (!shdat.client) return 0;
    int users = 0;
    for(int i=0; i<MAX_USER; i++) {
	if (shdat.client->user[i].active == 1)
	    users ++;
    }
    if(flg) stop_client_list();
    return users;
}

int
unique_ip_count()
{
    int flg = (shdat.client == 0);
    if(flg) open_client_list();
    if (!shdat.client) return 0;

    // No need to lock -- unimportant statistic.
    int users = 0;
    int ip_addrs = 0;

    for(int i=0; i<MAX_USER; i++)
	shdat.client->user[i].ip_dup = 0;

    for(int i=0; i<MAX_USER; i++) {
	if (shdat.client->user[i].active == 1) {
	    users ++;
	    if (shdat.client->user[i].ip_dup == 0) {
		ip_addrs ++;
		for(int j = i+1; j<MAX_USER; j++) {
		    if (shdat.client->user[j].active == 1 &&
			shdat.client->user[i].ip_address ==
			shdat.client->user[j].ip_address)
		    {
			shdat.client->user[j].ip_dup = 1;
		    }
		}
	    }
	}
    }

    if(flg) stop_client_list();
    return ip_addrs;
}
