
#include <signal.h>
#include <assert.h>

#include "client_list.h"

/* Note: I've made the choice to have one table for all users, not one
 * per level. That sets the maximum allowed number of users to 128/256.
 *
 * The first 128 will be visible on a level.
 * The first 256 will be shown in the player list
 * If the client looks like ClassiCube visible players are first 256
 *
 * For a reasonable display if this increased, the list sent to the client
 * will have to be renumbered.
 */

#if INTERFACE
#include <sys/types.h>

#define MAX_USER	1024
#define MAX_LEVEL	256

#define TY_MAGIC2    0x557FFF00
#define TY_MAGIC3    ((int)(sizeof(client_data_t)+sizeof(server_t)))
#define TY_CVERSION  0x00000200

typedef struct client_entry_t client_entry_t;
struct client_entry_t {
    nbtstr_t name;
    nbtstr_t client_software;
    nbtstr_t listname;
    nbtstr_t skinname;
    nbtstr_t modelname;
    client_state_entry_t state;
    int level_bkp_id;
    int summon_level_id;
    xyzhv_t summon_posn;
    time_t last_move;
    pid_t session_id;
    uint32_t ip_address;
    uint8_t ip_dup;
    char name_colour;
    uint8_t client_proto_ver;
    uint8_t client_cpe;
    uint8_t client_dup;
    uint8_t authenticated;
    uint8_t trusted;
    uint8_t packet_idle;
};

typedef struct client_level_t client_level_t;
struct client_level_t {
    pid_t loader_pid;
    int backup_id;
    nbtstr_t level;
    uint8_t loaded;
    uint8_t in_use;
    uint8_t force_unload;
    uint8_t force_backup;
    uint8_t delete_on_unload;
    uint8_t no_unload;
};

typedef struct client_data_t client_data_t;
struct client_data_t {
    int64_t magic_no;
    int magic_sz;
    uint32_t generation;
    uint32_t cleanup_generation;
    int highest_used_uid;
    client_entry_t user[MAX_USER];
    client_level_t levels[MAX_LEVEL];
    int magic2;
    int version;
};

typedef struct client_state_entry_t client_state_entry_t;
struct client_state_entry_t {
    int on_level;
    xyzhv_t posn;
    uint8_t active;
    uint8_t visible;
    uint8_t is_afk;
    uint8_t look_update_counter;
    uint8_t model_set;
};
#endif

int my_user_no = -1;
static client_state_entry_t myuser[MAX_USER];
static client_state_entry_t myuser_stat;
static struct timeval last_check;

static int uid_ren_mode = -1;
struct uid_ren_t { int uids; uint64_t range; };
static struct uid_ren_t uid_ren[MAX_USER];
static int was_uid_ren[MAX_USER];

xyzhv_t player_posn = {0};
block_t player_held_block = 1;
time_t player_last_move;
int player_on_new_level = 1;
int player_is_afk = 0;
aspam_t player_block_spam[1];
aspam_t player_cmd_spam[1];
aspam_t player_chat_spam[1];
int player_lockout = 5;

nbtstr_t player_title_name;	// Name including colour and title
nbtstr_t player_list_name;	// Name with &Colour
nbtstr_t player_group_name;

