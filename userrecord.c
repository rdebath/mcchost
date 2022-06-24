#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <arpa/inet.h>

#include <lmdb.h>
#include "userrecord.h"

/*
 * User details.
 * This converts the userrec_t structure into a portable format and writes it
 * to an lmbd file. Note, this is used in a very simple manner and could
 * in theory just use lots of tiny files.
 *
 */
#if INTERFACE
typedef struct userrec_t userrec_t;
struct userrec_t
{
    int32_t user_no;
    char user_id[NB_SLEN];
    uint64_t blocks_placed;
    uint64_t blocks_deleted;
    uint64_t blocks_drawn;
    uint64_t first_logon;	// time_t is sometimes 32bits.
    uint64_t last_logon;
    uint64_t logon_count;
    uint64_t kick_count;

    int dirty;
}
#endif

#define E(_x) ERR_MDB((_x), #_x, __FILE__, __LINE__)
static inline int ERR_MDB(int n, char * tn, char *fn, int ln)
{
    if (n == MDB_SUCCESS || n == MDB_NOTFOUND) return n;
    fprintf(stderr, "%s:%d: %s: %s\n", fn, ln, tn, mdb_strerror(n)),
    abort();
}

userrec_t my_user = {.user_no = 0};
static int userdb_open = 0;

MDB_env *mcc_mdb_env = 0;
MDB_dbi dbi_rec;
MDB_dbi dbi_ind1;
#define env mcc_mdb_env

enum {FLD_I32, FLD_I64, FLD_STR=16};

static inline void
write_fld(uint8_t **pp, void * data, int type)
{
    switch(type) {
    case FLD_I32:
	{
	    uint32_t v = htonl(*(uint32_t*)data);
	    memcpy(*pp, &v, 4);
	    (*pp)+=4;
	    break;
	}
    case FLD_I64:
	{
	    uint64_t v = *(uint64_t*)data;
	    for(int i = 0; i<8; i++) {
		(*pp)[7-i] = v & 0xFF;
		v >>= 8;
	    }
	    (*pp)+=8;
	    break;
	}
    default:
	if (type >= FLD_STR) {
	    memcpy(*pp, data, type);
	    (*pp) += type;
	    break;
	}
    }
}

static inline void
read_fld(uint8_t **pp, int * bytes, void * data, int type)
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

static void
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

void
write_userrec(int when)
{
    if (my_user.user_no != 0 && !my_user.dirty && when == 0) return;

    if (my_user.user_no == 0)
	if (read_userrec(&my_user, user_id) < 0)
	    my_user.user_no = 0;

    if (!userdb_open) open_userdb();

    uint8_t user_key[NB_SLEN*4];
    copy_user_key(user_key, user_id);

    if (when == 1) {
	if (!my_user.first_logon) my_user.first_logon = time(0);
	my_user.last_logon = time(0);
	my_user.logon_count++;
    }

    uint8_t recbuf[sizeof(userrec_t) * 2];
    uint8_t * p = recbuf;

    write_fld(&p, &my_user.user_no, FLD_I32);
    write_fld(&p, my_user.user_id, sizeof(my_user.user_id));
    write_fld(&p, &my_user.blocks_placed, FLD_I64);
    write_fld(&p, &my_user.blocks_deleted, FLD_I64);
    write_fld(&p, &my_user.blocks_drawn, FLD_I64);
    write_fld(&p, &my_user.first_logon, FLD_I64);
    write_fld(&p, &my_user.last_logon, FLD_I64);
    write_fld(&p, &my_user.logon_count, FLD_I64);

    MDB_txn *txn;
    E(mdb_txn_begin(env, NULL, 0, &txn));

    if (my_user.user_no == 0) {
	init_userrec(txn, &my_user);
	uint8_t * p2 = recbuf;
	write_fld(&p2, &my_user.user_no, FLD_I32);
	write_fld(&p2, my_user.user_id, sizeof(my_user.user_id));
    }

    MDB_val key, data;
    key.mv_size = sizeof(uint32_t);
    key.mv_data = recbuf;
    data.mv_size = p-recbuf;
    data.mv_data = recbuf;
    E(mdb_put(txn, dbi_rec, &key, &data, 0) );

    key.mv_size = strlen(user_key);
    key.mv_data = user_key;
    data.mv_size = sizeof(uint32_t);
    data.mv_data = recbuf;
    E(mdb_put(txn, dbi_ind1, &key, &data, 0) );

    E(mdb_txn_commit(txn));
}

