
#include "queue_cmd.h"

#if INTERFACE
#define CMD_QUEUE_LEN	256
enum cmd_token_e {
    cmd_token_kick = 1, cmd_token_ban,
    cmd_token_changemodel, cmd_token_changeskin};

typedef cmd_payload_t cmd_payload_t;
struct cmd_payload_t {
    enum cmd_token_e cmd_token;
    nbtstr_t arg;
};

typedef cmd_queue_t cmd_queue_t;
struct cmd_queue_t {
    uint32_t generation;	// uint so GCC doesn't fuck it up.
    uint32_t curr_offset;
    uint32_t queue_len;

    cmd_entry_t updates[CMD_QUEUE_LEN];
};

typedef cmd_entry_t cmd_entry_t;
struct cmd_entry_t {
    int to_level_id;
    int to_player_id;
    int to_team_id;
    int not_player_id;
    cmd_payload_t msg[1];
}
#endif

static int last_id = -1;
static uint32_t last_generation;

/* Send a single cmd message */
void
send_ipc_cmd(int to_where, int to_id, cmd_payload_t *pkt)
{
    if (!shared_cmd_queue || shared_cmd_queue->curr_offset >= shared_cmd_queue->queue_len)
	create_cmd_queue();

    lock_fn(cmd_queue_lock);
    int id = shared_cmd_queue->curr_offset;
    shared_cmd_queue->updates[id].to_player_id = -1;
    shared_cmd_queue->updates[id].to_level_id = -1;
    shared_cmd_queue->updates[id].to_team_id = -1;
    shared_cmd_queue->updates[id].not_player_id = -1;
    switch (to_where) {
    case 1: shared_cmd_queue->updates[id].to_player_id = to_id; break;
    case 2: shared_cmd_queue->updates[id].to_level_id = to_id; break;
    case 3: shared_cmd_queue->updates[id].to_team_id = to_id; break;
    }
    shared_cmd_queue->updates[id].msg[0] = *pkt;
    if (++shared_cmd_queue->curr_offset >= shared_cmd_queue->queue_len) {
	shared_cmd_queue->curr_offset = 0;
	shared_cmd_queue->generation ++;
    }
    unlock_fn(cmd_queue_lock);
}

void
set_last_cmd_queue_id()
{
    if (!shared_cmd_queue) create_cmd_queue();

    lock_fn(cmd_queue_lock);
    last_id = shared_cmd_queue->curr_offset;
    last_generation = shared_cmd_queue->generation;
    unlock_fn(cmd_queue_lock);
}

void
wipe_last_cmd_queue_id()
{
    last_id = -1;
    last_generation = 0;
}

void
process_queued_cmds()
{
    if (last_id < 0) return; // Hmmm.

    for(;;)
    {
	cmd_entry_t upd;
	lock_fn(cmd_queue_lock);
	if (last_generation != shared_cmd_queue->generation)
	{
	    int isok = 0;
	    if (last_generation == shared_cmd_queue->generation-1) {
		if (shared_cmd_queue->curr_offset < last_id)
		    isok = 1;
	    }
	    if (!isok) {
		// Buffer wrapped too far; generation incremented twice.
		// Skip everything and try again next tick.
		unlock_fn(cmd_queue_lock);
		set_last_cmd_queue_id();
		break;
	    }
	}
	if (last_id == shared_cmd_queue->curr_offset) {
	    // Nothing more to send.
	    unlock_fn(cmd_queue_lock);
	    break;
	}
	upd = shared_cmd_queue->updates[last_id++];
	if (last_id == shared_cmd_queue->queue_len) {
	    last_generation ++;
	    last_id = 0;
	}
	unlock_fn(cmd_queue_lock);

	// Already seen it
	if (upd.not_player_id >= 0 && upd.not_player_id == my_user_no)
	    continue;

	// To someone and not me
	if (upd.to_player_id >= 0 && upd.to_player_id != my_user_no)
	    continue;

	// To somewhere and not here
	if (upd.to_level_id >= 0) {
	    int my_level = shdat.client->user[my_user_no].on_level;
	    if (upd.to_level_id != my_level)
		continue;
	}

	// To a team ... Not me.
	if (upd.to_team_id >= 0) continue;

	process_ipc_cmd(upd.msg);
    }
}

void
process_ipc_cmd(cmd_payload_t *msg)
{
    switch(msg->cmd_token) {
    case cmd_token_kick:
	kicked(msg->arg.c);
	break;
    case cmd_token_ban:
	banned(msg->arg.c);
	break;
    case cmd_token_changemodel:
#ifdef CMD_MODEL
	do_cmd_model(msg->arg.c);
#endif
	break;
    case cmd_token_changeskin:
#ifdef CMD_SKIN
	do_cmd_skin(msg->arg.c);
#endif
	break;
    }
}
