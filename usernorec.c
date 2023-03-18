#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#include "usernorec.h"

/*
 * User details -- fallback.
 */

#if !defined(HAS_LMDB)
void
write_userrec(userrec_t * userrec, int UNUSED(ini_too))
{
    uint8_t user_key[NB_SLEN*4];
    copy_user_key(user_key, userrec->user_id);

    if (!userrec->ini_dirty)
	read_ini_file_fields(userrec);

    char userini[PATH_MAX];
    saprintf(userini, USER_INI_NAME, user_key);

    user_ini_tgt = userrec;
    save_ini_file(user_ini_fields, userini);
    user_ini_tgt = 0;
    userrec->ini_dirty = 0;
}

// If user_id arg is zero we use the user_no field so we can load the
// DB data (specifically the full name) for translation of id to name.
int
read_userrec(userrec_t * rec_buf, char * user_id, int UNUSED(load_ini))
{
    if (user_id && *user_id == 0) return -1;
    if (user_id == 0 && rec_buf->user_no == 0) return -1;

    if (user_id == 0) {
	printf_chat("&WUser database missing, try full user name");
	return -1;
    }

    uint8_t user_key[NB_SLEN*4];
    copy_user_key(user_key, user_id);

    char userini[PATH_MAX];
    rec_buf->click_distance = -1;  // Map default
    user_ini_tgt = rec_buf;
    saprintf(userini, USER_INI_NAME, user_key);
    int rv = load_ini_file(user_ini_fields, userini, 1, 0);
    user_ini_tgt = 0;
    return rv;
}

int
match_user_name(char * UNUSED(partname), char * UNUSED(namebuf), int UNUSED(l), int quiet, int * UNUSED(skip_count))
{
    if (!quiet) printf_chat("&WUser database missing, try full user name");
    return -1;
}

void
close_userdb()
{
    // No database to close
}
#endif
