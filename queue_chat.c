

#include "queue_chat.h"

#if INTERFACE
#define CHAT_QUEUE_LEN	256
#define CHAT_MSG_LEN	64

typedef chat_queue_t chat_queue_t;
struct chat_queue_t {
    uint32_t generation;	// uint so GCC doesn't fuck it up.
    uint32_t curr_offset;
    uint32_t queue_len;

    chat_entry_t updates[CHAT_QUEUE_LEN];
};

typedef chat_entry_t chat_entry_t;
struct chat_entry_t {
    int is_global;
    int to_level_id;
    int to_player_id;
    int to_team_id;
    int not_player_id;
    int from_player_id;
    int message_len;
    int message_type;
    char message[CHAT_MSG_LEN];
}
#endif

static int last_id = -1;
static uint32_t last_generation;

static char * queue_tmp_buf = 0;
static int queue_tmp_buf_len = 0, queue_tmp_buf_used = 0;

/* Send a single chat message */
/* to_where: 0) all, 1) Player(to_id), 2) Level(to_id), 3) Team(to_id) */
/* to_id: filter id or for "all" 0=>system, 1=>chat. */
void
queue_chat(int to_where, int to_id, int chat_type, char * chat_msg, int chat_len)
{
    if (!shared_chat_queue || shared_chat_queue->curr_offset >= shared_chat_queue->queue_len)
	create_chat_queue();

    lock_fn(chat_queue_lock);
    do {
	int id = shared_chat_queue->curr_offset;
	shared_chat_queue->updates[id].is_global = 0; // Is global chat.
	shared_chat_queue->updates[id].to_player_id = -1;
	shared_chat_queue->updates[id].to_level_id = -1;
	shared_chat_queue->updates[id].to_team_id = -1;
	shared_chat_queue->updates[id].not_player_id = -1;
	shared_chat_queue->updates[id].from_player_id = my_user_no;
	switch (to_where) {
	case 0: shared_chat_queue->updates[id].is_global = to_id; break;
	case 1: shared_chat_queue->updates[id].to_player_id = to_id; break;
	case 2: shared_chat_queue->updates[id].to_level_id = to_id; break;
	case 3: shared_chat_queue->updates[id].to_team_id = to_id; break;
	}
	shared_chat_queue->updates[id].message_type = chat_type;
	shared_chat_queue->updates[id].message_len = chat_len;

	int post_len = chat_len;
	if (post_len > CHAT_MSG_LEN) post_len = CHAT_MSG_LEN;
	memcpy(shared_chat_queue->updates[id].message, chat_msg, post_len);
	chat_msg += post_len;
	chat_len -= post_len;

	if (++shared_chat_queue->curr_offset >= shared_chat_queue->queue_len) {
	    shared_chat_queue->curr_offset = 0;
	    shared_chat_queue->generation ++;
	}
    }
    while(chat_len > 0);
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

    int my_level = shdat.client->user[my_user_no].state.on_level;
    int filter_level = 0;

    if (shdat.client && my_user_no >= 0 && my_user_no < MAX_USER)
	my_level = shdat.client->user[my_user_no].state.on_level;
    if (my_level >= MAX_LEVEL)
	my_level = -1;

    if (my_level >= 0 && level_prop && level_prop->level_chat) filter_level = 1;

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
	    if (upd.to_level_id != my_level)
		continue;
	}

	if (filter_level && upd.is_global)
	    continue;

	// To a team ... Not me.
	if (upd.to_team_id >= 0) continue;

	if (upd.from_player_id >= 0 && upd.from_player_id < MAX_USER) {
	    int i = upd.from_player_id;
	    if (shdat.client->user[i].state.active)
		if (in_strlist(my_user.ignore_list, shdat.client->user[i].name.c))
		    continue;
	}

	if (upd.message_len <= CHAT_MSG_LEN && queue_tmp_buf_used == 0) {
	    post_chat(-1, 0, upd.message_type, upd.message, upd.message_len);
	} else {
	    int post_len = upd.message_len;
	    if (post_len > CHAT_MSG_LEN) post_len = CHAT_MSG_LEN;
	    if (queue_tmp_buf_used + post_len >= queue_tmp_buf_len) {
		queue_tmp_buf_len = queue_tmp_buf_used + post_len + CHAT_MSG_LEN;
		queue_tmp_buf = realloc(queue_tmp_buf, queue_tmp_buf_len);
		if (queue_tmp_buf == 0) {
		    queue_tmp_buf_len = queue_tmp_buf_used = 0;
		    continue;
		}
	    }
	    memcpy(queue_tmp_buf+queue_tmp_buf_used, upd.message, post_len);
	    queue_tmp_buf_used += post_len;
	    if (upd.message_len <= CHAT_MSG_LEN) {
		post_chat(-1, 0, upd.message_type, queue_tmp_buf, queue_tmp_buf_used);
		queue_tmp_buf_used = 0;
	    }
	}
    }

    if (flush)
	flush_to_remote();
}
