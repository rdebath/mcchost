
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "nbtload.h"

/* NOTE: Do we want to be able to load other file formats?
 */

enum NbtTagType {
    NBT_END, NBT_I8, NBT_I16, NBT_I32, NBT_I64, NBT_F32,
    NBT_F64, NBT_I8ARRAY, NBT_STR, NBT_LIST, NBT_COMPOUND,
    NBT_LABEL = -1
};

static int NbtLen[] = { 0, 1, 2, 4, 8, 4, 8, 0, 0, 0, 0, 0, 0 };

static char *NbtName[] = {
    "NBT_END", "NBT_I8", "NBT_I16", "NBT_I32", "NBT_I64", "NBT_F32",
    "NBT_F64", "NBT_I8ARRAY", "NBT_STR", "NBT_LIST", "NBT_COMPOUND"
};

static char last_lbl[256];
static char last_sect[256];
static int indent = 0;
static int current_block = -1;
static int inventory_block = -1;

// Load into here, copy to the real one later.
static char new_level[256];

int
load_map_from_file(char * filename, char * levelname)
{
    FILE * ifd;
    char * cmdbuf, *s, *d;

    d = cmdbuf = calloc(1, strlen(filename)*4 + 32);

    d += sprintf(cmdbuf, "%s", "gzip -S .cw -cdf '");
    for (s=filename; *s; s++) {
	if (*s == '\'') {
	    *d++ = '\''; *d++ = '\\'; *d++ = '\''; *d++ = '\'';
	} else {
	    *d++ = *s;
	}
    }
    *d++ = '\''; *d = 0;
    if ((ifd = popen(cmdbuf, "r")) == 0) {
	perror(filename);
	return -1;
    }
    snprintf(new_level, sizeof(new_level), "%s", levelname);

    load_cwfile(ifd);
    pclose(ifd);
    free(cmdbuf);
    return 0;
}

LOCAL void
load_cwfile(FILE * ifd)
{
    int ClassicWorld_found = 0;
    int ch = fgetc(ifd);
    if (ch == NBT_COMPOUND) {
	*last_lbl = *last_sect = 0;
	read_element(ifd, NBT_LABEL);
	ClassicWorld_found = !strcmp("ClassicWorld", last_lbl);
	if (!ClassicWorld_found) {
	    fprintf(stderr, "File format incorrect.\n");
	    return;
	}
	fprintf(stderr, "Loading ClassicWorld map: ");
	open_level_files(new_level, 1);
	read_element(ifd, ch);
    } else {
	fprintf(stderr, "File format incorrect.\n");
	return;
    }

    if (ClassicWorld_found || (level_prop->cells_x>0 && level_prop->cells_y>0 && level_prop->cells_z>0))
	fprintf(stderr, "Load done.\n");
}

