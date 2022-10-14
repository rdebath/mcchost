

#include "queue_chat.h"

#if INTERFACE
#define CHAT_QUEUE_LEN	256

typedef chat_queue_t chat_queue_t;
struct chat_queue_t {
    uint32_t generation;	// uint so GCC doesn't fuck it up.
    uint32_t curr_offset;
    uint32_t queue_len;

    chat_entry_t updates[CHAT_QUEUE_LEN];
};

typedef chat_entry_t chat_entry_t;
struct chat_entry_t {
    int to_level_id;
    int to_player_id;
    int to_team_id;
    int not_player_id;
    pkt_message msg;
}
#endif

static int last_id = -1;
static uint32_t last_generation;

/* Send a single chat message */
void
update_chat(int to_where, int to_id, pkt_message *pkt)
{
    if (!shared_chat_queue || shared_chat_queue->curr_offset >= shared_chat_queue->queue_len)
	create_chat_queue();

    lock_fn(chat_queue_lock);
    int id = shared_chat_queue->curr_offset;
    shared_chat_queue->updates[id].to_player_id = -1;
    shared_chat_queue->updates[id].to_level_id = -1;
    shared_chat_queue->updates[id].to_team_id = -1;
    shared_chat_queue->updates[id].not_player_id = -1;
    switch (to_where) {
    case 1: shared_chat_queue->updates[id].to_player_id = to_id; break;
    case 2: shared_chat_queue->updates[id].to_level_id = to_id; break;
    case 3: shared_chat_queue->updates[id].to_team_id = to_id; break;
    }
    shared_chat_queue->updates[id].msg = *pkt;
    if (++shared_chat_queue->curr_offset >= shared_chat_queue->queue_len) {
	shared_chat_queue->curr_offset = 0;
	shared_chat_queue->generation ++;
    }
    unlock_fn(chat_queue_lock);
}

void
set_last_chat_queue_id()
{
    if (!shared_chat_queue) create_chat_queue();

    lock_fn(chat_queue_lock);
    last_id = shared_chat_queue->curr_offset;
    last_generation = shared_chat_queue->generation;
    unlock_fn(chat_queue_lock);
}

void
wipe_last_chat_queue_id()
{
    last_id = -1;
    last_generation = 0;
}

void
send_queued_chats(int flush)
{
    if (last_id < 0) return; // Hmmm.

    for(;;)
    {
	chat_entry_t upd;
	lock_fn(chat_queue_lock);
	if (last_generation != shared_chat_queue->generation)
	{
	    int isok = 0;
	    if (last_generation == shared_chat_queue->generation-1) {
		if (shared_chat_queue->curr_offset < last_id)
		    isok = 1;
	    }
	    if (!isok) {
		// Buffer wrapped too far; generation incremented twice.
		// Skip everything and try again next tick.
		unlock_fn(chat_queue_lock);
		set_last_chat_queue_id();
		break;
	    }
	}
	if (last_id == shared_chat_queue->curr_offset) {
	    // Nothing more to send.
	    unlock_fn(chat_queue_lock);
	    break;
	}
	upd = shared_chat_queue->updates[last_id++];
	if (last_id == shared_chat_queue->queue_len) {
	    last_generation ++;
	    last_id = 0;
	}
	unlock_fn(chat_queue_lock);

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

	send_message_pkt(0, upd.msg.message_type, upd.msg.message);
    }

    if (flush)
	flush_to_remote();
}
