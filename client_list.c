
#include <signal.h>
#include <errno.h>
#include <assert.h>

#include "client_list.h"

/* Note: I've made the choice to have one table for all users, not one
 * per level. That sets the maximum allowed number of users to 128/255.
 *
 * The first 128 will be visible on a level.
 * The first 255 will be shown in the player list
 * If the client looks like ClassiCube visible players are first 255
 *
 * For a reasonable display if this increased, the list sent to the client
 * will have to be renumbered.
 */

#if INTERFACE
#include <sys/types.h>

#define MAX_USER	256
#define MAX_LEVEL	255

#define TY_MAGIC     0x1A7FFF00
#define TY_MAGIC2    0x557FFF00
#define TY_MAGIC3    (0x1A7FFF00 ^ (MAX_USER*33+MAX_LEVEL))
#define TY_VERSION   0x00000100

typedef struct client_entry_t client_entry_t;
struct client_entry_t {
    nbtstr_t name;
    nbtstr_t client_software;
    nbtstr_t listname;
    nbtstr_t skinname;
    nbtstr_t modelname;
    char name_colour;
    int on_level;
    int level_bkp_id;
    xyzhv_t posn;
    uint8_t active;
    uint8_t visible;
    uint8_t look_update_counter;
    uint8_t ip_dup;
    uint32_t ip_address;
    time_t last_move;
    uint8_t is_afk;
    uint8_t client_proto_ver;
    uint8_t client_cpe;
    uint8_t client_dup;
    uint8_t authenticated;
    uint8_t trusted;
    pid_t session_id;
};

typedef struct client_level_t client_level_t;
struct client_level_t {
    int backup_id;
    nbtstr_t level;
    uint8_t loaded;
    uint8_t in_use;
    uint8_t force_unload;
    uint8_t force_backup;
    uint8_t delete_on_unload;
};

typedef struct client_data_t client_data_t;
struct client_data_t {
    int magic3;
    uint32_t generation;
    uint32_t cleanup_generation;
    client_entry_t user[MAX_USER];
    client_level_t levels[MAX_LEVEL];
    int magic2;
    int version;
};
#endif

int my_user_no = -1;
static client_entry_t myuser[MAX_USER];
static struct timeval last_check;

xyzhv_t player_posn = {0};
block_t player_held_block = -1;
time_t player_last_move;
int player_on_new_level = 1;
int player_is_afk = 0;
aspam_t player_block_spam[1];
aspam_t player_cmd_spam[1];
aspam_t player_chat_spam[1];

nbtstr_t player_title_name;	// Name including colour and title
nbtstr_t player_list_name;	// Name with &Colour
nbtstr_t player_group_name;

