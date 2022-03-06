
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>

#include "queue_chat.h"

#if INTERFACE
typedef chat_queue_t chat_queue_t;
struct chat_queue_t {
    uint32_t generation;	// uint so GCC doesn't fuck it up.
    int curr_offset;
    int queue_len;

    chat_entry_t updates[1];
};

typedef chat_entry_t chat_entry_t;
struct chat_entry_t {
    int to_level_id;
    int to_player_id;
    int to_team_id;
    pkt_message msg;
}
#endif

volatile chat_queue_t *level_chat_queue = 0;
intptr_t level_chat_queue_len = 0;

static int last_id = -1;
static uint32_t last_generation;

void
update_chat(pkt_message pkt)
{
    if (!level_chat_queue) create_chat_queue();

    lock_shared();
    int id = level_chat_queue->curr_offset++;
    if (level_chat_queue->curr_offset >= level_chat_queue->queue_len) {
	level_chat_queue->curr_offset = 0;
	level_chat_queue->generation ++;
    }
    level_chat_queue->updates[id].to_level_id = 0;
    level_chat_queue->updates[id].to_player_id = 0;
    level_chat_queue->updates[id].to_team_id = 0;
    level_chat_queue->updates[id].msg = pkt;
    unlock_shared();
}

void
set_last_chat_queue_id()
{
    if (!level_chat_queue) create_chat_queue();

    lock_shared();
    last_id = level_chat_queue->curr_offset;
    last_generation = level_chat_queue->generation;
    unlock_shared();
}

void
wipe_last_chat_queue_id()
{
    last_id = -1;
    last_generation = 0;
}

void
send_queued_chats()
{
    int counter = 0;
    if (last_id < 0) return; // Hmmm.

    for(;counter < 128; counter++)
    {
	chat_entry_t upd;
	lock_shared();
	if (last_generation != level_chat_queue->generation)
	{
	    int isok = 0;
	    if (last_generation == level_chat_queue->generation-1) {
		if (level_chat_queue->curr_offset < last_id)
		    isok = 1;
	    }
	    if (!isok) {
		unlock_shared();
		set_last_chat_queue_id(); // Skip to end.
		return;
	    }
	}
	if (last_id == level_chat_queue->curr_offset) {
	    // Nothing more to send.
	    unlock_shared();
	    return;
	}
	upd = level_chat_queue->updates[last_id++];
	if (last_id == level_chat_queue->queue_len) {
	    last_generation ++;
	    last_id = 0;
	}
	unlock_shared();

	send_message_pkt(upd.msg.msg_flag, upd.msg.message);
    }
}
