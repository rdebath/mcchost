/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
void send_message_pkt(int id,char *message);
void send_queued_chats();
void wipe_last_chat_queue_id();
void set_last_chat_queue_id();
#if !defined(_REENTRANT)
#define USE_FCNTL
#endif
#if !defined(USE_FCNTL)
#include <semaphore.h>
#define unlock_chat_shared()
#endif
#if defined(USE_FCNTL)
void unlock_chat_shared(void);
#endif
#if !defined(USE_FCNTL)
#define lock_chat_shared()
#endif
#if defined(USE_FCNTL)
void lock_chat_shared(void);
#endif
void create_chat_queue();
typedef struct pkt_message pkt_message;
#define MB_STRLEN 64
#define NB_SLEN	(MB_STRLEN+1)
struct pkt_message {
    int msg_flag;
    char message[NB_SLEN];
};
void update_chat(pkt_message pkt);
extern intptr_t level_chat_queue_len;
typedef struct chat_queue_t chat_queue_t;
typedef struct chat_entry_t chat_entry_t;
struct chat_entry_t {
    int to_level_id;
    int to_player_id;
    int to_team_id;
    pkt_message msg;
};
struct chat_queue_t {
    uint32_t generation;	// uint so GCC doesn't fuck it up.
    int curr_offset;
    int queue_len;

    chat_entry_t updates[1];
};
extern volatile chat_queue_t *level_chat_queue;
#define INTERFACE 0