void
check_user()
{
    if (my_user_no < 0 || my_user_no >= MAX_USER || !shdat.client) return;

    //TODO: Attempt a session upgrade -- call exec().
    if (shdat.client->magic3 != TY_MAGIC3 || shdat.client->magic2 != TY_MAGIC2 || shdat.client->version != TY_VERSION)
	fatal("Session upgrade failure, please reconnect");

    if (!shdat.client->user[my_user_no].active) {
	shdat.client->user[my_user_no].session_id = 0;
	logout("(Connecting on new session)");
    }

    struct timeval now;
    gettimeofday(&now, 0);
    if (last_check.tv_sec == 0) last_check = now;

    if (server->player_update_ms < 20) server->player_update_ms = 20;
    if (server->player_update_ms > 2000) server->player_update_ms = 2000;

    uint32_t msnow = now.tv_sec * 1000 + now.tv_usec / 1000;
    uint32_t mslast = last_check.tv_sec * 1000 + last_check.tv_usec / 1000;
    if (msnow-mslast<server->player_update_ms || msnow==mslast)
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
	int is_dirty = 0;

	// Is this user visible.
	c.visible = (c.active && c.authenticated && c.posn.valid &&
		c.on_level == my_level && c.level_bkp_id >= 0);

	if (c.visible && !myuser[i].visible)
	    myuser[i].look_update_counter--;

	if (extn_extplayerlist) {
	    if (!c.active && myuser[i].active) {
		if (i>=0 && i<255)
		    send_removeplayername_pkt(i);
		myuser[i].active = 0;
		is_dirty = 1;
	    } else if (c.active &&
		    (!myuser[i].active ||
		    myuser[i].look_update_counter != c.look_update_counter ||
		    myuser[i].on_level != c.on_level ||
		    myuser[i].is_afk != c.is_afk) ) {

		char groupname[256] = "Nowhere";
		if (c.on_level >= 0 && c.on_level < MAX_LEVEL) {
		    int l = c.on_level;
		    nbtstr_t n = shdat.client->levels[l].level;
		    if (shdat.client->levels[l].loaded) {
			if (shdat.client->levels[l].backup_id == 0)
			    saprintf(groupname, "On %s", n.c);
			else if (shdat.client->levels[l].backup_id > 0)
			    saprintf(groupname, "Museum %d %s", shdat.client->levels[l].backup_id, n.c);
		    }
		}

		char listname[256] = "";
		saprintf(listname, "%s%s", c.listname.c, c.is_afk?" &7(AFK)":"");

		send_addplayername_pkt(i, c.name.c, listname, groupname, 0);

		is_dirty = 1;
	    }
	}

	int upd_flg = (c.look_update_counter != myuser[i].look_update_counter);
	if (upd_flg || (!c.visible && myuser[i].visible)) {
	    // User gone.
	    send_despawn_pkt(i);
	    is_dirty = 1;
	}
	if ((upd_flg && c.visible) || (c.visible && !myuser[i].visible)) {
	    // New user.
	    char * skin = c.name.c;
	    if (c.skinname.c[0]) skin = c.skinname.c;
	    char namebuf[256];
	    if (c.name_colour) saprintf(namebuf, "&%c%s", c.name_colour, c.name.c);
	    else saprintf(namebuf, "&7%s", c.name.c);
	    revert_amp_to_perc(namebuf);
	    send_addentity_pkt(i, namebuf, skin, c.posn);
	    send_posn_pkt(i, 0, c.posn);
	    is_dirty = 1;
	} else if (c.visible) {
	    // Update user
	    send_posn_pkt(i, &myuser[i].posn, c.posn);
	    is_dirty = 1;
	}
	if (upd_flg) {
	    if ((c.modelname.c[0] != 0) || (myuser[i].modelname.c[0] != 0)) {
		send_changemodel_pkt(i, c.modelname.c);
		is_dirty = 1;
	    }
	}
	if (is_dirty)
	    myuser[i] = c;
    }

    if (server->afk_kick_interval > 60)
	if (player_last_move + server->afk_kick_interval < now.tv_sec) {
	    my_user.dirty = 1;
	    my_user.kick_count++;
	    logout("Auto-kick, AFK");
	}

    if (!player_is_afk && server->afk_interval >= 60) {
	if (player_last_move + server->afk_interval < now.tv_sec) {
	    player_is_afk = 1;
	    shdat.client->user[my_user_no].is_afk = 1;

	    printf_chat("@&S-%s&S- &Sis AFK auto", player_list_name.c);
	}
    }

    if (my_level >= 0 && my_level < MAX_LEVEL) {
	int go_main = 0;
	if (!shdat.client->levels[my_level].loaded) go_main = 1;
	else if (shdat.client->levels[my_level].force_unload) go_main = 1;

	if (go_main) {
#ifdef CMD_GOTOMAIN
	    if (strcmp(shdat.client->levels[my_level].level.c, main_level()) != 0)
	    {
		printf_chat("You are being moved to main as %s was unloaded",
		    shdat.client->levels[my_level].level.c);
		open_main_level();
	    }
	    else
#endif
	    {
		printf_chat("You have falled into the void as %s was unloaded",
		    shdat.client->levels[my_level].level.c);
		cmd_void(0,0);
	    }
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
	myuser[i].visible = 0;
	myuser[i].posn.valid = 0;
	if (shdat.client)
	    myuser[i].look_update_counter = shdat.client->user[i].look_update_counter;
    }

    reset_player_skinname();
    send_changemodel_pkt(-1, my_user.model);

    if (player_posn.valid)
	send_posn_pkt(-1, 0, player_posn);
    else {
	player_posn = level_prop->spawn;
	player_posn.v = 128; // Bots start with flip-head.
	player_posn.valid = 1;
	myuser[my_user_no].posn = player_posn;
	if (shdat.client)
	    shdat.client->user[my_user_no].posn = player_posn;
    }

    last_check.tv_sec = 1;
    player_on_new_level = 0;
}

