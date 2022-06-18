#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <limits.h>
#include <zlib.h>

#include "nbtsave.h"

/*
 * This writes a *.cw file.
 *
 * BEWARE: MCGalaxy's reader is very badly pedantic with many limitations
 *         beyond the specifications of the format.
 *
 *         One specific one being that every "path" must be unique which
 *         this encoder does NOT check.
 *
 * I've put the block arrays at the end of the file, this should allow a
 * quick scan of the properties if necessary.
 *
 * Some decoders insist on specific integer widths so standard fields
 * have defined widths.
 */
int
save_map_to_file(char * fn, int background)
{
    if (!fn || *fn == 0) return -1;

    // CC works with 2^^31 blocks (with cosmetic errors) so allow just one byte
    // over the limit. Note, however, that this does not apply to the CW file.
    // The last block in the array will thus become Block_Air (or undefined).

    // For MCGalaxy the size must not exceed (2^31-2^16) or [4681,128,3584]

    if (level_prop->total_blocks > 0x7FFFFFFF + (int64_t)1 ) {
	printlog("Oversized map \"%s\" (%d,%d,%d) failed to save\n",
	    fn, level_prop->cells_x, level_prop->cells_y, level_prop->cells_z);
	return -1;
    }

    gzFile savefile = gzopen(fn, "w9");

    if (!savefile) {
	if (!background)
	    printf_chat("&WMap save failed with error %d", errno);
	perror(fn);
	return -1;
    }

    bc_compound(savefile, "ClassicWorld");
    bc_ent_int8(savefile, "FormatVersion", 1);
    bc_ent_bytes(savefile, "UUID", (char*)level_prop->uuid, sizeof(level_prop->uuid));

    bc_ent_int16(savefile, "X", level_prop->cells_x);
    bc_ent_int16(savefile, "Y", level_prop->cells_y);
    bc_ent_int16(savefile, "Z", level_prop->cells_z);

    if (level_prop->time_created)
	bc_ent_int64(savefile, "TimeCreated", level_prop->time_created);
    if (level_prop->last_modified)
	bc_ent_int64(savefile, "LastModified", level_prop->last_modified);
    if (level_prop->last_backup)
	bc_ent_int64(savefile, "LastBackup", level_prop->last_backup);

    bc_compound(savefile, "Spawn");
    bc_ent_int16(savefile, "X", level_prop->spawn.x/32);
    bc_ent_int16(savefile, "Y", level_prop->spawn.y/32);
    bc_ent_int16(savefile, "Z", level_prop->spawn.z/32);
    bc_ent_int8(savefile, "H", level_prop->spawn.h);
    bc_ent_int8(savefile, "P", level_prop->spawn.v);
    bc_end(savefile);

    bc_compound(savefile, "Metadata");
    bc_compound(savefile, "CPE");

    // Written by CC
    if (level_prop->click_distance >= 0) {
	bc_compound(savefile, "ClickDistance");
	bc_ent_int16(savefile, "Distance", level_prop->click_distance);
	bc_end(savefile);
    }

    // Written by CC
    bc_compound(savefile, "EnvWeatherType");
    bc_ent_int8(savefile, "WeatherType", level_prop->weather);
    bc_end(savefile);

    // Written by CC
    bc_compound(savefile, "EnvColors");
    bc_colour(savefile, "Sky", level_prop->sky_colour);
    bc_colour(savefile, "Cloud", level_prop->cloud_colour);
    bc_colour(savefile, "Fog", level_prop->fog_colour);
    bc_colour(savefile, "Ambient", level_prop->ambient_colour);
    bc_colour(savefile, "Sunlight", level_prop->sunlight_colour);
    // Not written by CC {
    bc_colour(savefile, "Skybox", level_prop->skybox_colour);
    // }
    bc_end(savefile);

    // Written by CC
    bc_compound(savefile, "EnvMapAppearance");
    bc_ent_int8(savefile, "SideBlock", level_prop->side_block);
    bc_ent_int8(savefile, "EdgeBlock", level_prop->edge_block);
    bc_ent_int16(savefile, "SideLevel", level_prop->side_level);
    nbtstr_t b = level_prop->texname;
    bc_ent_string(savefile, "TextureURL", b.c);
    bc_end(savefile);

    // Not written by CC
    bc_compound(savefile, "SetSpawn");
    bc_ent_int(savefile, "X", level_prop->spawn.x);
    bc_ent_int(savefile, "Y", level_prop->spawn.y);
    bc_ent_int(savefile, "Z", level_prop->spawn.z);
    bc_ent_int(savefile, "H", level_prop->spawn.h);
    bc_ent_int(savefile, "P", level_prop->spawn.v);
    bc_end(savefile);

    // Not written by CC
    bc_compound(savefile, "EnvMapAspect");
    bc_ent_int16(savefile, "SideOffset", level_prop->side_offset);
    bc_ent_int16(savefile, "CloudsHeight", level_prop->clouds_height);

    bc_ent_int(savefile, "MapProperty0", level_prop->side_block);
    bc_ent_int(savefile, "MapProperty1", level_prop->edge_block);
    bc_ent_int(savefile, "MapProperty2", level_prop->side_level);
    bc_ent_int(savefile, "MapProperty3", level_prop->clouds_height);
    bc_ent_int(savefile, "MapProperty4", level_prop->max_fog);
    bc_ent_int(savefile, "MapProperty5", level_prop->clouds_speed);
    bc_ent_int(savefile, "MapProperty6", level_prop->weather_speed);
    bc_ent_int(savefile, "MapProperty7", level_prop->weather_fade);
    bc_ent_int(savefile, "MapProperty8", level_prop->exp_fog);
    bc_ent_int(savefile, "MapProperty9", level_prop->side_offset);
    bc_ent_int(savefile, "MapProperty10", level_prop->skybox_hor_speed);
    bc_ent_int(savefile, "MapProperty11", level_prop->skybox_ver_speed);
    bc_end(savefile);

    // Not written by CC
    if (level_prop->hacks_flags) {
	bc_compound(savefile, "HackControl");
	int v = level_prop->hacks_flags;
	bc_ent_int(savefile, "Flying", !(v&1));
	bc_ent_int(savefile, "NoClip", !(v&2));
	bc_ent_int(savefile, "Speed", !(v&4));
	bc_ent_int(savefile, "SpawnControl", !(v&8));
	bc_ent_int(savefile, "ThirdPersonView", !(v&0x10));
	if (!(v&0x20))
	    bc_ent_int(savefile, "JumpHeight", -1);
	else
	    bc_ent_int(savefile, "JumpHeight", level_prop->hacks_jump);
	bc_end(savefile);
    }

    // Written by CC
    int bdopen = 0;
    for(int i=1; i<BLOCKMAX; i++) {
	int id = (i+256)%BLOCKMAX;
	if (level_prop->blockdef[id].defined) {
	    if (!bdopen) {
		bc_compound(savefile, "BlockDefinitions");
		bdopen = 1;
	    }
	    save_block_def(savefile, id, (void*)(&level_prop->blockdef[id]));
	}
    }
    if (bdopen)
	bc_end(savefile);

    // Not written by CC
    bdopen = 0;
    for(int i=1; i<BLOCKMAX; i++)
	if (level_prop->blockdef[i].defined)
	{
	    int ord = level_prop->blockdef[i].inventory_order;
	    if (ord < 0 || ord == (block_t)-1) ord = i;
	    if (ord != i) {
		if (!bdopen) {
		    bc_compound(savefile, "InventoryOrder");
		    bdopen = 1;
		}

		char uniquename[64]; // Limitation of MCGalaxy loader.
		sprintf(uniquename, "InventoryOrder%04x", i);

		bc_compound(savefile, uniquename);
		bc_ent_int(savefile, "Block", i);
		bc_ent_int(savefile, "Order", ord);
		bc_end(savefile);
	    }
	}

    if (bdopen)
	bc_end(savefile);

    bc_compound(savefile, "MCCHost");
    bc_ent_int(savefile, "AllowChange", level_prop->allowchange);
    bc_ent_int(savefile, "ReadOnly", level_prop->readonly);
    bc_end(savefile);

    /* TODO -- server level ?
        bc_compound(ofd, "SetTextHotKey", seqid);
        bc_ent_string(ofd, "label", pkt+1, 64);
        bc_ent_string(ofd, "action", pkt+65, 64);
        bc_ent_int(ofd, "keycode", IntBE32(pkt+129));
        bc_ent_int(ofd, "keymods", (pkt[133] & 0xFF));
        bc_end(ofd);
    */

    /* TODO
	Zones ...
    */

    bc_end(savefile);
    bc_end(savefile);

    if (level_blocks) {
	int flg = 0, flg2 = 0;

	// Save as much as we can, lost blocks would be at the top of the Y axis.
	int saveable_blocks = level_prop->total_blocks;
	if (level_prop->total_blocks > 0x7FFFFFFF)
	    saveable_blocks = 0x7FFFFFFF;

	// Written by CC
	bc_ent_bytes_header(savefile, "BlockArray", saveable_blocks);

	for (intptr_t i = 0; i<saveable_blocks; i++) {
	    int b = level_blocks[i];
	    // if (b>=BLOCKMAX) b = BLOCKMAX-1;
	    if (b>=CPELIMIT) {flg2 = 1; b &= 0xFF; }
	    gzputc(savefile, b & 0xFF);
	    flg |= (b>0xFF);
	}

	// Written by CC
	if (flg) {
	    // BlockArray2 can only have 0..767 so we need BlockArray3
	    bc_ent_bytes_header(savefile, "BlockArray2", saveable_blocks);

	    for (intptr_t i = 0; i<saveable_blocks; i++) {
		int b = level_blocks[i];
		// if (b>=BLOCKMAX) b = BLOCKMAX-1;
		if (b>=CPELIMIT) b &= 0xFF;
		gzputc(savefile, b>>8);
	    }
	}

	// Not written by CC
	if (flg2) {
	    // What format should I use for this?
	    // Currently it's a corrected high byte but this means BA2 makes
	    // a random block in 0..767. This byte is only set if BA2 is wrong.
	    bc_ent_bytes_header(savefile, "BlockArray3", saveable_blocks);

	    for (intptr_t i = 0; i<saveable_blocks; i++) {
		int b = level_blocks[i];
		// if (b>=BLOCKMAX) b=BLOCKMAX-1;
		if (b>=CPELIMIT)
		    gzputc(savefile, b>>8);
		else
		    gzputc(savefile, 0);
	    }

	    // Another option is to discard BA2 completely, put the 255
	    // fallback into BA1 and have BA3 and BA4 with the actual value.
	}
    }

    bc_end(savefile);

    int rv = gzclose(savefile);
    if (rv) {
	if (background)
	    printlog("gzclose('%s') failed, error %d\n", fn, rv);
	else
	    printf_chat("&WSave failed error Z%d", rv);
    }

    if (rv) return -1;
    return 0;
}

