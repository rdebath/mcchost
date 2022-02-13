#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <zlib.h>

#include "send_map_file.h"

#define ABORT_ZLIB(x) {                                                  \
        int status;                                                     \
        status = x;                                                     \
        if (status < 0) {                                               \
            fprintf (stderr,                                            \
                     "%s:%d: %s returned a bad status of %d.\n",        \
                     __FILE__, __LINE__, #x, status);                   \
            exit (EXIT_FAILURE);                                        \
        }                                                               \
    }

static inline int
block_convert(int in)
{
    return in;
}

void
send_map_file(int ofd)
{
    int zrv = 0;
    send_lvlinit_pkt(ofd);

    uintptr_t level_len = (uintptr_t)level_prop->cells_x * level_prop->cells_y * level_prop->cells_z;
    unsigned char blockbuffer[4096];
    blockbuffer[0] = (level_len>>24);
    blockbuffer[1] = (level_len>>16);
    blockbuffer[2] = (level_len>>8);
    blockbuffer[3] = (level_len&0xFF);

    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree  = Z_NULL;
    strm.opaque = Z_NULL;
    zrv = deflateInit2 (&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                             MAX_WBITS | 16, 8,
                             Z_DEFAULT_STRATEGY);
    if (zrv != Z_OK) {
	fprintf(stderr,
		 "%s:%d: deflateInit2() returned a bad status of %d.\n",
		 __FILE__, __LINE__, zrv);
	exit(1);
    }

    strm.next_in = blockbuffer;
    strm.avail_in = 4;

    intptr_t level_blocks_used = 0;
    unsigned char zblockbuffer[1024];
    int percent = 0;

    strm.avail_out = sizeof(zblockbuffer);
    strm.next_out = zblockbuffer;

    do {
	if (strm.avail_in == 0 && level_blocks_used < level_len) {
	    // copy up to 4096 bytes from level_blocks
	    strm.next_in = blockbuffer;
	    strm.avail_in = 0;

	    for(int i=0; i<sizeof(blockbuffer) && level_blocks_used < level_len; i++)
	    {
		blockbuffer[i] = block_convert(level_blocks[level_blocks_used]);
		level_blocks_used += 1;
		strm.avail_in += 1;
	    }
	}

        zrv = deflate(&strm, strm.avail_in != 0?Z_NO_FLUSH:Z_FINISH);
	if (zrv != Z_OK && zrv != Z_STREAM_END && zrv != Z_BUF_ERROR) {
	    fprintf(stderr,
		     "%s:%d: deflate() returned a bad status of %d.\n",
		     __FILE__, __LINE__, zrv);
	    exit(1);
	}

	if (strm.avail_out == 0 || (zrv == Z_STREAM_END && sizeof(zblockbuffer) != strm.avail_out)) {
	    send_lvldata_pkt(ofd, zblockbuffer, sizeof(zblockbuffer)-strm.avail_out, percent);
	    strm.avail_out = sizeof(zblockbuffer);
	    strm.next_out = zblockbuffer;
	    percent = level_blocks_used * 100 / level_len;
	}

    } while(zrv != Z_STREAM_END);

    deflateEnd(&strm);

    send_lvldone_pkt(ofd, level_prop->cells_x, level_prop->cells_y, level_prop->cells_z);
}