int
read_userrec(userrec_t * rec_buf, char * user_id)
{
    if (!userdb_open) open_userdb();

    MDB_txn *txn;
    E(mdb_txn_begin(env, NULL, MDB_RDONLY, &txn));

    uint8_t user_key[NB_SLEN*4];
    copy_user_key(user_key, user_id);

    MDB_val key, key2, data;

    key.mv_size = strlen(user_key);
    key.mv_data = user_key;

    int rv = E(mdb_get(txn, dbi_ind1, &key, &key2));
    if (rv == MDB_NOTFOUND) {
	mdb_txn_abort(txn);
	return -1;
    }

    rv = E(mdb_get(txn, dbi_rec, &key2, &data));
    if (rv == MDB_NOTFOUND) {
	mdb_txn_abort(txn);
	return -1;
    }

    // Copy data to the dest before we leave the txn.
    uint8_t *p = data.mv_data;
    int bytes = data.mv_size;

    read_fld(&p, &bytes, &rec_buf->user_no, FLD_I32);
    read_fld(&p, &bytes, rec_buf->user_id, sizeof(rec_buf->user_id));
    read_fld(&p, &bytes, &rec_buf->blocks_placed, FLD_I64);
    read_fld(&p, &bytes, &rec_buf->blocks_deleted, FLD_I64);
    read_fld(&p, &bytes, &rec_buf->blocks_drawn, FLD_I64);
    read_fld(&p, &bytes, &rec_buf->first_logon, FLD_I64);
    read_fld(&p, &bytes, &rec_buf->last_logon, FLD_I64);
    read_fld(&p, &bytes, &rec_buf->logon_count, FLD_I64);

    E(mdb_txn_commit(txn));

    return 0;
}

void
init_userrec(MDB_txn * txn, userrec_t * rec_buf)
{
    // fetch next id
    //if (!userdb_open) open_userdb();
    uint8_t buf[16], *p = buf;
    int idno = 0;
    write_fld(&p, &idno, FLD_I32);

    MDB_val key, data;
    key.mv_size = sizeof(uint32_t);
    key.mv_data = buf;

    int rv = E(mdb_get(txn, dbi_rec, &key, &data));
    if (rv != MDB_NOTFOUND) {
	uint8_t *p = data.mv_data;
	int bytes = data.mv_size;
	read_fld(&p, &bytes, &idno, FLD_I32);
    }
    idno ++;
    uint8_t buf2[16];
    p = buf2;
    write_fld(&p, &idno, FLD_I32);

    data.mv_size = sizeof(uint32_t);
    data.mv_data = buf2;
    E(mdb_put(txn, dbi_rec, &key, &data, 0) );

    rec_buf->user_no = idno;
    strcpy(rec_buf->user_id, user_id);
}

LOCAL void
open_userdb()
{
    if (env) { userdb_open = 1; return; }

    E(mdb_env_create(&env));
    // Maximum size of DB, MUST lock if done after mdb_env_open
    // E(mdb_env_set_mapsize(env, 10485760));
    E(mdb_env_set_maxdbs(env, 8));
    E(mdb_env_set_maxreaders(env, server->max_players+3));
    E(mdb_env_open(env, USERDB_DIR, 0/*MDB_RDWR*/, 0664));

    // Make sure the "tables" and "indexes" exist
    MDB_txn *txn;
    E(mdb_txn_begin(env, NULL, 0, &txn));

    E(mdb_dbi_open(txn, "user_rec", MDB_CREATE, &dbi_rec));
    E(mdb_dbi_open(txn, "user_id", MDB_CREATE, &dbi_ind1));

    E(mdb_txn_commit(txn));
}