void
check_other_users()
{
    if (my_user_no < 0 || my_user_no >= MAX_USER || !shdat.client) return;

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

    int my_level = shdat.client->user[my_user_no].state.on_level;
    if (my_level == -1) my_level = -2; //NOPE

    reorder_visible_users(my_level);

    // I have a small trick to allow exactly 256 users without invoking the
    // reordering. But this only works if there is a hole in the IDs sent
    // to the client for my user id number.
    int loop_max = 256;
    if (uid_ren_mode) loop_max = 255;

    for(int i=0; i<loop_max; i++)
    {
	int uids = uid_ren[i].uids;
	if (uids == my_user_no) continue; // Me

	// Note: slurp it so a torn struct is unlikely.
	// But don't lock as we're not too worried.
	client_entry_t c = shdat.client->user[uids];
	int is_dirty = 0;

	// Is this user visible.
	c.state.visible = (c.state.active && c.authenticated && c.state.posn.valid &&
		c.state.on_level == my_level && c.level_bkp_id >= 0);

	if (extn_extplayerlist) {
	    if (!c.state.active && myuser[i].active) {
		send_removeplayername_pkt(i);
		myuser[i].active = 0;
		is_dirty = 1;
	    } else if (c.state.active &&
		    (!myuser[i].active ||
		    myuser[i].look_update_counter != c.state.look_update_counter ||
		    myuser[i].on_level != c.state.on_level ||
		    myuser[i].is_afk != c.state.is_afk ||
		    was_uid_ren[i] != uids) ) {

		char groupname[256] = "Nowhere";
		if (c.state.on_level >= 0 && c.state.on_level < MAX_LEVEL) {
		    int l = c.state.on_level;
		    nbtstr_t n = shdat.client->levels[l].level;
		    if (shdat.client->levels[l].loaded) {
			if (shdat.client->levels[l].backup_id == 0)
			    saprintf(groupname, "On %s", n.c);
			else if (shdat.client->levels[l].backup_id > 0)
			    saprintf(groupname, "Museum %d %s", shdat.client->levels[l].backup_id, n.c);
		    }
		}

		char listname[256] = "";
		saprintf(listname, "%s%s", c.listname.c, c.state.is_afk?" &7(AFK)":"");

		send_addplayername_pkt(i, c.name.c, listname, groupname, 0);

		is_dirty = 1;
	    }
	}

	int upd_flg = (c.state.look_update_counter != myuser[i].look_update_counter);

	if (c.state.visible && !myuser[i].visible)
	    upd_flg = 1;
	if (was_uid_ren[i] != uids)
	    upd_flg = 1;
	was_uid_ren[i] = uids;

	if (upd_flg || (!c.state.visible && myuser[i].visible)) {
	    // User gone. (Or we need to reset it)
	    send_despawn_pkt(i);
	    is_dirty = 1;
	}
	if ((upd_flg && c.state.visible) || (c.state.visible && !myuser[i].visible)) {
	    // New user.
	    char * skin = c.name.c;
	    if (c.skinname.c[0]) skin = c.skinname.c;
	    char namebuf[256];
	    if (c.name_colour) saprintf(namebuf, "&%c%s", c.name_colour, c.name.c);
	    else saprintf(namebuf, "&7%s", c.name.c);
	    revert_amp_to_perc(namebuf);
	    send_addentity_pkt(i, namebuf, skin, c.state.posn);
	    send_posn_pkt(i, 0, c.state.posn);
	    is_dirty = 1;
	} else if (c.state.visible) {
	    // Update user
	    send_posn_pkt(i, &myuser[i].posn, c.state.posn);
	    is_dirty = 1;
	}
	if (upd_flg) {
	    c.state.model_set = (c.modelname.c[0] != 0);
	    if (c.state.model_set || myuser[i].model_set) {
		send_changemodel_pkt(i, c.modelname.c);
		is_dirty = 1;
	    }
	}
	if (is_dirty)
	    myuser[i] = c.state;
    }
}

