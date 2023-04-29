#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#ifndef DISABLE_LMDB
#include <lmdb.h>
#include "userrecord.h"

/*
 * User details -- lmdb.
 */

#if INTERFACE
enum csv_type {FLD_I32, FLD_I64, FLD_SKIP, FLD_STR=16};
#define HAS_LMDB
#endif

#define E(_x) ERR_MDB((_x), #_x, __FILE__, __LINE__)
static inline int ERR_MDB(int n, char * tn, char *fn, int ln)
{
    if (n == MDB_SUCCESS || n == MDB_NOTFOUND) return n;
    printlog("%s:%d: %s: %s", fn, ln, tn, mdb_strerror(n)),
    abort();
}

static int userdb_open = 0;

MDB_env *mcc_mdb_env = 0;
MDB_dbi dbi_rec;
MDB_dbi dbi_ind1;
#define env mcc_mdb_env

void
write_userrec(userrec_t * userrec, int ini_too)
{
    uint8_t user_key[NB_SLEN*4];
    copy_user_key(user_key, userrec->user_id);

    if (ini_too) {
	if (!userrec->ini_dirty)
	    read_ini_file_fields(userrec);

	char userini[PATH_MAX];
	saprintf(userini, USER_INI_NAME, user_key);

	user_ini_tgt = userrec;
	save_ini_file(user_ini_fields, userini);
	user_ini_tgt = 0;
	userrec->ini_dirty = 0;
    }

    if (!userdb_open) open_userdb();

    MDB_txn *txn;
    E(mdb_txn_begin(env, NULL, 0, &txn));

    if (userrec->user_no == 0)
	init_userrec(txn, userrec);

    put_userrec(txn, user_key, userrec);

    E(mdb_txn_commit(txn));
}

LOCAL void
put_userrec(MDB_txn *txn, char * user_key, userrec_t * userrec)
{
    uint8_t recbuf[sizeof(userrec_t) * 4];
    uint8_t * p = recbuf;

    write_fld(&p, &userrec->user_no, FLD_I32);
    write_fld(&p, userrec->user_id, FLD_STR);
    write_fld(&p, &userrec->blocks_placed, FLD_I64);
    write_fld(&p, &userrec->blocks_deleted, FLD_I64);
    write_fld(&p, &userrec->blocks_drawn, FLD_I64);
    write_fld(&p, &userrec->first_logon, FLD_I64);
    write_fld(&p, &userrec->last_logon, FLD_I64);
    write_fld(&p, &userrec->logon_count, FLD_I64);
    write_fld(&p, &userrec->kick_count, FLD_I64);
    write_fld(&p, &userrec->death_count, FLD_I64);
    write_fld(&p, &userrec->message_count, FLD_I64);
    write_fld(&p, userrec->last_ip, FLD_STR);
    write_fld(&p, &userrec->time_online_secs, FLD_I64);

    uint8_t idbuf[4];
    {
	uint8_t * p2 = idbuf;
	write_int32(&p2, userrec->user_no);
    }

    MDB_val key, data;
    key.mv_size = sizeof(idbuf);
    key.mv_data = idbuf;
    data.mv_size = p-recbuf;
    data.mv_data = recbuf;
    E(mdb_put(txn, dbi_rec, &key, &data, 0) );

    key.mv_size = strlen(user_key);
    key.mv_data = user_key;
    data.mv_size = sizeof(idbuf);
    data.mv_data = idbuf;
    E(mdb_put(txn, dbi_ind1, &key, &data, 0) );
}

