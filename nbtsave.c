#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
#include <limits.h>

#include "nbtsave.h"

enum NbtTagType {
    NBT_END, NBT_I8, NBT_I16, NBT_I32, NBT_I64, NBT_F32,
    NBT_F64, NBT_I8ARRAY, NBT_STR, NBT_LIST, NBT_COMPOUND
};

static char *entpropnames[6] = {
    "ModelRotX", "ModelRotY", "ModelRotZ",
    "ModelScaleX", "ModelScaleY", "ModelScaleZ"
    };

/*
 * This writes a *.cw file.
 *
 * BEWARE: MCGalaxy's reader is very badly pedantic with many limitations
 *         beyond the specifications of the format.
 *
 *         One specific one being that every "path" must be unique.
 *
 * I've put the block arrays at the end of the file, this should allow a
 * quick scan of the properties if necessary.
 */
void
save_map_to_file(char * fn)
{
    FILE * savefile;

    if (!fn || *fn == 0) return;

    if (strlen(fn) > PATH_MAX-64) {
	fprintf(stderr, "Filename too long\n");
	return;
    } else {
	char cmdbuf[PATH_MAX];
	char * p = fn;
	char * d;
	strcpy(cmdbuf, "gzip>'");
	d = cmdbuf+strlen(cmdbuf);
	for(;*p;p++) {
	    if((d-cmdbuf) >= sizeof(cmdbuf)-8) {
		fprintf(stderr, "Filename too long\n");
		return;
	    }
	    if (*p == '\'') {
		*d++ = '\'';
		*d++ = '\\';
		*d++ = '\'';
		*d++ = '\'';
	    } else if (*p > ' ' && *p < '~') {
		*d++ = *p;
	    } else {
		fprintf(stderr, "Filename conversion error character 0x%02x.\n", *p & 0xFF);
		return;
	    }
	}
	*d++ = '\'';
	*d = 0;

	savefile = popen(cmdbuf, "w");

	if (!savefile) {
	    perror(fn);
	    return;
	}
    }

    signal(SIGPIPE, SIG_IGN);	// If gzip or shell fails, check later.

    bc_compound(savefile, "ClassicWorld");
    bc_ent_int(savefile, "FormatVersion", 1);
    {
	char buf[16] = {0};
	bc_ent_bytes(savefile, "UUID", buf, sizeof(buf));
    }

    bc_ent_int16(savefile, "X", level_prop->cells_x);
    bc_ent_int16(savefile, "Y", level_prop->cells_y);
    bc_ent_int16(savefile, "Z", level_prop->cells_z);

    bc_compound(savefile, "Spawn");
    bc_ent_int16(savefile, "X", level_prop->spawn.x/32);
    bc_ent_int16(savefile, "Y", level_prop->spawn.y/32);
    bc_ent_int16(savefile, "Z", level_prop->spawn.z/32);
    bc_ent_int8(savefile, "H", level_prop->spawn.h);
    bc_ent_int8(savefile, "P", level_prop->spawn.v);
    bc_end(savefile);

    bc_compound(savefile, "Metadata");
    bc_compound(savefile, "CPE");

    bc_compound(savefile, "EnvWeatherType");
    bc_ent_int8(savefile, "WeatherType", level_prop->weather);
    bc_end(savefile);
    bc_compound(savefile, "EnvColors");
    bc_colour(savefile, "Sky", level_prop->sky_colour);
    bc_colour(savefile, "Cloud", level_prop->cloud_colour);
    bc_colour(savefile, "Fog", level_prop->fog_colour);
    bc_colour(savefile, "Ambient", level_prop->ambient_colour);
    bc_colour(savefile, "Sunlight", level_prop->sunlight_colour);
    bc_end(savefile);

    bc_compound(savefile, "EnvMapAppearance");
    bc_ent_int8(savefile, "SideBlock", level_prop->side_block);
    bc_ent_int8(savefile, "EdgeBlock", level_prop->edge_block);
    bc_ent_int16(savefile, "SideLevel", level_prop->side_level);
    nbtstr_t b = level_prop->texname;
    bc_ent_string(savefile, "TextureURL", b.c, 0);
    bc_end(savefile);

    int bdopen = 0;
    for(int i=1; i<BLOCKMAX; i++)
	if (level_prop->blockdef[i].defined) {
	    if (!bdopen) {
		bc_compound(savefile, "BlockDefinitions");
		bdopen = 1;
	    }
	    save_block_def(savefile, i, (void*)(&level_prop->blockdef[i]));
	}
    if (bdopen)
	bc_end(savefile);

    bdopen = 0;
    for(int i=1; i<BLOCKMAX; i++)
	if (level_prop->blockdef[i].defined)
	{
	    int ord = level_prop->invt_order[i];
	    if (ord < 0) ord = 0;
	    if (ord != i) {
		if (!bdopen) {
		    bc_compound(savefile, "InventoryOrder");
		    bdopen = 1;
		}

		char uniquename[64]; // Limitation of MCGalaxy loader.
		sprintf(uniquename, "inventoryorder%04x", i);

		bc_compound(savefile, uniquename);
		bc_ent_int(savefile, "block", i);
		bc_ent_int(savefile, "order", ord);
		bc_end(savefile);
	    }
	}

    if (bdopen)
	bc_end(savefile);

    bc_end(savefile);
    bc_end(savefile);

    if (level_blocks) {
	int flg = 0, flg2 = 0;
	bc_ent_bytes_header(savefile, "BlockArray", level_prop->total_blocks);

	for (intptr_t i = 0; i<level_prop->total_blocks; i++) {
	    int b = level_blocks[i];
	    // if (b>=BLOCKMAX) b = BLOCKMAX-1;
	    if (b>=768) {flg2 = 1; b &= 0xFF; }
	    fputc(b & 0xFF, savefile);
	    flg |= (b>0xFF);
	}

	if (flg) {
	    // BlockArray2 can only have 0..767 so we need BlockArray3
	    bc_ent_bytes_header(savefile, "BlockArray2", level_prop->total_blocks);

	    for (intptr_t i = 0; i<level_prop->total_blocks; i++) {
		int b = level_blocks[i];
		// if (b>=BLOCKMAX) b = BLOCKMAX-1;
		if (b>=768) b &= 0xFF;
		fputc(b>>8, savefile);
	    }
	}

	if (flg2) {
	    // What format should I use for this?
	    // Currently it's a corrected high byte but this means BA2 makes
	    // a random block in 0..767. This byte is only set if BA2 is wrong.
	    bc_ent_bytes_header(savefile, "BlockArray3", level_prop->total_blocks);

	    for (intptr_t i = 0; i<level_prop->total_blocks; i++) {
		int b = level_blocks[i];
		// if (b>=BLOCKMAX) b=BLOCKMAX-1;
		if (b>=768)
		    fputc(b>>8, savefile);
		else
		    fputc(0, savefile);
	    }

	    // Another option is to discard BA2 completely, put the 255
	    // fallback into BA1 and have BA3 and BA4 with the actual value.
	}

    }

    bc_end(savefile);

    int rv = pclose(savefile);
    if (rv)
	fprintf(stderr, "Save failed error %d\n", rv);

    signal(SIGPIPE, SIG_DFL);
}