void
reorder_visible_users(int my_level)
{
    // Do we need to switch modes?
    int f = (shdat.client->highest_used_uid > max_proto_player_id);
    if (f != uid_ren_mode) {
	if (uid_ren_mode != -1 || f) {
	    fprintf_logfile("User %s Switching UID mode TopUID=%d uid_ren_mode=%d from %d",
		user_id, shdat.client->highest_used_uid, f, uid_ren_mode);
	    despawn_all_players();
	}
	uid_ren_mode = f;
	for(int i = 0; i<MAX_USER; i++)
	    uid_ren[i].uids = i;
    }
    if (!uid_ren_mode) return;

    int max_id = max_proto_player_id;
    if (max_proto_player_id == 255) max_proto_player_id = 254;

    uint64_t largest_shown_range = 0;
    uint64_t smallest_hidden_range = UINT64_MAX;
    for(int i=0; i<MAX_USER; i++)
    {
	int uids = uid_ren[i].uids;
	client_entry_t *c = &shdat.client->user[uids];

	int visible = (c->state.active && c->authenticated && c->state.posn.valid &&
		c->state.on_level == my_level && c->level_bkp_id >= 0);

	int64_t range;
	if (!visible || uids == my_user_no)
	     range = UINT64_MAX;
	else {
	    xyzhv_t posn = c->state.posn;

	    int64_t rx = abs(player_posn.x - posn.x);
	    int64_t ry = abs(player_posn.y - posn.y);
	    int64_t rz = abs(player_posn.z - posn.z);
	    range = rx*rx + ry*ry + rz*rz;
	    range /= 100;
	}
	uid_ren[i].range = range;

	if (i > max_id) {
	    if (range < smallest_hidden_range) smallest_hidden_range = range;
	} else {
	    if (range > largest_shown_range) largest_shown_range = range;
	}
    }

    // Use a partial sort, we don't want to change any in the lower
    // part if we don't need to.
    if (smallest_hidden_range < largest_shown_range) {

	// Which do we need to sort.
	int min_off = MAX_USER, max_off = 0;
	for (int i=0; i<MAX_USER; i++)
	{
	    if (uid_ren[i].range >= smallest_hidden_range &&
		uid_ren[i].range <= largest_shown_range &&
		(uid_ren[i].range != UINT64_MAX || i < max_id)) {

		if (i>max_off) max_off = i;
		if (i<min_off) min_off = i;
	    }
	}

	if (min_off < max_off)
	    qsort(uid_ren+min_off, max_off-min_off+1, sizeof(*uid_ren), uid_ren_range_cmp);
    }
}

#define UNWRAP(x,y) (((x) > (y)) - ((x) < (y)))
int
uid_ren_range_cmp(const void *p1, const void *p2)
{

    struct uid_ren_t *ps1 = (struct uid_ren_t *)p1;
    struct uid_ren_t *ps2 = (struct uid_ren_t *)p2;

    return UNWRAP(ps1->range, ps2->range);
}

void
check_user_summon()
{
    if (my_user_no < 0 || my_user_no >= MAX_USER || !shdat.client) return;

    //TODO: Attempt a session upgrade -- call exec().
    if (server->magic != TY_MAGIC
        || shdat.client->magic_no != TY_MAGIC
	|| shdat.client->magic_sz != TY_MAGIC3
	|| shdat.client->magic2 != TY_MAGIC2
	|| shdat.client->version != TY_CVERSION)
	fatal("Session upgrade failure, please reconnect");

    if (!shdat.client->user[my_user_no].state.active) {
	shdat.client->user[my_user_no].session_id = 0;
	logout("(Connecting on new session)");
    }

    if (shdat.client->user[my_user_no].summon_level_id >= 0) {
	xyzhv_t tp = shdat.client->user[my_user_no].summon_posn, t={0};

	// Just on the same level ?
	if (shdat.client->user[my_user_no].summon_level_id == shdat.client->user[my_user_no].state.on_level) {

	    shdat.client->user[my_user_no].summon_level_id = -1;
	    shdat.client->user[my_user_no].summon_posn = t;
            send_posn_pkt(-1, &player_posn, tp);
	    player_posn = tp;

	} else {

	    // Full teleport between levels.
	    nbtstr_t level = {0};
	    int bkpid = 0;

	    int lv = shdat.client->user[my_user_no].summon_level_id;
	    if (shdat.client->levels[lv].loaded) {
		level = shdat.client->levels[lv].level;
		bkpid = shdat.client->levels[lv].backup_id;
	    }
	    shdat.client->user[my_user_no].summon_level_id = -1;
	    shdat.client->user[my_user_no].summon_posn = t;

	    direct_teleport(level.c, bkpid, &tp);
	}
    }
}