LOCAL int
read_element(FILE * ifd, int etype)
{
    if (etype == NBT_END) {
	// EOF
    } else if (etype == NBT_I8ARRAY) {
	int i, len = 0, ch;

	/* NB: Only lengths 0 ..0x7FFFFFFF are valid. */
	for(i=0; i<4; i++)
	    len = (len<<8) + (ch = fgetc(ifd));

	if (strcmp(last_lbl, "BlockArray") == 0 && len>0)
	    return read_blockarray(ifd, len);

	if (strcmp(last_lbl, "BlockArray2") == 0 && len>0)
	    return read_blockarray2(ifd, len);

	if (strcmp(last_lbl, "BlockArray3") == 0 && len>0)
	    return read_blockarray3(ifd, len);

	if (len < 0 || len > 256) {
	    for(i=0; i<len; i++) {
		if ((ch = fgetc(ifd)) == EOF) return 0;
	    }
	} else {
	    uint8_t bin_buf[256];
	    for(i=0; i<len; i++) {
		if ((ch = fgetc(ifd)) == EOF) return 0;
		if (i<sizeof(bin_buf)) bin_buf[i] = ch;
	    }

	    if (len <= sizeof(bin_buf))
		change_bin_value(last_sect, last_lbl, bin_buf, len);
	}

    } else if (etype == NBT_STR || etype == NBT_LABEL) {
	int i, len = 0, ch;
	char str_buf[4096];
	str_buf[0] = 0;

	len = (len<<8) + (ch = fgetc(ifd));
	len = (len<<8) + (ch = fgetc(ifd));
	if (ch == EOF) return 0;

	if (etype == NBT_STR) {
	    str_buf[len < sizeof(str_buf)-1?len:0] = 0;
	} else
	    last_lbl[len < sizeof(last_lbl)-1?len:0] = 0;
	for(i=0; i<len; i++) {
	    if ((ch = fgetc(ifd)) == EOF) return 0;
	    if (etype == NBT_STR)
	    {
		if (i<sizeof(str_buf)-1) { str_buf[i] = ch; str_buf[i+1] = 0; }
	    } else {
		if (len < sizeof(last_lbl)-1) {
		    if ((ch >= 'a' && ch <= 'z') ||
			(ch >= 'A' && ch <= 'Z') ||
			(ch >= '0' && ch <= '9' && i != 0) ||
			(ch == '_' || ch == '(' || ch == ')'))
			last_lbl[i] = ch;
		    else
			last_lbl[i] = '-';
		}
	    }
	}

	if (etype == NBT_STR) {
	    change_str_value(last_sect, last_lbl, str_buf);
	}

    } else if (etype == NBT_COMPOUND) {
	if (indent > 0)
	    strcpy(last_sect, last_lbl);
	indent++;
	for(;;) {
	    etype = getc(ifd);
	    if (etype == NBT_END) {
		indent--;
		*last_sect = 0;
		return 1;
	    }

	    if (!read_element(ifd, NBT_LABEL)) return 0;
	    if (!read_element(ifd, etype)) return 0;
	}

    } else if (etype == NBT_LIST) {
	int etype = fgetc(ifd);
	if (etype == EOF) return 0;

	int len = 0, i, ch;
	for(i = 0; i<4; i++)
	    len = (len<<8) + ((ch=fgetc(ifd)) & 0xFF);
	if (ch == EOF) return 0;

	indent++;
	for(i=0; i<len; i++) {
	    if (!read_element(ifd, etype)) return 0;
	}

	*last_sect = 0;

	indent--;
	return 1;

    } else if ( etype < NBT_END || etype > NBT_COMPOUND) {
	return 0;

    } else if (NbtLen[etype] > 0) {
	long long V = 0;
	int i;
	for(i=0; i<NbtLen[etype]; i++) {
	    V = (V<<8) + (fgetc(ifd) & 0xFF);
	}
	switch(etype) {
	case NBT_I8: V = (int8_t) V; break;
	case NBT_I16: V = (int16_t) V; break;
	case NBT_I32: V = (int32_t) V; break;
	}
	if (etype <= NBT_I64 && etype >= NBT_I8) {
	    ;
	} else if (etype == NBT_F32) {
	    union { int32_t i32; float f32; } bad;
	    bad.i32 = V;
	    V = bad.f32 * 1000;
	} else {
	    ;
	}

	change_int_value(last_sect, last_lbl, V);

    } else {
	fprintf(stderr, "# UNIMPL: %s\n", NbtName[etype]);
	return 0;
    }
    return 1;
}

LOCAL int
read_blockarray(FILE *ifd, int len)
{
    level_prop->total_blocks = (int64_t)level_prop->cells_x * level_prop->cells_y * level_prop->cells_z;
    if (open_blocks(new_level) < 0)
	return 0;

    map_len_t test_map;
    test_map.magic_no = MAP_MAGIC;
    test_map.cells_x = level_prop->cells_x;
    test_map.cells_y = level_prop->cells_y;
    test_map.cells_z = level_prop->cells_z;
    memcpy((void*)(level_blocks+level_prop->total_blocks),
	    &test_map, sizeof(map_len_t));

    for(int i=0; i<len; i++) {
	int ch;
	if ((ch = getc(ifd)) == EOF) return 0;
	level_blocks[i] = (level_blocks[i] & 0xFF00) + (ch&0xFF);
    }
    return 1;
}

LOCAL int
read_blockarray2(FILE *ifd, int len)
{

    if (level_blocks == 0 || level_prop->total_blocks < len) {
	fprintf(stderr, "Incorrect BlockArray2 found\n");
	return 0;
    }

    for(int i=0; i<len; i++) {
	int ch;
	if ((ch = getc(ifd)) == EOF) return 0;
	level_blocks[i] = (level_blocks[i] & 0x00FF) + (ch<<8);
    }
    return 1;
}

LOCAL int
read_blockarray3(FILE *ifd, int len)
{

    if (level_blocks == 0 || level_prop->total_blocks < len) {
	fprintf(stderr, "Incorrect BlockArray3 found\n");
	return 0;
    }

    for(int i=0; i<len; i++) {
	int ch;
	if ((ch = getc(ifd)) == EOF) return 0;
	if (ch)
	    level_blocks[i] = (level_blocks[i] & 0x00FF) + (ch<<8);
    }
    return 1;
}