LOCAL void
bc_string(FILE * ofd, char * str, int len)
{
    if (len) {
        while(len>0 && (str[len-1] == ' ' || str[len-1] == 0))
	    len--;
    } else {
        len = strlen(str);
    }
    fputc(len>>8, ofd);
    fputc(len&0xFF, ofd);
    fwrite(str, len, 1, ofd);
}

LOCAL void
bc_ent_label(FILE * ofd, char * name)
{
    bc_string(ofd, name, 0);
}

LOCAL void
bc_ent_string(FILE * ofd, char * name, char * str, int len)
{
    fputc(NBT_STR, ofd);
    bc_ent_label(ofd, name);
    bc_string(ofd, str, len);
}

LOCAL void
bc_ent_bytes_header(FILE * ofd, char * name, int len)
{
    fputc(NBT_I8ARRAY, ofd);
    bc_ent_label(ofd, name);
    fputc((len>>24) & 0xFF, ofd);
    fputc((len>>16) & 0xFF, ofd);
    fputc((len>>8)  & 0xFF, ofd);
    fputc( len      & 0xFF, ofd);
}

LOCAL void
bc_ent_bytes(FILE * ofd, char * name, char * bstr, int len)
{
    bc_ent_bytes_header(ofd, name, len);
    fwrite(bstr, len, 1, ofd);
}

LOCAL void
bc_ent_int(FILE * ofd, char * name, int val)
{
    if (val >= -128 && val <= 127) {
	fputc(NBT_I8, ofd);
	bc_ent_label(ofd, name);
	fputc(val & 0xFF, ofd);
	return;
    }

    if (val >= -32768 && val <= 32767) {
	fputc(NBT_I16, ofd);
	bc_ent_label(ofd, name);
	fputc((val>>8), ofd);
	fputc(val&0xFF, ofd);
	return;
    }

    fputc(NBT_I32, ofd);
    bc_ent_label(ofd, name);
    fputc((val>>24) & 0xFF, ofd);
    fputc((val>>16) & 0xFF, ofd);
    fputc((val>>8)  & 0xFF, ofd);
    fputc( val      & 0xFF, ofd);
}

