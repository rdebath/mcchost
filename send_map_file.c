#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <zlib.h>

#include "send_map_file.h"

#if INTERFACE
#define block_convert(_bl) ((_bl)<=max_blockno_to_send?_bl:f_block_convert(_bl))
#endif

// Convert from Map block numbers to ones the client will understand.
block_t
f_block_convert(block_t in)
{
    // CPE defined translations.
    static block_t cpe_conv[] = {
	0x2C, 0x27, 0x0C, 0x00, 0x0A, 0x21, 0x19, 0x03,
	0x1d, 0x1c, 0x14, 0x2a, 0x31, 0x24, 0x05, 0x01
    };

    if (in <= max_blockno_to_send) return in;
    if (in >= BLOCKMAX) in = BLOCKMAX-1;

    if (level_prop->blockdef[in].defined)
	in = level_prop->blockdef[in].fallback;

    if (in > max_blockno_to_send && in >= 50 && in < 66) in = cpe_conv[in-50];

    return in > max_blockno_to_send ? Block_Bedrock : in;
}

void
send_map_file()
{
    int zrv = 0;
    int blocks_buffered = 0;
    set_last_block_queue_id(); // Send updates from now.
    send_lvlinit_pkt();

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
    zrv = deflateInit2 (&strm, Z_BEST_SPEED, Z_DEFLATED,
                             MAX_WBITS | 16, 8,
                             Z_DEFAULT_STRATEGY);
    if (zrv != Z_OK) {
	char buf[256];
	sprintf(buf, "ZLib:deflateInit2() failed RV=%d.\n", zrv);
	fatal(buf);
    }

    strm.next_in = blockbuffer;
    strm.avail_in = 4;

    intptr_t level_blocks_used = 0;
    unsigned char zblockbuffer[1024];
    int percent = 0;
    uintptr_t blocks_sent = 0;

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
	    char buf[256];
	    sprintf(buf, "ZLib:deflate() failed RV=%d.\n", zrv);
	    fatal(buf);
	}

	if (strm.avail_out == 0 || (zrv == Z_STREAM_END && sizeof(zblockbuffer) != strm.avail_out)) {
	    send_lvldata_pkt(zblockbuffer, sizeof(zblockbuffer)-strm.avail_out, percent);
	    blocks_sent += 1;
	    strm.avail_out = sizeof(zblockbuffer);
	    strm.next_out = zblockbuffer;
	    percent = level_blocks_used * 100 / level_len;

	    blocks_buffered ++;
	    if (blocks_buffered > 64)
		flush_to_remote();
	}

    } while(zrv != Z_STREAM_END);

    deflateEnd(&strm);

    send_lvldone_pkt(level_prop->cells_x, level_prop->cells_y, level_prop->cells_z);

    // Record for next time.
    level_prop->last_map_download_size = blocks_sent * 1028 + 8;
}
