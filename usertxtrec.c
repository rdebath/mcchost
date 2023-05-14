#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#include "usertxtrec.h"

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

    if (userrec->user_no == 0)
	userrec->user_no = scan_user_dir() + 1;

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
match_user_name(char * partname, char * namebuf, int nlen, int quiet, int * skip_count)
{
    if (!partname || !*partname || strlen(partname) > nlen) {
        if (!quiet) printf_chat("The user pattern given is invalid.");
        return -1;
    }

    int found = 0, skipped = 0;
    DIR *directory = 0;
    struct dirent *entry;
    directory = opendir(USER_DIR);
    if (directory) {
	while( (entry=readdir(directory)) )
	{
	    char *p, file_name[NB_SLEN*4];
	    if (strlen(entry->d_name) > sizeof(file_name)-4) continue;
	    strcpy(file_name, entry->d_name);
	    if ((p=strrchr(file_name, '.')) == 0) continue;
	    if (strcmp(p, ".ini") != 0) continue;
	    *p = 0;
	    char user_name[NB_SLEN];
	    decode_user_key(file_name, user_name, sizeof(user_name));
	    if (strcasecmp(user_name, partname) == 0) {
		// "Exact" match.
		snprintf(namebuf, nlen, "%s", user_name);
		found = 1;
		break;
	    } else if (my_strcasestr(user_name, partname)) {
		if (found == 0)
		    snprintf(namebuf, nlen, "%s", user_name);
		else if (strlen(namebuf) + strlen(user_name) + 3 < nlen) {
		    strcat(namebuf, ", ");
		    strcat(namebuf, user_name);
		} else
		    skipped++;
		found++;
	    }
	}
	closedir(directory);

    } else {
	printf_chat("&WSearch of user directory failed");
	return -1;
    }

    if (!quiet) {
	if (found>1) {
            if (skipped)
                printf_chat("The id \"%s\" matches %d users including %s", partname, found, namebuf);
            else
                printf_chat("The id \"%s\" matches %d users; %s", partname, found, namebuf);
	} else if (found != 1)
	    printf_chat("User \"%s\" not found.", partname);
    }

    if (skip_count) *skip_count = skipped;
    return found;
}

void
close_userdb()
{
    // No database to close
}

// Find the highest user_no in all the user files.
// TODO: Save an number->name translation array?
LOCAL int
scan_user_dir()
{
    struct dirent *entry;
    DIR *directory = opendir(USER_DIR);
    if (!directory) return 0;

    int maxno = 0;

    while( (entry=readdir(directory)) )
    {
	int l = strlen(entry->d_name);
	if (l<=4 || strcmp(entry->d_name+l-4, ".ini") != 0) continue;
	entry->d_name[l-4] = 0;

	char nbuf[MAXLEVELNAMELEN*4];
	saprintf(nbuf, USER_INI_NAME, entry->d_name);

	userrec_t rec_buf[1] = {0};
	user_ini_tgt = rec_buf;
	load_ini_file(user_ini_fields, nbuf, 1, 0);
	user_ini_tgt = 0;
	if (rec_buf->user_no == 0) continue;
	if (rec_buf->user_id[0] == 0) continue;

	if (rec_buf->user_no > maxno) maxno = rec_buf->user_no;
    }
    closedir(directory);
    return maxno;
}
#endif