// If user_id arg is zero we use the user_no field so we can load the
// DB data (specifically the full name) for translation of id to name.
int
read_userrec(userrec_t * rec_buf, char * user_id, int load_ini)
{
    if (user_id && *user_id == 0) return -1;
    if (user_id == 0 && rec_buf->user_no == 0) return -1;

    uint8_t user_key[NB_SLEN*4];
    if (user_id)
	copy_user_key(user_key, user_id);

    // INI file loaded first, overridden by DB
    if (user_id && load_ini) {
	char userini[PATH_MAX];
	rec_buf->click_distance = -1;  // Map default
	user_ini_tgt = rec_buf;
	saprintf(userini, USER_INI_NAME, user_key);
	load_ini_file(user_ini_fields, userini, 1, 0);
	user_ini_tgt = 0;
    }

    if (!userdb_open) open_userdb();

    MDB_txn *txn;
    E(mdb_txn_begin(env, NULL, MDB_RDONLY, &txn));

    int rv;
    MDB_val key2, data;
    uint8_t idbuf[4];

    if (user_id == 0) {
	uint8_t *p2 = idbuf;
	write_int32(&p2, rec_buf->user_no);
	key2.mv_size = sizeof(idbuf);
	key2.mv_data = idbuf;

    } else {
	MDB_val key;

	key.mv_size = strlen(user_key);
	key.mv_data = user_key;

	rv = E(mdb_get(txn, dbi_ind1, &key, &key2));
	if (rv == MDB_NOTFOUND) {
	    mdb_txn_abort(txn);
	    return -1;
	}
    }

    rv = E(mdb_get(txn, dbi_rec, &key2, &data));
    if (rv == MDB_NOTFOUND) {
	mdb_txn_abort(txn);
	return -1;
    }

    // Copy data to the dest before we leave the txn.
    uint8_t *p = data.mv_data;
    int bytes = data.mv_size;

#ifndef DELETE_OLDBIN
    // Old binary records.
    if (bytes >= 16 && *p == 0) {
	memset(rec_buf, 0, sizeof(*rec_buf));
	read_bin_userrec(rec_buf, p, bytes);
	E(mdb_txn_commit(txn));
	return 0;
    }
#endif

    read_fld(&p, &bytes, &rec_buf->user_no, FLD_I32, 0);
    read_fld(&p, &bytes, rec_buf->user_id, FLD_STR, sizeof(rec_buf->user_id));
    read_fld(&p, &bytes, &rec_buf->blocks_placed, FLD_I64, 0);
    read_fld(&p, &bytes, &rec_buf->blocks_deleted, FLD_I64, 0);
    read_fld(&p, &bytes, &rec_buf->blocks_drawn, FLD_I64, 0);
    if (rec_buf->first_logon == 0) // Set once.
	read_fld(&p, &bytes, &rec_buf->first_logon, FLD_I64, 0);
    else
	read_fld(&p, &bytes, 0, FLD_SKIP, 0);
    read_fld(&p, &bytes, &rec_buf->last_logon, FLD_I64, 0);
    read_fld(&p, &bytes, &rec_buf->logon_count, FLD_I64, 0);
    read_fld(&p, &bytes, &rec_buf->kick_count, FLD_I64, 0);
    read_fld(&p, &bytes, &rec_buf->death_count, FLD_I64, 0);
    read_fld(&p, &bytes, &rec_buf->message_count, FLD_I64, 0);
    read_fld(&p, &bytes, 0, FLD_SKIP, 0);
    read_fld(&p, &bytes, &rec_buf->time_online_secs, FLD_I64, 0);

    E(mdb_txn_commit(txn));

    return 0;
}

LOCAL void
init_userrec(MDB_txn * txn, userrec_t * rec_buf)
{
    // fetch next id
    //if (!userdb_open) open_userdb();
    uint8_t buf[16], *p = buf;
    int idno = 0;
    write_int32(&p, idno);

    MDB_val key, data;
    key.mv_size = sizeof(uint32_t);
    key.mv_data = buf;

    int rv = E(mdb_get(txn, dbi_rec, &key, &data));
    if (rv != MDB_NOTFOUND) {
	uint8_t *p = data.mv_data;
	if (data.mv_size >= 4)
	    idno = IntBE32(p);
    }
    idno ++;
    uint8_t buf2[16];
    p = buf2;
    write_int32(&p, idno);

    data.mv_size = sizeof(uint32_t);
    data.mv_data = buf2;
    E(mdb_put(txn, dbi_rec, &key, &data, 0) );

    rec_buf->user_no = idno;
}