LOCAL void
bc_ent_float(FILE * ofd, char * name, double fval)
{
    union { int32_t i32; float f32; } bad;
    bad.f32 = fval;
    int val = bad.i32;
    fputc(NBT_F32, ofd);
    bc_ent_label(ofd, name);
    fputc((val>>24) & 0xFF, ofd);
    fputc((val>>16) & 0xFF, ofd);
    fputc((val>>8)  & 0xFF, ofd);
    fputc( val      & 0xFF, ofd);
}

LOCAL void
bc_ent_int16(FILE * ofd, char * name, int val)
{
    fputc(NBT_I16, ofd);
    bc_ent_label(ofd, name);
    fputc((val>>8), ofd);
    fputc(val&0xFF, ofd);
}

LOCAL void
bc_ent_int8(FILE * ofd, char * name, int val)
{
    fputc(NBT_I8, ofd);
    bc_ent_label(ofd, name);
    fputc(val & 0xFF, ofd);
}

LOCAL void
bc_compound(FILE * ofd, char * name)
{
    fputc(NBT_COMPOUND, ofd);
    bc_ent_label(ofd, name);
}

LOCAL void
bc_end(FILE * ofd)
{
    fputc(NBT_END, ofd);
}

void
bc_colour(FILE * ofd, char * name, int colour)
{
    int r, g, b;
    if (colour < 0)
	r = g = b = -1;
    else {
	r = ((colour>>16) & 0xFF);
	g = ((colour>> 8) & 0xFF);
	b = ((colour>> 0) & 0xFF);
    }

    bc_compound(ofd, name);
    bc_ent_int16(ofd, "R", r);
    bc_ent_int16(ofd, "G", g);
    bc_ent_int16(ofd, "B", b);
    bc_end(ofd);
}

LOCAL void
save_block_def(FILE * ofd, int idno, blockdef_t * blkdef)
{
    char buf[64];

    sprintf(buf, "Block%04x", idno);

    bc_compound(ofd, buf);
    bc_ent_int8(ofd, "ID", idno);
    bc_ent_int16(ofd, "ID2", idno);

    bc_ent_string(ofd, "Name", blkdef->name.c, 64);
    bc_ent_int8(ofd, "CollideType", blkdef->collide);
    {
//	double log2 = 0.693147180559945f;
//	double spl2 = (UB(pkt[p++]) - 128) / 64.0;
//	double spd = exp(log2 * spl2);
	bc_ent_float(ofd, "Speed", blkdef->speed);
    }

    {
	char texbyte[12] = {0,0,0,0,0,0, 0,0,0,0,0,0};

	// CW File: Top, Bottom, Left, Right, Front, Back
	// Packet:  Top, Left, Right, Front, Back, Bottom

	texbyte[0]  = blkdef->textures[0];
	texbyte[1]  = blkdef->textures[5];
	texbyte[2]  = blkdef->textures[1];
	texbyte[3]  = blkdef->textures[2];
	texbyte[4]  = blkdef->textures[3];
	texbyte[5]  = blkdef->textures[4];
	texbyte[6]  = blkdef->textures[0] >> 8;
	texbyte[7]  = blkdef->textures[5] >> 8;
	texbyte[8]  = blkdef->textures[1] >> 8;
	texbyte[9]  = blkdef->textures[2] >> 8;
	texbyte[10] = blkdef->textures[3] >> 8;
	texbyte[11] = blkdef->textures[4] >> 8;
	bc_ent_bytes(ofd, "Textures", texbyte, 12);

    }

    bc_ent_int8(ofd, "TransmitsLight", blkdef->transmits_light);
    bc_ent_int8(ofd, "WalkSound", blkdef->walksound);
    bc_ent_int8(ofd, "FullBright", blkdef->fullbright);

    {
	char box[6] = {0,0,0,0,0,0};
	box[0] = blkdef->coords[0];	// min X
	box[1] = blkdef->coords[1];	// min Y
	box[2] = blkdef->coords[2];	// min Z
	box[3] = blkdef->coords[3];	// max X
	box[4] = blkdef->coords[4];	// max Y
	box[5] = blkdef->coords[5];	// max Z
	bc_ent_bytes(ofd, "Coords", box, 6);

	bc_ent_int8(ofd, "Shape", blkdef->shape);
    }

    bc_ent_int8(ofd, "BlockDraw", blkdef->draw);

    {
	char fog[4] = {0,0,0,0};
	fog[0] = blkdef->fog[0];
	fog[1] = blkdef->fog[1];
	fog[2] = blkdef->fog[2];
	fog[3] = blkdef->fog[3];
	bc_ent_bytes(ofd, "Fog", fog, 4);
    }

    bc_end(ofd);
}