LOCAL void
bc_string(gzFile ofd, char * str)
{
    int len = strlen(str);
    gzputc(ofd, len>>8);
    gzputc(ofd, len&0xFF);
    gzwrite(ofd, str, len);
}

LOCAL void
bc_ent_label(gzFile ofd, char * name)
{
    bc_string(ofd, name);
}

LOCAL void
bc_ent_string(gzFile ofd, char * name, char * str)
{
    char buf[NB_SLEN*4];
    convert_to_utf8(buf, sizeof(buf), str);
    gzputc(ofd, NBT_STR);
    bc_ent_label(ofd, name);
    bc_string(ofd, buf);
}

LOCAL void
bc_ent_bytes_header(gzFile ofd, char * name, uint32_t len)
{
    gzputc(ofd, NBT_I8ARRAY);
    bc_ent_label(ofd, name);
    gzputc(ofd, (len>>24) & 0xFF);
    gzputc(ofd, (len>>16) & 0xFF);
    gzputc(ofd, (len>>8)  & 0xFF);
    gzputc(ofd,  len      & 0xFF);
}

LOCAL void
bc_ent_bytes(gzFile ofd, char * name, char * bstr, uint32_t len)
{
    bc_ent_bytes_header(ofd, name, len);
    gzwrite(ofd, bstr, len);
}