int
match_user_name(char * partname, char * namebuf, int l, int quiet, int *skip_count)
{
    if (!partname || !*partname || strlen(partname) > l) {
	if (!quiet) printf_chat("The user pattern given is invalid.");
	return -1;
    }

    if (!userdb_open) open_userdb();
    MDB_txn *txn;
    E(mdb_txn_begin(env, NULL, MDB_RDONLY, &txn));

    MDB_cursor *curs;

    E(mdb_cursor_open(txn, dbi_ind1, &curs));

    MDB_val key = {0}, data = {0};

    int found = 0, skipped = 0;
    int n = mdb_cursor_get(curs, &key, &data, MDB_FIRST);
    for( ; n == MDB_SUCCESS; n = mdb_cursor_get(curs, &key, &data, MDB_NEXT))
    {
	char user_name[NB_SLEN*4];
	decode_user_key(key.mv_data, user_name, sizeof(user_name));
	if (strcasecmp(user_name, partname) == 0) {
	    // "Exact" match.
	    snprintf(namebuf, l, "%s", user_name);
	    found = 1;
	    break;
	} else if (my_strcasestr(user_name, partname)) {
	    if (found == 0)
		snprintf(namebuf, l, "%s", user_name);
	    else if (strlen(namebuf) + strlen(user_name) + 3 < l) {
		strcat(namebuf, ", ");
		strcat(namebuf, user_name);
	    } else
		skipped++;
	    found++;
	}
    }
    if (n != MDB_SUCCESS && n != MDB_NOTFOUND) {
	printf_chat("&WSearch of user index failed");
        mdb_txn_abort(txn);
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

    E(mdb_txn_commit(txn));
    if (skip_count) *skip_count = skipped;
    return found;
}

LOCAL void
open_userdb()
{
    if (env) { userdb_open = 1; return; }

    for(int tries = 0; tries < 2; tries++) {
	E(mdb_env_create(&env));
	// Maximum size of DB, MUST lock if done after mdb_env_open
	// E(mdb_env_set_mapsize(env, 10485760));
	E(mdb_env_set_maxdbs(env, 8));
	E(mdb_env_set_maxreaders(env, MAX_USER+4));
	int rv = mdb_env_open(env, USERDB_FILE, MDB_NOSUBDIR, 0664);

	if (rv != MDB_SUCCESS) {
    #if USERDB_RECREATE
	    if (tries == 0 && (rv == MDB_VERSION_MISMATCH || rv == MDB_INVALID)) {
		printlog("Cannot open \"%s\": %s", USERDB_FILE, mdb_strerror(rv)),
		unlink(USERDB_FILE);
		char buf[256];
		saprintf(buf, "%s-lock", USERDB_FILE);
		unlink(buf);
		mdb_env_close(env);
		continue;
	    }
    #endif
	    printlog("Cannot open \"%s\": %s", USERDB_FILE, mdb_strerror(rv)),
	    exit(1);
	}
	break;
    }

    // Should I set the FD_CLOEXEC flag on this fd?
    // E(mdb_env_get_fd(env, ...

    // Make sure the "tables" and "indexes" exist
    MDB_txn *txn;
    E(mdb_txn_begin(env, NULL, 0, &txn));

    E(mdb_dbi_open(txn, "user_rec", MDB_CREATE, &dbi_rec));
    E(mdb_dbi_open(txn, "user_id", MDB_CREATE, &dbi_ind1));

    check_userdb_exists(txn);

    E(mdb_txn_commit(txn));
}

void
close_userdb()
{
    if (!userdb_open || env == 0) return;
    mdb_env_close(env);
    userdb_open = 0;
    env = 0;
}

#if INTERFACE
inline static void
write_int32(uint8_t **pp, int32_t val)
{
    uint8_t *p = *pp;
    *p++ = (val>>24);
    *p++ = (val>>16);
    *p++ = (val>>8);
    *p++ = (val&0xFF);
    *pp = p;
}
#endif

LOCAL void
write_fld(uint8_t **pp, void * data, enum csv_type type)
{
    int64_t v = 0;
    char nbuf[sizeof(v)*3+3];

    switch(type) {
    case FLD_I32:
    case FLD_I64:
	switch(type) {
	case FLD_I32:
	    v = *(int32_t*)data;
	    break;
	case FLD_I64:
	    v = *(int64_t*)data;
	    break;
	default:break; //STFU idiot
	}
	int l = sprintf(nbuf, "%jd,", (intmax_t)v);
	strcpy(*pp, nbuf);
	(*pp) += l;
	return;

    case FLD_STR:
	{
	    int use_quote = 0;
	    for(uint8_t *s = data; *s; s++)
		if (!isalnum(*s)) {
		    use_quote = 1;
		    break;
		}

	    char * p = *pp;
	    if (use_quote) *p++ = '"';
	    for(uint8_t *s = data; *s; s++) {
		if (*s == '"') *p++ = '"';
		*p++ = *s;
	    }

	    if (use_quote) *p++ = '"';
	    *p++ = ',';
	    *pp = p;
	}
	break;

    default:
	*(*pp)++ = ','; // Unknown field type for later.
	return;
    }
}

LOCAL void
read_fld(uint8_t **pp, int * bytes, void * data, enum csv_type type, int len)
{
    uint8_t *p = *pp;
    int l = *bytes, e = 0, ec = ',';
    if (p[0] == '"') { ec = '"'; e++; }
    for(; e < l ; e++) {
	if (p[e] == ec) {
	    if (ec == ',') break;
	    if (e+1>=l || p[e+1] != ec) break;
	    e++;
	}
    }
    int k = e;
    if (ec == '"' && p[e] == '"') e++;

    switch(type) {
    case FLD_I32:
    case FLD_I64:
	{
	    char nbuf[sizeof(long long)*3+3];
	    char * en = 0;
	    int nl = k-(ec=='"');
	    if (nl >= sizeof(nbuf)) nl = sizeof(nbuf)-1;
	    memcpy(nbuf, p+(ec=='"'), nl);
	    nbuf[nl] = 0;
	    long long v = strtoll(nbuf, &en, 10);

	    switch(type) {
	    case FLD_I32:
		*(int32_t*)data = v;
		break;
	    case FLD_I64:
		*(int64_t*)data = v;
		break;
	    default:break; //STFU idiot
	    }
	    *pp += e+1;
	}
	return;

    case FLD_STR:
	{
	    int s = (ec == '"'), d = 0;
	    uint8_t * dt = data;
	    while (s < k && d < len) {
		if (p[s] == '"') s++;
		dt[d++] = p[s++];
	    }
	    if (d<len) dt[d] = 0; else dt[len-1] = 0;
	    *pp += e+1;
	}
	break;

    default:
    case FLD_SKIP:
	*pp += e+1;
	return;
    }
}

LOCAL void
check_userdb_exists(MDB_txn *txn)
{
    if (userdb_open) return;

    uint8_t buf[16], *p = buf;
    int idno = 0;
    write_int32(&p, idno);

    MDB_val key, data;
    key.mv_size = sizeof(uint32_t);
    key.mv_data = buf;

    int rv = E(mdb_get(txn, dbi_rec, &key, &data));
    if (rv != MDB_NOTFOUND) {
	uint8_t *p = data.mv_data;
	if (data.mv_size >= 4) {
	    idno = IntBE32(p);
	    if (idno >= 0) return;
	}
    }

    idno = rebuild_user_database(txn);

    uint8_t buf2[16];
    p = buf2;
    write_int32(&p, idno);

    data.mv_size = sizeof(uint32_t);
    data.mv_data = buf2;
    E(mdb_put(txn, dbi_rec, &key, &data, 0) );
}

LOCAL int
rebuild_user_database(MDB_txn *txn)
{
    struct dirent *entry;
    DIR *directory = opendir(USER_DIR);
    if (!directory) return 0;

    printlog("Rebuilding user index");
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

	uint8_t user_key[NB_SLEN*4];
	copy_user_key(user_key, rec_buf->user_id);
	put_userrec(txn, user_key, rec_buf);

	if (rec_buf->user_no > maxno) maxno = rec_buf->user_no;
    }
    closedir(directory);
    return maxno;
}

