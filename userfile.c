#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#include "userfile.h"

/*
 * User details -- file.
 */

#if INTERFACE
typedef struct userrec_t userrec_t;
struct userrec_t
{
    int32_t user_no;
    char user_id[NB_SLEN];
    int64_t blocks_placed;
    int64_t blocks_deleted;
    int64_t blocks_drawn;
    int64_t first_logon;	// is time_t, time_t is sometimes 32bits.
    int64_t last_logon;		// is time_t, time_t is sometimes 32bits.
    int64_t logon_count;
    int64_t kick_count;
    int64_t death_count;
    int64_t message_count;
    char last_ip[NB_SLEN];
    int64_t time_online_secs;

    // Saved to ini file
    int64_t coin_count;
    int32_t click_distance;
    char nick[NB_SLEN];
    char title[NB_SLEN];
    char skin[NB_SLEN];
    char model[NB_SLEN];
    char colour[NB_SLEN];
    char title_colour[NB_SLEN];
    char timezone[NB_SLEN];
    int32_t user_group;
    uint8_t banned;
    char ban_message[NB_SLEN];

    char * ignore_list;

    // Not saved
    int saveable;
    int dirty;
    int ini_dirty;
    int user_logged_in;
    int64_t time_of_last_save;
}
#endif

userrec_t my_user = {.user_no = 0, .user_group=1, .saveable=1};

/*
    (when == 0) => Tick
    (when == 1) => At logon
    (when == 2) => At logoff
    (when == 3) => At config change
 */
void
write_current_user(int when)
{
    time_t now = time(0);
    if (when == 0 && now - my_user.time_of_last_save >= 300)
	my_user.dirty = 1;

    if (when == 0 && !my_user.dirty) return;
    if (when >= 2 && !my_user.user_logged_in) return;

    if (my_user.user_no == 0) {
	if (*user_id == 0) return;
	if (read_userrec(&my_user, user_id, 1) < 0)
	    my_user.user_no = 0;
    }

    if (when == 1) {
	login_time = now;
	if (my_user.logon_count == 0) my_user.first_logon = time(0);
	my_user.last_logon = now;
	my_user.time_of_last_save = now;
	my_user.logon_count++;
	my_user.user_logged_in = 1;
	my_user.ini_dirty = 1;

	if (*client_ipv4_str) {
	    snprintf(my_user.last_ip, sizeof(my_user.last_ip), "%s", client_ipv4_str);
	    char *p = strrchr(my_user.last_ip, ':');
	    if (p) *p = 0;
	}
    }

    if (now > my_user.time_of_last_save && my_user.user_logged_in) {
	if (my_user.time_of_last_save == 0) my_user.time_of_last_save = now;
	int64_t d = now - my_user.time_of_last_save;
	my_user.time_online_secs += d;
	my_user.time_of_last_save += d;
    }

    strcpy(my_user.user_id, user_id);

    if (when == 3) my_user.ini_dirty = 1;
    write_userrec(&my_user, (when>=1));

    my_user.dirty = 0;
}

void
copy_user_key(char *p, char * user_id)
{
    for(char * s = user_id; *s; s++)
    {
	char *hex = "0123456789ABCDEF";
	int ch = *s & 0xFF;
	if ((ch >= '0' && ch <= '9') || ch == '_' || ch == '.' || (ch >= 'a' && ch <= 'z'))
	    *p++ = ch;
	else if (ch >= 'A' && ch <= 'Z')
	    *p++ = ch - 'Z' + 'z';
	else {
	    *p++ = '%';
	    *p++ = hex[(ch>>4) & 0xF];
	    *p++ = hex[ch & 0xF];
	}
    }
    *p = 0;
}

void decode_user_key(char *hexed_key, char * user_id, int l)
{
    char * p = user_id;
    for(char * s = hexed_key; *s && p-user_id+1<l; s++)
    {
	char *hex = "0123456789ABCDEF";
	int ch = *s & 0xFF;
	if (ch != '%') {
	    *p++ = ch; continue;
	}
	char *h1, *h2;
	if (s[0] && s[1] && (h1=strchr(hex,s[0])) != 0 && (h2=strchr(hex,s[1])) != 0) {
	    s+=2;
	    ch = ((h1-p) << 4);
	    ch += (h2-p);
	    *p++ = ch;
	} else
	    *p++ = ch;
    }
    *p = 0;
}

void
read_ini_file_fields(userrec_t * userrec)
{
    uint8_t user_key[NB_SLEN*4];
    copy_user_key(user_key, userrec->user_id);

    char userini[PATH_MAX];
    saprintf(userini, USER_INI_NAME, user_key);

    userrec_t tmp = *userrec;
    user_ini_tgt = userrec;
    load_ini_file(user_ini_fields, userini, 1, 1);
    user_ini_tgt = 0;

    // Preserve the fields that are in the lmdb database (and user_id).
    userrec->user_no = tmp.user_no;
    memcpy(userrec->user_id, tmp.user_id, NB_SLEN);
    userrec->blocks_placed = tmp.blocks_placed;
    userrec->blocks_deleted = tmp.blocks_deleted;
    userrec->blocks_drawn = tmp.blocks_drawn;
    // userrec->first_logon = tmp.first_logon;
    // userrec->last_logon = tmp.last_logon;
    // userrec->logon_count = tmp.logon_count;
    userrec->kick_count = tmp.kick_count;
    userrec->death_count = tmp.death_count;
    userrec->message_count = tmp.message_count;
    // memcpy(userrec->last_ip, tmp.last_ip, NB_SLEN);
    userrec->time_online_secs = tmp.time_online_secs;
    userrec->ini_dirty = 0;
}