LOCAL void
bc_ent_int(gzFile ofd, char * name, int val)
{
    if (val >= -128 && val <= 127) {
	gzputc(ofd, NBT_I8);
	bc_ent_label(ofd, name);
	gzputc(ofd, val & 0xFF);
	return;
    }

    if (val >= -32768 && val <= 32767) {
	gzputc(ofd, NBT_I16);
	bc_ent_label(ofd, name);
	gzputc(ofd, (val>>8));
	gzputc(ofd, val&0xFF);
	return;
    }

    gzputc(ofd, NBT_I32);
    bc_ent_label(ofd, name);
    gzputc(ofd, (val>>24) & 0xFF);
    gzputc(ofd, (val>>16) & 0xFF);
    gzputc(ofd, (val>>8)  & 0xFF);
    gzputc(ofd,  val      & 0xFF);
}

LOCAL void
bc_ent_float(gzFile ofd, char * name, double fval)
{
    union { int32_t i32; float f32; } bad;
    bad.f32 = fval;
    int val = bad.i32;
    gzputc(ofd, NBT_F32);
    bc_ent_label(ofd, name);
    gzputc(ofd, (val>>24) & 0xFF);
    gzputc(ofd, (val>>16) & 0xFF);
    gzputc(ofd, (val>>8)  & 0xFF);
    gzputc(ofd,  val      & 0xFF);
}