LOCAL void
set_colour(volatile int *colour, int by, int value)
{
    if (value < 0)
	*colour = -1;
    else {
	*colour &= 0xFFFFFF;
	*colour = *colour & ~(0xFF << (8*(2-by)));
	*colour = *colour | ((value & 0xFF) << (8*(2-by)));
    }
}

LOCAL void
change_int_value(char * section, char * item, long long value)
{
    if (*section == 0 ||
	    strcmp(section, "ClassicWorld") == 0 ||
	    strcmp(section, "MapSize") == 0) {
	int ok = 1;
	if (strcmp(item, "X") == 0) level_prop->cells_x = value;
	else if (strcmp(item, "Y") == 0) level_prop->cells_y = value;
	else if (strcmp(item, "Z") == 0) level_prop->cells_z = value;
	else ok = 0;
	if (ok) {
	    level_prop->total_blocks = (int64_t)level_prop->cells_x * level_prop->cells_y * level_prop->cells_z;
	    if (level_prop->total_blocks)
		init_map_from_size((xyz_t){level_prop->cells_x, level_prop->cells_y, level_prop->cells_z});
	}

    } else if (strcmp(section, "Spawn") == 0) {
	// Add precise spawn?
	if (strcmp(item, "X") == 0) level_prop->spawn.x = value*32+16;
	if (strcmp(item, "Y") == 0) level_prop->spawn.y = value*32+16;
	if (strcmp(item, "Z") == 0) level_prop->spawn.z = value*32+16;
	if (strcmp(item, "H") == 0) level_prop->spawn.h = value*360/256;
	if (strcmp(item, "P") == 0) level_prop->spawn.v = value*360/256;
    } else if (strcmp(section, "Sky") == 0) {
	int c = -1;
	if (strcmp(item, "R") == 0) c = 0;
	if (strcmp(item, "G") == 0) c = 1;
	if (strcmp(item, "B") == 0) c = 2;
	if (c>=0) set_colour(&level_prop->sky_colour, c, value);
    } else if (strcmp(section, "Cloud") == 0) {
	int c = -1;
	if (strcmp(item, "R") == 0) c = 0;
	if (strcmp(item, "G") == 0) c = 1;
	if (strcmp(item, "B") == 0) c = 2;
	if (c>=0) set_colour(&level_prop->cloud_colour, c, value);
    } else if (strcmp(section, "Fog") == 0) {
	int c = -1;
	if (strcmp(item, "R") == 0) c = 0;
	if (strcmp(item, "G") == 0) c = 1;
	if (strcmp(item, "B") == 0) c = 2;
	if (c>=0) set_colour(&level_prop->fog_colour, c, value);
    } else if (strcmp(section, "Ambient") == 0) {
	int c = -1;
	if (strcmp(item, "R") == 0) c = 0;
	if (strcmp(item, "G") == 0) c = 1;
	if (strcmp(item, "B") == 0) c = 2;
	if (c>=0) set_colour(&level_prop->ambient_colour, c, value);
    } else if (strcmp(section, "Sunlight") == 0) {
	int c = -1;
	if (strcmp(item, "R") == 0) c = 0;
	if (strcmp(item, "G") == 0) c = 1;
	if (strcmp(item, "B") == 0) c = 2;
	if (c>=0) set_colour(&level_prop->sunlight_colour, c, value);
    } else if (strcmp(section, "Skybox") == 0) {
	int c = -1;
	if (strcmp(item, "R") == 0) c = 0;
	if (strcmp(item, "G") == 0) c = 1;
	if (strcmp(item, "B") == 0) c = 2;
	if (c>=0) set_colour(&level_prop->skybox_colour, c, value);
    } else if (strcmp(section, "EnvMapAppearance") == 0) {
	if (strcmp(item, "SideBlock") == 0) level_prop->side_block = value;
	if (strcmp(item, "EdgeBlock") == 0) level_prop->edge_block = value;
	if (strcmp(item, "SideLevel") == 0) level_prop->side_level = value;
	if (strcmp(item, "SideOffset") == 0) level_prop->side_offset = value;
	// String: if (strcmp(item, "TextureURL") == 0)
    } else if (strcmp(section, "EnvMapAspect") == 0) {
	if (strcmp(item, "SideOffset") == 0) level_prop->side_offset = value;
	if (strcmp(item, "CloudsHeight") == 0) level_prop->clouds_height = value;

	if (strcmp(item, "MapProperty0") == 0) level_prop->side_block = value;
	if (strcmp(item, "MapProperty1") == 0) level_prop->edge_block = value;
	if (strcmp(item, "MapProperty2") == 0) level_prop->side_level = value;
	if (strcmp(item, "MapProperty3") == 0) level_prop->clouds_height = value;
	if (strcmp(item, "MapProperty4") == 0) level_prop->max_fog = value;
	if (strcmp(item, "MapProperty5") == 0) level_prop->clouds_speed = value;
	if (strcmp(item, "MapProperty6") == 0) level_prop->weather_speed = value;
	if (strcmp(item, "MapProperty7") == 0) level_prop->weather_fade = value;
	if (strcmp(item, "MapProperty8") == 0) level_prop->exp_fog = value;
	if (strcmp(item, "MapProperty9") == 0) level_prop->side_offset = value;
	if (strcmp(item, "MapProperty10") == 0) level_prop->skybox_hor_speed = value;
	if (strcmp(item, "MapProperty11") == 0) level_prop->skybox_ver_speed = value;

    } else if (strcmp(section, "ClickDistance") == 0) {
	if (strcmp(item, "Distance") == 0) level_prop->click_distance = value;
    } else if (strcmp(section, "EnvWeatherType") == 0) {
	if (strcmp(item, "WeatherType") == 0) level_prop->weather = value;
    } else if (strncasecmp(section, "InventoryOrder", 14) == 0) {
	if (strcasecmp(item, "Block") == 0) inventory_block = value;
	if (strcasecmp(item, "Order") == 0) {
	    int inventory_order = value;

	    if (inventory_order >= 0 && inventory_block > 0 && inventory_block < BLOCKMAX) {
		level_prop->blockdef[inventory_block].inventory_order = inventory_order;
	    }
	    inventory_order = -1;
	    inventory_block = -1;
	}

    } else if (strncmp(section, "Block", 5) == 0) {
	if (strcmp(item, "ID2") == 0) {
	    current_block = value;
	    if (current_block >= 0 && current_block<66 && !level_prop->blockdef[current_block].defined) {
		level_prop->blockdef[current_block] = default_blocks[current_block];
	    }

	    level_prop->blockdef[current_block].defined = 1;
	    level_prop->blockdef[current_block].inventory_order = -1;
	    level_prop->blockdef[current_block].fallback = -1;
	} else if (current_block < 0 || current_block >= BLOCKMAX) {
	    // Skip -- no ID2.
	} else if (strcmp(item, "CollideType") == 0) {
	    level_prop->blockdef[current_block].collide = value;
	} else if (strcmp(item, "Speed") == 0) {
	    level_prop->blockdef[current_block].speed = value/1000.0;
	} else if (strcmp(item, "TransmitsLight") == 0) {
	    level_prop->blockdef[current_block].transmits_light = value;
	} else if (strcmp(item, "WalkSound") == 0) {
	    level_prop->blockdef[current_block].walksound = value;
	} else if (strcmp(item, "FullBright") == 0) {
	    level_prop->blockdef[current_block].fullbright = value;
	} else if (strcmp(item, "Shape") == 0) {
	    level_prop->blockdef[current_block].shape = value;
	} else if (strcmp(item, "BlockDraw") == 0) {
	    level_prop->blockdef[current_block].draw = value;

	} else if (strcmp(item, "Fallback") == 0) {
	    level_prop->blockdef[current_block].fallback = value;
	} else if (strcmp(item, "InventoryOrder") == 0) {
	    level_prop->blockdef[current_block].inventory_order = value;
	} else if (strcmp(item, "FireFlag") == 0) {
	    level_prop->blockdef[current_block].fire_flag = value;
	} else if (strcmp(item, "DoorFlag") == 0) {
	    level_prop->blockdef[current_block].door_flag = value;
	} else if (strcmp(item, "MblockFlag") == 0) {
	    level_prop->blockdef[current_block].mblock_flag = value;
	} else if (strcmp(item, "PortalFlag") == 0) {
	    level_prop->blockdef[current_block].portal_flag = value;
	} else if (strcmp(item, "LavakillsFlag") == 0) {
	    level_prop->blockdef[current_block].lavakills_flag = value;
	} else if (strcmp(item, "WaterkillsFlag") == 0) {
	    level_prop->blockdef[current_block].waterkills_flag = value;
	} else if (strcmp(item, "TdoorFlag") == 0) {
	    level_prop->blockdef[current_block].tdoor_flag = value;
	} else if (strcmp(item, "RailsFlag") == 0) {
	    level_prop->blockdef[current_block].rails_flag = value;
	} else if (strcmp(item, "OpblockFlag") == 0) {
	    level_prop->blockdef[current_block].opblock_flag = value;
	} else if (strcmp(item, "StackBlock") == 0) {
	    level_prop->blockdef[current_block].stack_block = value;
	} else if (strcmp(item, "OdoorBlock") == 0) {
	    level_prop->blockdef[current_block].odoor_block = value;
	} else if (strcmp(item, "GrassBlock") == 0) {
	    level_prop->blockdef[current_block].grass_block = value;
	} else if (strcmp(item, "DirtBlock") == 0) {
	    level_prop->blockdef[current_block].dirt_block = value;
	}

    }
}