#ifndef DELETE_OLDBIN
static inline void
read_bin_fld(uint8_t **pp, int * bytes, void * data, int type)
{
    switch(type) {
    case FLD_I32:
	{
	    if (*bytes < 4) { *bytes = 0; *(uint32_t*)data = 0; return; }
	    uint64_t v = 0;
	    for(int i = 0; i<4; i++) {
		v <<= 8;
		v += (*pp)[i];
	    }
	    *(uint32_t*)data = v;
	    (*pp)+=4;
	    *bytes-=4;
	    break;
	}
    case FLD_I64:
	{
	    if (*bytes < 8) { *bytes = 0; *(uint64_t*)data = 0; return; }
	    uint64_t v = 0;
	    for(int i = 0; i<8; i++) {
		v <<= 8;
		v += (*pp)[i];
	    }
	    *(uint64_t*)data = v;
	    (*pp)+=8;
	    *bytes-=8;
	    break;
	}
    default:
	if (type >= FLD_STR) {
	    if (*bytes < type) { *bytes = 0; *(char*)data = 0; return; }
	    memcpy(data, *pp, type);
	    (*pp) += type;
	    *bytes -= type;
	    break;
	}
    }
}

LOCAL void
read_bin_userrec(userrec_t * rec_buf, uint8_t *p, int bytes)
{
    read_bin_fld(&p, &bytes, &rec_buf->user_no, FLD_I32);
    read_bin_fld(&p, &bytes, rec_buf->user_id, sizeof(rec_buf->user_id));
    read_bin_fld(&p, &bytes, &rec_buf->blocks_placed, FLD_I64);
    read_bin_fld(&p, &bytes, &rec_buf->blocks_deleted, FLD_I64);
    read_bin_fld(&p, &bytes, &rec_buf->blocks_drawn, FLD_I64);
    read_bin_fld(&p, &bytes, &rec_buf->first_logon, FLD_I64);
    read_bin_fld(&p, &bytes, &rec_buf->last_logon, FLD_I64);
    read_bin_fld(&p, &bytes, &rec_buf->logon_count, FLD_I64);
    // No more fields ever written to bin format.
    rec_buf->dirty = 1;
}
#endif
#endif