LOCAL void
bc_ent_int64(gzFile ofd, char * name, int64_t val)
{
    gzputc(ofd, NBT_I64);
    bc_ent_label(ofd, name);
    gzputc(ofd, (val>>56));
    gzputc(ofd, (val>>48));
    gzputc(ofd, (val>>40));
    gzputc(ofd, (val>>32));
    gzputc(ofd, (val>>24));
    gzputc(ofd, (val>>16));
    gzputc(ofd, (val>>8));
    gzputc(ofd, val&0xFF);
}

LOCAL void
bc_ent_int16(gzFile ofd, char * name, int val)
{
    gzputc(ofd, NBT_I16);
    bc_ent_label(ofd, name);
    gzputc(ofd, (val>>8));
    gzputc(ofd, val&0xFF);
}

LOCAL void
bc_ent_int8(gzFile ofd, char * name, int val)
{
    gzputc(ofd, NBT_I8);
    bc_ent_label(ofd, name);
    gzputc(ofd, val & 0xFF);
}

LOCAL void
bc_compound(gzFile ofd, char * name)
{
    gzputc(ofd, NBT_COMPOUND);
    bc_ent_label(ofd, name);
}

LOCAL void
bc_end(gzFile ofd)
{
    enum NbtTagType v = NBT_END;
    gzputc(ofd, v);
}

void
bc_colour(gzFile ofd, char * name, int colour)
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
save_block_def(gzFile ofd, int idno, blockdef_t * blkdef)
{
    char buf[64];

    // Written by CC
    sprintf(buf, "Block%04x", idno);

    bc_compound(ofd, buf);
    bc_ent_int8(ofd, "ID", idno);
    bc_ent_int16(ofd, "ID2", idno);

    bc_ent_string(ofd, "Name", blkdef->name.c);
    bc_ent_int8(ofd, "CollideType", blkdef->collide);
    {
//	double log2 = 0.693147180559945f;
//	double spl2 = (UB(pkt[p++]) - 128) / 64.0;
//	double spd = exp(log2 * spl2);
	bc_ent_float(ofd, "Speed", blkdef->speed/1000.0);
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

	/* ... Huh?
            if (box[1] != 0 || box[4] == 0)
                bc_ent_int8(ofd, "Shape", 1);
            else
                bc_ent_int8(ofd, "Shape", (box[4] & 0xFF));
	*/
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

    // Not written by CC ...
    bc_ent_int(ofd, "Fallback", blkdef->fallback);

    if (blkdef->inventory_order != (block_t)-1)
	bc_ent_int(ofd, "InventoryOrder", blkdef->inventory_order);

    bc_ent_int(ofd, "FireFlag", blkdef->fire_flag);
    bc_ent_int(ofd, "DoorFlag", blkdef->door_flag);
    bc_ent_int(ofd, "MblockFlag", blkdef->mblock_flag);
    bc_ent_int(ofd, "PortalFlag", blkdef->portal_flag);
    bc_ent_int(ofd, "LavakillsFlag", blkdef->lavakills_flag);
    bc_ent_int(ofd, "WaterkillsFlag", blkdef->waterkills_flag);
    bc_ent_int(ofd, "TdoorFlag", blkdef->tdoor_flag);
    bc_ent_int(ofd, "RailsFlag", blkdef->rails_flag);
    bc_ent_int(ofd, "OpblockFlag", blkdef->opblock_flag);

    bc_ent_int(ofd, "StackBlock", blkdef->stack_block);
    bc_ent_int(ofd, "OdoorBlock", blkdef->odoor_block);
    bc_ent_int(ofd, "GrassBlock", blkdef->grass_block);
    bc_ent_int(ofd, "DirtBlock", blkdef->dirt_block);
    bc_end(ofd);
}