LOCAL void
change_bin_value(char * section, char * item, uint8_t * value, int len)
{
    if (strncmp(section, "Block", 5) == 0 &&
	current_block >= 0 && current_block < BLOCKMAX) {

	if (strcmp(item, "Textures") == 0) {
	    // CW File: Top, Bottom, Left, Right, Front, Back
	    // Packet:  Top, Left, Right, Front, Back, Bottom

	    if (len < 6) return;

	    level_prop->blockdef[current_block].textures[0] = value[0];
	    level_prop->blockdef[current_block].textures[1] = value[2];
	    level_prop->blockdef[current_block].textures[2] = value[3];
	    level_prop->blockdef[current_block].textures[3] = value[4];
	    level_prop->blockdef[current_block].textures[4] = value[5];
	    level_prop->blockdef[current_block].textures[5] = value[1];

	    if (len < 12) return;

	    level_prop->blockdef[current_block].textures[0] += value[6+0] * 256;
	    level_prop->blockdef[current_block].textures[1] += value[6+2] * 256;
	    level_prop->blockdef[current_block].textures[2] += value[6+3] * 256;
	    level_prop->blockdef[current_block].textures[3] += value[6+4] * 256;
	    level_prop->blockdef[current_block].textures[4] += value[6+5] * 256;
	    level_prop->blockdef[current_block].textures[5] += value[6+1] * 256;

	} else if (strcmp(item, "Fog") == 0) {
	    int i;
	    if (len < 4) return;
	    for(i=0; i<4; i++) {
		level_prop->blockdef[current_block].fog[i] = value[i];
	    }

	} else if (strcmp(item, "Coords") == 0) {
	    int i;
	    if (len < 6) return;
	    for(i=0; i<6; i++) {
		level_prop->blockdef[current_block].coords[i] = value[i];
	    }

	}
    }
}