void
check_this_user()
{
    if (my_user_no < 0 || my_user_no >= MAX_USER || !shdat.client) return;

    int my_level = shdat.client->user[my_user_no].state.on_level;
    time_t now = time(0);

    if (server->afk_kick_interval > 60)
	if (player_last_move + server->afk_kick_interval < now) {
	    my_user.dirty = 1;
	    my_user.kick_count++;
	    logout("Auto-kick, AFK");
	}

    if (!player_is_afk && server->afk_interval >= 60) {
	if (player_last_move + server->afk_interval < now) {
	    player_is_afk = 1;
	    shdat.client->user[my_user_no].state.is_afk = 1;

	    printf_chat("@&S-%s&S- &Sis AFK auto", player_list_name.c);
	}
    }

    if (my_level >= 0 && my_level < MAX_LEVEL) {
	int go_main = 0;
	if (!shdat.client->levels[my_level].loaded) go_main = 1;
	else if (shdat.client->levels[my_level].force_unload)
	    go_main = shdat.client->levels[my_level].force_unload;

	if (go_main) {
#ifdef UCMD_GOTOMAIN
	    if (go_main == 1 && strcmp(shdat.client->levels[my_level].level.c, main_level()) != 0)
	    {
		printf_chat("You are being moved to main as %s was unloaded",
		    shdat.client->levels[my_level].level.c);
		open_main_level();
	    }
	    else
#endif
	    {
		if (go_main <= 1)
		    printf_chat("You have fallen into the void as %s was unloaded",
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
despawn_all_players()
{
    for(int i=0; i<MAX_USER; i++) {
	if (myuser[i].visible)
	    send_despawn_pkt(i);
	myuser[i].visible = 0;
	myuser[i].posn.valid = 0;
	myuser[i].look_update_counter = 1;
	was_uid_ren[i] = -1;
    }
}

void
reset_player_list()
{
    despawn_all_players();

    reset_player_skinname();
    send_changemodel_pkt(-1, my_user.model);

    if (player_posn.valid)
	send_posn_pkt(-1, 0, player_posn);
    else {
	player_posn = level_prop->spawn;
	player_posn.v = -128; // Bots start with flip-head.
	player_posn.valid = 1;
	myuser_stat.posn = player_posn;
	if (shdat.client)
	    shdat.client->user[my_user_no].state.posn = player_posn;
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

    if (pkt.held_block < BLOCKMAX)
	player_held_block = pkt.held_block;

    // No movement, ignore
    xyzhv_t p1 = pkt.pos;
    xyzhv_t p2 = myuser_stat.posn;
    if (p1.x == p2.x && p1.y == p2.y && p1.z == p2.z &&
        p1.v == p2.v && p1.h == p2.h)
	return;

    myuser_stat.posn = pkt.pos;
    if (shdat.client)
	shdat.client->user[my_user_no].state.posn = pkt.pos;

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
    shdat.client->user[my_user_no].state.is_afk = 0;

    if (player_is_afk) {
	printf_chat("@&a-&7%s&a- &Sis no longer AFK", user_id);
	player_is_afk = 0;
    }
}

void
update_player_packet_idle()
{
    if (my_user_no < 0 || my_user_no >= MAX_USER) return;
    if (!shdat.client) return;
    shdat.client->user[my_user_no].packet_idle = (idle_ticks >= TICK_SECS(10));
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
	t->state.look_update_counter ++;

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
	t->state.look_update_counter ++;
    }

    saprintf(namebuf.c, "%s", my_user.skin);
    if (strcmp(namebuf.c, t->skinname.c) != 0) {
	t->skinname = namebuf;
	t->state.look_update_counter ++;
    }

    saprintf(namebuf.c, "%s", my_user.model);
    if (strcmp(namebuf.c, t->modelname.c) != 0) {
	t->modelname = namebuf;
	t->state.look_update_counter ++;
    }

}

void
start_user()
{
    int new_one = -1, kicked = 0;

    lock_fn(system_lock);
    if (!shdat.client) fatal("Connection failed");

    int connected_sessions = 0;
    int highest_uid = 0;

    for(int i=0; i<MAX_USER; i++)
    {
	if (shdat.client->user[i].state.active != 1) {
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
		shdat.client->user[i].state.active = 0;
		shdat.client->user[i].session_id = 0;
		if (new_one == -1) new_one = i;
		continue;
	    }
	    if (!user_authenticated && !shdat.client->user[i].packet_idle)
		disconnect_f(0, "User %s is already logged in!", user_id);
	    shdat.client->user[i].state.active = 0;
	    kicked++;
	}
	highest_uid = i;
    }

    if (server->max_players < 1) server->max_players = 1;
    if (server->max_players > MAX_USER) server->max_players = MAX_USER;
    if (server->max_players <= connected_sessions && !perm_is_admin())
	new_one = -1;

    if (new_one < 0 || new_one >= MAX_USER) {
	unlock_fn(system_lock);
	if (kicked)
	    disconnect_f(0, "Too many sessions, please try again");
	else
	    disconnect_f(0, "Too many sessions already connected");
    }

    if (new_one > highest_uid) highest_uid = new_one;
    shdat.client->highest_used_uid = highest_uid;

    my_user_no = new_one;
    client_entry_t t = {0};
    strcpy(t.name.c, user_id);
    t.state.active = 1;
    t.state.look_update_counter = 1;
    t.session_id = getpid();
    t.client_software = client_software;
    t.client_proto_ver = protocol_base_version;
    t.client_cpe = cpe_enabled;
    t.state.on_level = -1;
    t.summon_level_id = -1;
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
	    strcpy(t.client_software.c, "Classic");
    }

    shdat.client->user[my_user_no] = t;
    shdat.client->generation++;

    myuser_stat = shdat.client->user[my_user_no].state;
    player_last_move = time(0);
    server->connected_sessions = connected_sessions;
    unlock_fn(system_lock);
}

int
start_level(char * levelname, int backup_id)
{
    nbtstr_t level = {0};
    int level_id = -1;

    strcpy(level.c, levelname);
    strcpy(current_level_name, levelname);
    current_level_backup_id = backup_id;
    player_posn = (xyzhv_t){0};
    player_on_new_level = 1;

    lock_fn(system_lock);

    if (backup_id >= 0) {
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

	if (level_id<0) {
	    // Too many already loaded levels.
	    unlock_fn(system_lock);
	    return 0;
	}

	if (!shdat.client->levels[level_id].loaded) {
	    client_level_t t = {0};
	    t.level = level;
	    t.loaded = 1;
	    t.backup_id = backup_id;
	    shdat.client->levels[level_id] = t;
	    server->loaded_levels++;
	}
    }

    // Move me to new level_no.
    if (my_user_no >= 0 && my_user_no < MAX_USER) {
	shdat.client->user[my_user_no].state.on_level = level_id;
	shdat.client->user[my_user_no].level_bkp_id = backup_id;
	shdat.client->user[my_user_no].state.posn = (xyzhv_t){0};
	// Refresh this.
	shdat.client->levels[level_id].no_unload = 0;
    }

    shdat.client->generation++;
    player_lockout = 5;

    // Change system player list on my client.
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

    return 1;
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

    if (shdat.client->user[my_user_no].state.active) {
	shdat.client->generation++;
	shdat.client->user[my_user_no].state.active = 0;
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

    int flg = (shdat.client == 0);
    if (flg) open_client_list();
    if (!shdat.client) return 0;
    for(int i=0; i<MAX_USER; i++)
    {
	int wipe_this = 0;
	if (pid > 0 && shdat.client->user[i].state.active == 1 &&
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
	    shdat.client->user[i].state.active = 0;
	    shdat.client->user[i].state.on_level = -1;
	    if (server->connected_sessions>0) server->connected_sessions--;
	    if (pid>0) break;
	}
    }
    if (flg) stop_client_list();
    return cleaned;
}
