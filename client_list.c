
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
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
#define MAGIC_USR	0x0012FF7E

typedef struct client_entry_t client_entry_t;
struct client_entry_t {
    nbtstr_t name;
    nbtstr_t client_software;
    int on_level;
    xyzhv_t posn;
    uint8_t active;
    uint8_t visible;
    time_t afk_time;	//TODO
    pid_t session_id;
};

struct client_level_t {
    int loaded;
    nbtstr_t level;
};

typedef struct client_data_t client_data_t;
struct client_data_t {
    int magic1;
    uint32_t generation;
    client_entry_t user[MAX_USER];
    client_level_t levels[MAX_LEVEL];
    int magic2;
};
#endif

int my_user_no = -1;
static client_entry_t myuser[MAX_USER];
static struct timeval last_check;

xyzhv_t player_posn = {0};

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

    int32_t msnow = now.tv_sec * 1000 + now.tv_usec / 1000;
    int32_t mslast = last_check.tv_sec * 1000 + last_check.tv_usec / 1000;
    if (msnow-(mslast+100)<=0)
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
	c.visible = (c.active && c.on_level == my_level);

	if (c.visible && !myuser[i].visible) {
	    // New user.
	    send_spawn_pkt(i, c.name.c, c.posn);
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
}

void
reset_player_list()
{
    for(int i=0; i<MAX_USER; i++)
	myuser[i].active = 0;

    send_spawn_pkt(255, user_id, level_prop->spawn);
    if (myuser[my_user_no].posn.valid)
	send_posn_pkt(255, 0, myuser[my_user_no].posn);
}

void
update_player_pos(pkt_player_posn pkt)
{
    player_posn = pkt.pos;
    if (my_user_no < 0 || my_user_no >= MAX_USER) return;
    myuser[my_user_no].posn = pkt.pos;
    if (!shdat.client) return;
    shdat.client->user[my_user_no].posn = pkt.pos;
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

    lock_client_data();
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
	unlock_client_data();
	if (kicked)
	    fatal("Too many sessions, please try again");
	else
	    fatal("Too many sessions already connected");
    }

    my_user_no = new_one;
    nbtstr_t t = {0};
    strcpy(t.c, user_id);
    shdat.client->generation++;
    shdat.client->user[my_user_no].active = 1;
    shdat.client->user[my_user_no].session_id = getpid();
    shdat.client->user[my_user_no].name = t;
    shdat.client->user[my_user_no].client_software = client_software;
    shdat.client->user[my_user_no].on_level = -1;

    unlock_client_data();
}

void
start_level(char * levelname, char * levelfile)
{
    nbtstr_t level = {0};
    int level_id = -1;

    strcpy(level.c, levelname);
    strcpy(current_level_name, levelname);
    strcpy(current_level_fname, levelfile);

    lock_client_data();

    for(int i=0; i<MAX_LEVEL; i++) {
	if (!shdat.client->levels[i].loaded) {
	    if (level_id == -1) level_id = i;
	    continue;
	}
	nbtstr_t n = shdat.client->levels[i].level;
	if (strcmp(n.c, level.c) == 0) {
	    level_id = i;
	    break;
	}
    }

    if (level_id<0)
	fatal("Too many levels loaded");

    if (!shdat.client->levels[level_id].loaded) {
	shdat.client->levels[level_id].level = level;
	shdat.client->levels[level_id].loaded = 1;
    }

    shdat.client->user[my_user_no].on_level = level_id;

    unlock_client_data();
}

void
stop_user()
{
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
	    cleaned ++;
	    if (killed_user)
		snprintf(killed_user, len, "%s", shdat.client->user[i].name.c);
	    fprintf(stderr, "Wiped session %d (%.32s)\n", i, shdat.client->user[i].name.c);
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