LOCAL void
change_str_value(char * section, char * item, char * value)
{
    if (strcmp(section, "EnvMapAppearance") == 0) {
	if (strcmp(item, "TextureURL") == 0) {
	    cpy_nstr(level_prop->texname.c, value, MB_STRLEN);
	}
    } else if (strncmp(section, "Block", 5) == 0) {
	if (strcmp(item, "Name") == 0) {
	    if (current_block >= 0 && current_block < BLOCKMAX) {
		cpy_nstr(level_prop->blockdef[current_block].name.c, value, MB_STRLEN);
	    }
	}

#if 0 // Bots?
    } else if (strcmp(section, "Player") == 0) {
	if (strcmp(item, "name") == 0) {
	    if (curr_entity >= 0 && curr_entity < MAX_ENT) {
		strncpy(entities[curr_entity].name, value, sizeof(entities->name)-1);
	    }
	}
    } else if (strcmp(section, "ExtAddEntity2") == 0) {
	if (strcmp(item, "ingamename") == 0) {
	    if (curr_entity >= 0 && curr_entity < MAX_ENT) {
		strncpy(entities[curr_entity].name, value, sizeof(entities->name)-1);
	    }
	} else
	if (strcmp(item, "skinname") == 0) {
	    if (curr_entity >= 0 && curr_entity < MAX_ENT) {
		strncpy(entities[curr_entity].skin, value, sizeof(entities->skin)-1);
	    }
	}
    } else if (strcmp(section, "ChangeModel") == 0) {
	if (strcmp(item, "ModelName") == 0) {
	    if (curr_entity >= 0 && curr_entity < MAX_ENT) {
		strncpy(entities[curr_entity].model, value, sizeof(entities->model)-1);
	    }
	}
#endif

    }
}

LOCAL void
cpy_nstr(volatile char *buf, char *str, int len)
{
    volatile char *d = buf;
    char * s;
    for(s=str; s && *s; s++) {
	*d++ = *s;
	if (d == buf+len-1) break;
    }
    *d = 0;
}
