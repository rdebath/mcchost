
#include <unistd.h>
#include <stdio.h>

#include "queue_chat.h"

#if INTERFACE
#include <stdint.h>

typedef chat_queue_t chat_queue_t;
struct chat_queue_t {
    uint32_t generation;	// uint so GCC doesn't fuck it up.
    uint32_t curr_offset;
    uint32_t queue_len;

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

static int last_id = -1;
static uint32_t last_generation;

/* Send a single chat message to everyone (0) */
void
update_chat(pkt_message *pkt)
{
    if (!level_chat_queue || level_chat_queue->curr_offset >= level_chat_queue->queue_len)
	create_chat_queue();

    lock_fn(chat_queue_lock);
    int id = level_chat_queue->curr_offset;
    level_chat_queue->updates[id].to_level_id = 0;
    level_chat_queue->updates[id].to_player_id = 0;
    level_chat_queue->updates[id].to_team_id = 0;
    level_chat_queue->updates[id].msg = *pkt;
    if (++level_chat_queue->curr_offset >= level_chat_queue->queue_len) {
	level_chat_queue->curr_offset = 0;
	level_chat_queue->generation ++;
    }
    unlock_fn(chat_queue_lock);
}

void
set_last_chat_queue_id()
{
    if (!level_chat_queue) create_chat_queue();

    lock_fn(chat_queue_lock);
    last_id = level_chat_queue->curr_offset;
    last_generation = level_chat_queue->generation;
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
	if (last_generation != level_chat_queue->generation)
	{
	    int isok = 0;
	    if (last_generation == level_chat_queue->generation-1) {
		if (level_chat_queue->curr_offset < last_id)
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
	if (last_id == level_chat_queue->curr_offset) {
	    // Nothing more to send.
	    unlock_fn(chat_queue_lock);
	    break;
	}
	upd = level_chat_queue->updates[last_id++];
	if (last_id == level_chat_queue->queue_len) {
	    last_generation ++;
	    last_id = 0;
	}
	unlock_fn(chat_queue_lock);

	send_msg_pkt_filtered(upd.msg.message_type, upd.msg.message);
    }

    if (flush)
	flush_to_remote();
}

/* Sends a message packet, filtering the CP437 to ASCII if required */
void
send_msg_pkt_filtered(int msg_flag, char * message)
{
    if (extn_fullcp437)
	send_message_pkt(0, msg_flag, message);
    else
    {
	char msgbuf[NB_SLEN];
	char *s=message, *d = msgbuf;
	for(int i = 0; i<MB_STRLEN; i++, s++) {
	    if (*s >= ' ' && *s <= '~')
		*d++ = *s;
	    else if (*s & 0x80)
		*d++ = cp437_ascii[*s & 0x7f];
	    else
		*d++ = '*';
	}
	*d++ = 0;
	send_message_pkt(0, msg_flag, msgbuf);
    }
}