void
reset_player_skinname()
{
    char * skin = user_id;
    if (my_user.skin[0]) skin = my_user.skin;
    char namebuf[256];

    if (my_user.colour[0]) saprintf(namebuf, "&%c%s", my_user.colour[0], user_id);
    else saprintf(namebuf, "&7%s", user_id);
    revert_amp_to_perc(namebuf);

    if (level_prop)
	send_addentity_pkt(-1, namebuf, skin, level_prop->spawn);
    else
	send_addentity_pkt(-1, namebuf, skin, player_posn);
}

void
update_player_pos(pkt_player_posn pkt)
{
    player_posn = pkt.pos;
    if (my_user_no < 0 || my_user_no >= MAX_USER) return;

#if 0
    if (!user_authenticated) {
	pos.y = -1023*32;
	send_posn_pkt(-1, 0, pos);
	return;
    }
#endif

    if (shdat.client)
	shdat.client->user[my_user_no].authenticated = user_authenticated;

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

    if (player_is_afk) {
	printf_chat("@&a-&7%s&a- &Sis no longer AFK", user_id);
	player_is_afk = 0;
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
update_player_look()
{
    if (!shdat.client || my_user_no < 0 || my_user_no >= MAX_USER) return;
    client_entry_t *t = &shdat.client->user[my_user_no];

    int oc = t->name_colour;
    t->name_colour = my_user.colour[0];
    if (t->name_colour == 0) {
	if (perm_is_admin())
	    t->name_colour = 'S';
	else
	    t->name_colour = '2';
    }
    if (oc != t->name_colour)
	t->look_update_counter ++;

    int c = t->name_colour;
    nbtstr_t namebuf;
    if (my_user.nick[0])
	saprintf(namebuf.c, "&%c%s", c, my_user.nick);
    else
	saprintf(namebuf.c, "&%c%s", c, user_id);

    if (my_user.title[0])
	saprintf(player_title_name.c, "&%c[%s&%c] %s", c, my_user.title, c, namebuf.c);
    else
	saprintf(player_title_name.c, "%s", namebuf.c);

    revert_amp_to_perc(namebuf.c);
    revert_amp_to_perc(player_title_name.c);

    if (strcmp(player_list_name.c, namebuf.c) != 0) {
	player_list_name = namebuf;
	if (extn_extplayerlist)
	    send_addplayername_pkt(-1, user_id, player_list_name.c, player_group_name.c, 0);
    }

    if (strcmp(namebuf.c, t->listname.c) != 0) {
	t->listname = namebuf;
	t->look_update_counter ++;
    }

    saprintf(namebuf.c, "%s", my_user.skin);
    if (strcmp(namebuf.c, t->skinname.c) != 0) {
	t->skinname = namebuf;
	t->look_update_counter ++;
    }

    saprintf(namebuf.c, "%s", my_user.model);
    if (strcmp(namebuf.c, t->modelname.c) != 0) {
	t->modelname = namebuf;
	t->look_update_counter ++;
    }

}

void
start_user()
{
    int new_one = -1, kicked = 0;

    lock_fn(system_lock);
    if (!shdat.client) fatal("Connection failed");

    int connected_sessions = 0;

    for(int i=0; i<MAX_USER; i++)
    {
	if (shdat.client->user[i].active != 1) {
	    if (new_one == -1 && shdat.client->user[i].session_id == 0)
		new_one = i;
	    continue;
	}
	connected_sessions++;
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
		quiet_logout("Already logged in!");
	    shdat.client->user[i].active = 0;
	    kicked++;
	}
    }

    if (server->max_players < 1) server->max_players = 1;
    if (server->max_players > MAX_USER) server->max_players = MAX_USER;
    if (server->max_players <= connected_sessions && !perm_is_admin())
	new_one = -1;

    if (new_one < 0 || new_one >= MAX_USER) {
	unlock_fn(system_lock);
	if (kicked)
	    quiet_logout("Too many sessions, please try again");
	else
	    quiet_logout("Too many sessions already connected");
    }

    my_user_no = new_one;
    client_entry_t t = {0};
    strcpy(t.name.c, user_id);
    t.active = 1;
    t.look_update_counter = 1;
    t.session_id = getpid();
    t.client_software = client_software;
    t.client_proto_ver = protocol_base_version;
    t.client_cpe = cpe_enabled;
    t.on_level = -1;
    t.level_bkp_id = -1;
    t.ip_address = client_ipv4_addr;
    t.authenticated = user_authenticated;
    t.trusted = 0;
    strcpy(t.listname.c, user_id);
    strcpy(t.skinname.c, user_id);
    strcpy(t.modelname.c, "");
    t.name_colour = 0;
    connected_sessions++;

    if (t.client_software.c[0] == 0) {
	if (t.client_cpe)
	    strcpy(t.client_software.c, "Classic 0.30/CPE");
	else if (t.client_proto_ver == 7)
	    strcpy(t.client_software.c, "Classic 0.28-0.30");
	else if (t.client_proto_ver == 6)
	    strcpy(t.client_software.c, "Classic 0.0.20-0.0.23");
	else if (t.client_proto_ver == 5)
	    strcpy(t.client_software.c, "Classic 0.0.19");
	else if (t.client_proto_ver == 4)
	    strcpy(t.client_software.c, "Classic 0.0.17-0.0.18");
	else if (t.client_proto_ver == 3)
	    strcpy(t.client_software.c, "Classic 0.0.16");
	else
	    strcpy(t.client_software.c, "Unknown");
    }

    shdat.client->user[my_user_no] = t;
    shdat.client->generation++;

    myuser[my_user_no] = shdat.client->user[my_user_no];
    player_last_move = time(0);
    server->connected_sessions = connected_sessions;
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
    player_posn = (xyzhv_t){0};
    player_on_new_level = 1;

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
	server->loaded_levels++;
    }

    if (my_user_no < 0 || my_user_no >= MAX_USER) {
	unlock_fn(system_lock);
	return;
    }

    shdat.client->user[my_user_no].on_level = level_id;
    shdat.client->user[my_user_no].level_bkp_id = backup_id;
    shdat.client->user[my_user_no].posn = (xyzhv_t){0};
    shdat.client->generation++;

    if (extn_extplayerlist) {
	char groupname[256] = "";
	if (current_level_backup_id == 0)
	    saprintf(groupname, "On %s", current_level_name);
	else if (current_level_backup_id > 0)
	    saprintf(groupname, "Museum %d %s", current_level_backup_id, current_level_name);
	else
	    strcpy(groupname, "Nowhere");
	saprintf(player_group_name.c, "%s", groupname);

	char listname[256] = "";
	if (shdat.client && my_user_no >= 0 && my_user_no < MAX_USER) {
	    client_entry_t *t = &shdat.client->user[my_user_no];
	    strcpy(listname, t->listname.c);
	} else
	    strcpy(listname, user_id);
	saprintf(player_list_name.c, "%s", listname);

	unlock_fn(system_lock);

	send_addplayername_pkt(-1, user_id, listname, groupname, 0);
    } else
	unlock_fn(system_lock);
}

void
stop_user()
{
    if (my_user.user_logged_in)
	write_current_user(2);
    my_user.user_logged_in = 0;
    close_userdb();

    if (my_user_no < 0 || my_user_no >= MAX_USER) return;
    if (!shdat.client) return;

    if (shdat.client->user[my_user_no].active) {
	shdat.client->generation++;
	shdat.client->user[my_user_no].active = 0;
	shdat.client->user[my_user_no].session_id = 0;
	if (server->connected_sessions>0) server->connected_sessions--;
    }
}

int
delete_session_id(int pid, char * killed_user, int len)
{
    int cleaned = 0;
    if (server->max_players > MAX_USER || server->max_players < 0)
	server->max_players = MAX_USER;

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
	    // Increment kick count
	    userrec_t user_rec;
	    if (read_userrec(&user_rec, shdat.client->user[i].name.c, 0) == 0) {
		user_rec.kick_count++;
		user_rec.dirty = 1;
		write_userrec(&user_rec, 0);
	    }
	    close_userdb(); // We will be fork()ing later.

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
