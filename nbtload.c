#include <math.h>
#include <zlib.h>
#include <sys/stat.h>

#include "nbtload.h"

// NOTE: Do we want to be able to load other file formats?

#if INTERFACE
enum NbtTagType {
    NBT_END, NBT_I8, NBT_I16, NBT_I32, NBT_I64, NBT_F32,
    NBT_F64, NBT_I8ARRAY, NBT_STR, NBT_LIST, NBT_COMPOUND,
    NBT_LABEL = -1
};
#endif

char *NbtName[] = {
    "NBT_END", "NBT_I8", "NBT_I16", "NBT_I32", "NBT_I64", "NBT_F32",
    "NBT_F64", "NBT_I8ARRAY", "NBT_STR", "NBT_LIST", "NBT_COMPOUND"
};

static int NbtLen[] = { 0, 1, 2, 4, 8, 4, 8, 0, 0, 0, 0, 0, 0 };

static char last_lbl[256];
static char last_sect[256];
static int indent = 0;
static int current_block = -1;
static int current_cuboid = -1;
static int inventory_block = -1;
static int permission_block = -1;
static int curr_x, curr_y, curr_z;

static char * loading_level_fname = 0;

#if INTERFACE
#ifdef EIGHTBITMAP
#define MCG_PHYSICS_0FFSET 0
#else
#define MCG_PHYSICS_0FFSET CPELIMIT	//768
#endif
#define IsMCGPhysics(bno) (bno>=Block_CPE && bno>MCG_PHYSICS_0FFSET && bno < MCG_PHYSICS_0FFSET+255)
#endif

// MCGalaxy physics block fallback and visual representations.
uint8_t mcg_physics[256] = {
  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
 64, 65,  0,  0,  0,  0, 39, 36, 36, 10, 46, 21, 22, 22, 22, 22,
  4,  0, 22, 21,  0, 22, 23, 24, 22, 26, 27, 28, 30, 31, 32, 33,
 34, 35, 36, 22, 20, 49, 45,  1,  4,  0,  9, 11,  4, 19,  5, 17,
 10, 49, 20,  1, 18, 12,  5, 25, 46, 44, 17, 49, 20,  1, 18, 12,
  5, 25, 36, 34,  0,  9, 11, 46, 44,  0,  9, 11,  8, 10, 22, 27,
 22,  8, 10, 28, 17, 49, 20,  1, 18, 12,  5, 25, 46, 44, 11,  9,
  0,  9, 11, 22,  0,  0,  9, 11,  0,  0,  0,  0,  0,  0,  0, 28,
 22, 21, 11,  0,  0,  0, 46, 46, 10, 10, 46, 20, 41, 42, 11,  9,
  0,  8, 10, 10,  8,  0, 22, 22,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0, 21, 10,  0,  0,  0,  0,  0, 22, 22, 42,  3,  2, 29,
 47,  0,  0,  0,  0,  0, 27, 46, 48, 24, 22, 36, 34,  8, 10, 21,
 29, 22, 10, 22, 22, 41, 19, 35, 21, 29, 49, 34, 16, 41,  0, 22
};

int
load_map_from_file(char * filename, char * level_fname, char * level_name, int backup_id)
{
    int quiet = 0;
    struct timeval start;
    gettimeofday(&start, 0);

    gzFile ifd;

    char ini_name[PATH_MAX];
    if (strlen(filename) > sizeof(ini_name)-4) {
	printlog("Level file \"%s\" name too long", filename);
	return -1;
    }
    {
	strcpy(ini_name, filename);
	char * p = strrchr(ini_name, '.');
	if (p && strcmp(p, ".cw") == 0)
	    *p = 0;
	strcat(ini_name, ".ini");
	// if (access(ini_name, R_OK) != 0) *ini_name = 0;
    }

    if ((ifd = gzopen(filename, "r")) == 0) {
	perror(filename);
	return -1;
    }

    loading_level_fname = level_fname;

    // 1 -> yes, 0 -> Nothing to load, -1 -> failed miserably.
    int cw_loaded = load_cwfile(ifd, level_fname, level_name, filename);

    loading_level_fname = 0;
    int rv = gzclose(ifd);
    if (rv) printlog("Load '%s' failed error (gzclose) Z%d", filename, rv);

    if (cw_loaded == 0) {
	// Looks like it might be a text file.
	quiet = 1;
	struct stat st = {0};
	(void)stat(filename, &st);
	if (!try_asciimode(level_fname, filename, ini_name, (uint64_t)st.st_mtime)) {
	    printlog("Level file \"%s\" NBT and INI load failed.", filename);
	    cw_loaded = -1;
	} else {
	    printlog("Level file \"%s\" loaded as INI.", filename);
	    cw_loaded = 1;
	}
    }

    if (cw_loaded != 1 || !level_prop)
	return -1;

    if (*ini_name) {
	if (access(ini_name, F_OK) == 0)
	    load_ini_file(mcc_level_ini_fields, ini_name, 1, 1);
	if (backup_id == 0) {
	    level_prop->last_loaded = time(0);
	    save_ini_file(mcc_level_ini_fields, ini_name);
	}
    }

    if (level_prop->time_created == 0) {
	// Hmm, no creation time, pickup the file modified time.
	struct stat st;
	char buf[PATH_MAX];
	// First look for backup No.1
	saprintf(buf, LEVEL_BACKUP_NAME, level_fname, 1);
	if (stat(buf, &st) >= 0)
	    level_prop->time_created = st.st_mtime;
	// Otherwise the cw file itself.
	else if (stat(filename, &st) >= 0)
	    level_prop->time_created = st.st_mtime;
	if (level_prop->time_created == 0)
	    level_prop->time_created = time(0);
    }

    if (level_prop->map_load_failure)
	level_prop->readonly = 1;

    if (!quiet) {
	struct timeval now;
	gettimeofday(&now, 0);
	printlog("Map load (%d,%d,%d) time %s",
	    level_prop->cells_x, level_prop->cells_y, level_prop->cells_z,
	    conv_ms_a((now.tv_sec-start.tv_sec)*1000.0+(now.tv_usec-start.tv_usec)/1000.0));
    }

    return 0;
}

LOCAL int
load_cwfile(gzFile ifd, char * level_fname, char * level_name, char * cw_filename)
{
    int ClassicWorld_found = 0;
    int ch = gzgetc(ifd);

    // File type peek
    if (ch == NBT_COMPOUND) { // Sigh, this is '\n'
	int ch1 = gzgetc(ifd);
	if (ch1 != 0) {
	    gzungetc(ch1, ifd);
	    gzungetc(ch, ifd);
	    ch = 0;
	} else {
#ifdef CHECK_FOR_SHORT_NBT_LABEL
	    // NB: This needs more than one char in gzungetc();
	    // That's usually fine; unless you have a very old zlib (1.2.3)
	    int ch2 = gzgetc(ifd);
	    gzungetc(ch2, ifd);
	    gzungetc(ch1, ifd);
	    if (ch2 < 4 || ch2 > 30) { // Reasonable lengths
		gzungetc(ch, ifd);
		ch = 0xFF;
	    }
#else
	    gzungetc(ch1, ifd);
#endif
	}
    } else
	gzungetc(ch, ifd);

    if (ch == NBT_COMPOUND) {
	*last_lbl = *last_sect = 0;
	if (!read_element(ifd, NBT_LABEL))
	    return -1;

	ClassicWorld_found = !strcmp("ClassicWorld", last_lbl);
	if (!ClassicWorld_found)
	    ClassicWorld_found = !strcmp("CPEPacketLog", last_lbl);

	if (!ClassicWorld_found) {
	    fprintf_logfile("Level \"%s\" incorrect NBT schema label \"%s\".", level_name, last_lbl);
	    return -1;
	}
	printlog("Loading %s map into \"%s\" from \"%s\" (%d)", last_lbl, level_fname, cw_filename, getpid());
	create_property_file(level_name, level_fname);
	init_map_null();
	if (!read_element(ifd, ch))
	    return -1;
    } else if (ch == EOF || (ch&0x80) != 0) {
	printlog("Level file \"%s\" unknown file format", cw_filename);
	return -1;
    } else
	return 0;

    if (ClassicWorld_found || (level_prop->cells_x>0 && level_prop->cells_y>0 && level_prop->cells_z>0))
	;
    else
	printlog("Unknown load failure for \"%s\"", level_name);

    return 1;
}

LOCAL int
read_element(gzFile ifd, int etype)
{
    if (etype == NBT_END) {
	// EOF
    } else if (etype == NBT_I8ARRAY) {
	int i, ch;
	uint32_t len = 0;

	/* NB: Only lengths 0 .. 0x7FFFFFFF are officially valid. */
	for(i=0; i<4; i++)
	    len = (len<<8) + (ch = gzgetc(ifd));

	if (len>0) {
	    if (strcmp(last_lbl, "BlockArray") == 0)
		return read_blockarray(ifd, len);

	    if (strcmp(last_lbl, "BlockArray2") == 0)
		return read_blockarray2(ifd, len);

	    if (strcmp(last_lbl, "BlockArray3") == 0)
		return read_blockarray3(ifd, len);

	    if (strcmp(last_lbl, "BlockArrayExt") == 0)
		return read_blockarray3(ifd, len);

	    if (strcmp(last_lbl, "BlockArrayPhysics") == 0)
		return read_blockarray_physics(ifd, len);
	}

	uint8_t bin_buf[256];
	if (len <= 0)
	    ;
	else if (len > sizeof(bin_buf)) {
	    for(i=0; i<len; i++) {
		if ((ch = gzgetc(ifd)) == EOF) goto read_failed;
	    }
	} else {
	    for(i=0; i<len; i++) {
		if ((ch = gzgetc(ifd)) == EOF) goto read_failed;
		if (i<sizeof(bin_buf)) bin_buf[i] = ch;
	    }

	    if (len <= sizeof(bin_buf))
		change_bin_value(last_sect, last_lbl, bin_buf, len);
	}

    } else if (etype == NBT_STR || etype == NBT_LABEL) {
	int i, len = 0, ch;
	char str_buf[4096];
	str_buf[0] = 0;

	len = (len<<8) + (ch = gzgetc(ifd));
	len = (len<<8) + (ch = gzgetc(ifd));
	if (ch == EOF) goto read_failed;

	if (etype == NBT_STR) {
	    str_buf[len < sizeof(str_buf)-1?len:0] = 0;
	} else
	    last_lbl[len < sizeof(last_lbl)-1?len:0] = 0;
	for(i=0; i<len; i++) {
	    if ((ch = gzgetc(ifd)) == EOF) goto read_failed;
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
	    int l = strlen(str_buf);
	    // This uses Java's Modified UTF-8 which is CESU-8 with a
	    // special overlong encoding of the NUL character (U+0000)
	    // as the two-byte sequence C0 80. CESU-8 is Unicode first
	    // encoded with UTF-16 then UTF-8 so that codes over 65535
	    // take 6 bytes not 4. This technicality does not impact
	    // this decoder as no CP437 character is over U+FFFF.
	    convert_to_cp437(str_buf, &l);
	    str_buf[l] = 0;
	    change_str_value(last_sect, last_lbl, str_buf);
	}

    } else if (etype == NBT_COMPOUND) {
	if (indent > 0)
	    strcpy(last_sect, last_lbl);
	indent++;
	for(;;) {
	    etype = gzgetc(ifd);
	    // NB: Allow an NBT_END to be clipped from the file.
	    if (etype == NBT_END || etype == EOF) {
		indent--;
		*last_sect = 0;
		return 1;
	    }

	    if (!read_element(ifd, NBT_LABEL)) return 0;
	    if (!read_element(ifd, etype)) return 0;
	}

    } else if (etype == NBT_LIST) {
	int etype = gzgetc(ifd);
	if (etype == EOF) goto read_failed;

	int len = 0, i, ch;
	for(i = 0; i<4; i++)
	    len = (len<<8) + ((ch=gzgetc(ifd)) & 0xFF);
	if (ch == EOF) goto read_failed;

	indent++;
	for(i=0; i<len; i++) {
	    if (!read_element(ifd, etype)) return 0;
	}

	*last_sect = 0;

	indent--;
	return 1;

    } else if ( etype < NBT_END || etype > NBT_COMPOUND) {
	printlog("Bad tag 0x%02x in level \"%s\"", etype, loading_level_fname);
	return 0;

    } else if (NbtLen[etype] > 0) {
	int64_t V = 0;
	int i;
	for(i=0; i<NbtLen[etype]; i++) {
	    V = (V<<8) + (gzgetc(ifd) & 0xFF);
	}
	switch(etype) {
	case NBT_I8: V = (int8_t) V; break;	// SIGNED!
	case NBT_I16: V = (int16_t) V; break;
	case NBT_I32: V = (int32_t) V; break;
	}
	if (etype <= NBT_I64 && etype >= NBT_I8) {
	    change_int_value(last_sect, last_lbl, V);
	} else if (etype == NBT_F32) {
	    union { int32_t i32; float f32; } bad;
	    bad.i32 = V;
	    change_float_value(last_sect, last_lbl, bad.f32);
	} else if (etype == NBT_F64) {
	    union { int64_t i64; double f64; } bad;
	    bad.i64 = V;
	    change_float_value(last_sect, last_lbl, bad.f64);
	} else {
	    ;
	}

    } else {
	printlog("Unimplemented tag %s", NbtName[etype]);
	return 0;
    }
    return 1;

read_failed:
    printlog("Short NBT file in tag %s in %s", NbtName[etype], loading_level_fname);
    return 0;
}

LOCAL int
read_blockarray(gzFile ifd, uint32_t len)
{
    // This must be before other block arrays to ensure that the shared
    // memory is the correct size for others and correctly wiped.
    level_prop->total_blocks = (int64_t)level_prop->cells_x * level_prop->cells_y * level_prop->cells_z;
    if (len!=level_prop->total_blocks) {
	printlog("Too many blocks %jd!=%jd (%d,%d,%d) for \"%s\"",
	    (intmax_t)len, (intmax_t)level_prop->total_blocks,
	    level_prop->cells_x, level_prop->cells_y, level_prop->cells_z,
	    loading_level_fname);
	level_prop->map_load_failure = 1;
    }

    if (open_blocks(loading_level_fname) < 0) {
	printlog("Unable to open block file for \"%s\"", loading_level_fname);
	return 0;
    }

    map_len_t test_map = {0};
    test_map.magic_no = TY_MAGIC;
    test_map.cells_x = level_prop->cells_x;
    test_map.cells_y = level_prop->cells_y;
    test_map.cells_z = level_prop->cells_z;
    test_map.blksz = sizeof(map_block_t);
    memcpy(level_blocks_len_t, &test_map, sizeof(map_len_t));

    int xbytes = 0;
    if (len > level_prop->total_blocks) { xbytes = len - level_prop->total_blocks; len = level_prop->total_blocks; }
    for(uint32_t i=0; i<len; i++) {
	int ch;
	if ((ch = gzgetc(ifd)) == EOF) {
	    printlog("Short block array on \"%s\"", loading_level_fname);
	    return 0;
	}
	level_blocks[i] = (ch&0xFF);
    }
    while(xbytes>0) {
	int ch = gzgetc(ifd);
	if (ch == EOF) return 0;
	xbytes--;
    }
    return 1;
}

LOCAL int
read_blockarray2(gzFile ifd, uint32_t len)
{
    if (level_blocks == 0) {
	printlog("Incorrect BlockArray2 found");
	return 0;
    }
    if (len!=level_prop->total_blocks) {
	printlog("Bad BlockArray2 %jd!=%jd (%d,%d,%d) for \"%s\"",
	    (intmax_t)len, (intmax_t)level_prop->total_blocks,
	    level_prop->cells_x, level_prop->cells_y, level_prop->cells_z,
	    loading_level_fname);
	level_prop->map_load_failure = 1;
    }
    if (len > 0xFFFF0000U) return 1; // Almost certainly negative so skip.

#ifdef EIGHTBITMAP
    level_prop->map_load_failure = 1;
#endif

    int xbytes = 0;
    if (len > level_prop->total_blocks) { xbytes = len - level_prop->total_blocks; len = level_prop->total_blocks; }
    for(int i=0; i<len; i++) {
	uint32_t ch;
	if ((ch = gzgetc(ifd)) == EOF) return 0;
#ifdef EIGHTBITMAP
	block_t b = (level_blocks[i] & 0x00FF) + (ch<<8);
	if (b > 255 && b < BLOCKMAX && level_prop->blockdef[b].defined) {
	    block_t nb = level_prop->blockdef[b].fallback;
	    if (nb < 255) b = nb; else b = Block_Bedrock;
	}
	if ((b > 255 && b < 767) || b >= 1023) b = Block_Bedrock;
	level_blocks[i] = b;
#else
	level_blocks[i] = (level_blocks[i] & 0x00FF) + (ch<<8);
#endif
    }
    while(xbytes>0) {
	int ch = gzgetc(ifd);
	if (ch == EOF) return 0;
	xbytes--;
    }
    return 1;
}

LOCAL int
read_blockarray3(gzFile ifd, uint32_t len)
{
    if (level_blocks == 0) {
	printlog("Incorrect BlockArray3 found");
	return 0;
    }
    if (len > 0xFFFF0000U) return 1; // Almost certainly negative so skip.
#ifdef EIGHTBITMAP
    level_prop->map_load_failure = 1;
#endif

    int xbytes = 0;
    if (len > level_prop->total_blocks) { xbytes = len - level_prop->total_blocks; len = level_prop->total_blocks; }
    for(uint32_t i=0; i<len; i++) {
	int ch;
	if ((ch = gzgetc(ifd)) == EOF) return 0;
	if (ch == 0) continue;
#ifdef EIGHTBITMAP
	block_t b = (level_blocks[i] & 0x00FF) + (ch<<8);
	if (b > 255 && b < BLOCKMAX && level_prop->blockdef[b].defined) {
	    block_t nb = level_prop->blockdef[b].fallback;
	    if (nb < 255) b = nb; else b = Block_Bedrock;
	}
	if ((b > 255 && b < 767) || b >= 1023) b = Block_Bedrock;
	level_blocks[i] = b;
#else
	level_blocks[i] = (level_blocks[i] & 0x00FF) + (ch<<8);
#endif
    }
    while(xbytes>0) {
	int ch = gzgetc(ifd);
	if (ch == EOF) return 0;
	xbytes--;
    }
    return 1;
}

LOCAL int
read_blockarray_physics(gzFile ifd, uint32_t len)
{
    if (level_blocks == 0) {
	printlog("Incorrect BlockArrayPhysics found");
	return 1;
    }
    if (len > 0xFFFF0000U) return 1; // Almost certainly negative so skip.

    int xbytes = 0;
    if (len > level_prop->total_blocks) { xbytes = len - level_prop->total_blocks; len = level_prop->total_blocks; }

#ifdef EIGHTBITMAP
    int load_failure = 0;
    for(int i = 0; i<len; i++) {
	if (level_blocks[i] >= Block_CPE) {
	    load_failure = 1;
	    break;
	}
    }

    if (load_failure)
	level_prop->map_load_failure = 1;
    else
#endif

	if (!level_prop->mcg_physics_blocks) {
	    level_prop->mcg_physics_blocks = 1;
	    define_mcg_physics_blocks();
	}

    for(uint32_t i=0; i<len; i++) {
	int ch;
	if ((ch = gzgetc(ifd)) == EOF) return 0;
#ifdef EIGHTBITMAP
	if (ch) {
	    if (level_prop->mcg_physics_blocks)
		level_blocks[i] = ch + MCG_PHYSICS_0FFSET;
	    else
		level_blocks[i] = mcg_physics[ch];
	}
#else
	if (ch)
	    level_blocks[i] = ch + MCG_PHYSICS_0FFSET;
#endif
    }
    while(xbytes>0) {
	int ch = gzgetc(ifd);
	if (ch == EOF) return 0;
	xbytes--;
    }
    return 1;
}

LOCAL void
set_colour(int *colour, int by, int value)
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
change_int_value(char * section, char * item, int64_t value)
{
    if (*section == 0 ||
	    strcmp(section, "ClassicWorld") == 0 ||
	    strcmp(section, "MapSize") == 0) {
	int ok = 1;
	if (value<0 && value >= -32768) value &= 0xFFFF; // CC written files
	if (value<1) ok = 0;
	else if (strcmp(item, "X") == 0) level_prop->cells_x = value;
	else if (strcmp(item, "Y") == 0) level_prop->cells_y = value;
	else if (strcmp(item, "Z") == 0) level_prop->cells_z = value;
	else ok = 0;
	if (ok) {
	    level_prop->total_blocks = (int64_t)level_prop->cells_x *
		    level_prop->cells_y * level_prop->cells_z;
	    if (level_prop->total_blocks)
		patch_map_nulls((xyzhv_t){0});
	}

	if (strcmp(item, "TimeCreated") == 0) level_prop->time_created = value;
	if (strcmp(item, "LastModified") == 0) level_prop->last_modified = value;
	if (strcmp(item, "LastBackup") == 0) level_prop->last_backup = value;

    } else if (strcasecmp(section, "SetBlock") == 0) {
	if (strcmp(item, "X") == 0) curr_x = value;
	if (strcmp(item, "Y") == 0) curr_y = value;
	if (strcmp(item, "Z") == 0) curr_z = value;
	if (strcmp(item, "Blk") == 0) {
	    if (curr_x >= 0 && curr_x < level_prop->cells_x &&
	        curr_y >= 0 && curr_y < level_prop->cells_y &&
	        curr_z >= 0 && curr_z < level_prop->cells_z)
		level_blocks[World_Pack(curr_x, curr_y, curr_z)] = value;
	}

    } else if (strcasecmp(section, "MapGenerator") == 0) {
	if (strcasecmp(item, "Seed") == 0)
	    sprintf(level_prop->seed, "%jd", (intmax_t)value);
    } else if (strcmp(section, "Spawn") == 0) {
	if (strcmp(item, "X") == 0) level_prop->spawn.x = value*32+16;
	if (strcmp(item, "Y") == 0) level_prop->spawn.y = value*32+16;
	if (strcmp(item, "Z") == 0) level_prop->spawn.z = value*32+16;
	if (strcmp(item, "H") == 0) level_prop->spawn.h = value;
	if (strcmp(item, "P") == 0) level_prop->spawn.v = value;
    } else if (strcmp(section, "SetSpawn") == 0) {
	if (strcmp(item, "X") == 0) level_prop->spawn.x = value;
	if (strcmp(item, "Y") == 0) level_prop->spawn.y = value;
	if (strcmp(item, "Z") == 0) level_prop->spawn.z = value;
	if (strcmp(item, "H") == 0) level_prop->spawn.h = value;
	if (strcmp(item, "P") == 0) level_prop->spawn.v = value;
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
	if (strcmp(item, "EdgeBlock") == 0) level_prop->edge_block = value;
	if (strcmp(item, "SideBlock") == 0) level_prop->side_block = value;
	if (strcmp(item, "EdgeHeight") == 0) level_prop->side_level = value;
	if (strcmp(item, "SideOffset") == 0) level_prop->side_offset = value;
	if (strcmp(item, "CloudsHeight") == 0) level_prop->clouds_height = value;
	if (strcmp(item, "CloudsSpeed") == 0) level_prop->clouds_speed = value;		// Float
	if (strcmp(item, "WeatherSpeed") == 0) level_prop->weather_speed = value;	// Float
	if (strcmp(item, "WeatherFade") == 0) level_prop->weather_fade = value;		// Float
	if (strcmp(item, "ExpFog") == 0) level_prop->exp_fog = value;
	if (strcmp(item, "SkyboxHor") == 0) level_prop->skybox_hor_speed = value;	// Float
	if (strcmp(item, "SkyboxVer") == 0) level_prop->skybox_ver_speed = value;	// Float

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
    } else if (strncasecmp(section, "SetBlockPermission", 18) == 0) {
        if (strcasecmp(item, "BlockType") == 0) permission_block = value;
        if (strcasecmp(item, "AllowPlacement") == 0) {
            if (permission_block > 0 && permission_block < BLOCKMAX) {
                level_prop->blockdef[permission_block].block_perm = !value +
                    (level_prop->blockdef[permission_block].block_perm & 2);
            }
        }
        if (strcasecmp(item, "AllowDeletion") == 0) {
            if (permission_block > 0 && permission_block < BLOCKMAX) {
                level_prop->blockdef[permission_block].block_perm = 2*!value +
                    (level_prop->blockdef[permission_block].block_perm & 1);
            }
        }

    } else if (strcmp(section, "HackControl") == 0) {

	int f = level_prop->hacks_flags;
	if (strcmp(item, "Flying") == 0) f = (f&~1) | (1&-!value);
	if (strcmp(item, "NoClip") == 0) f = (f&~2) | (2&-!value);
	if (strcmp(item, "Speed") == 0) f = (f&~4) | (4&-!value);
	if (strcmp(item, "SpawnControl") == 0) f = (f&~8) | (8&-!value);
	if (strcmp(item, "ThirdPersonView") == 0) f = (f&~0x10) | (0x10&-!value);
	if (strcmp(item, "JumpHeight") == 0) {
	    if (value == -1) f = (f&~0x20);
	    else {
		level_prop->hacks_jump = value;
		f |= 0x20;
	    }
	}
	level_prop->hacks_flags = f;

    } else if (strcmp(section, "MCCHost") == 0) {

	if (strcmp(item, "AllowChange") == 0) level_prop->disallowchange = !value;
	if (strcmp(item, "DisallowChange") == 0) level_prop->disallowchange = value;
	if (strcmp(item, "ReadOnly") == 0) level_prop->readonly = value;

    } else if (strcasecmp(section, "MCGalaxy") == 0) {

#ifndef EIGHTBITMAP
	if (strcasecmp(item, "PhysicsBlocks") == 0) {
	    level_prop->mcg_physics_blocks = value;
	    if (level_prop->mcg_physics_blocks)
		define_mcg_physics_blocks();
	}
#endif

    } else if (strncmp(section, "Block", 5) == 0) {
	if (strcmp(item, "ID2") == 0) {
	    current_block = value;
	    if (!(current_block < 0 || current_block >= BLOCKMAX)) {
		if (!level_prop->blockdef[current_block].defined) {
		    if (current_block >= 0 && current_block<Block_CPE) {
			level_prop->blockdef[current_block] = default_blocks[current_block];
		    } else {
			blockdef_t t = default_blocks[1];
			sprintf(t.name.c, "@%d", current_block);
			level_prop->blockdef[current_block] = default_blocks[1];
		    }
		}
		level_prop->blockdef[current_block].defined = 1;
		level_prop->blockdef[current_block].inventory_order = -1;
		level_prop->blockdef[current_block].fallback = -1;
	    }

	} else if (current_block < 0 || current_block >= BLOCKMAX) {
	    // Skip -- bad ID2.
	} else if (strcmp(item, "CollideType") == 0) {
	    level_prop->blockdef[current_block].collide = value;
	} else if (strcmp(item, "Speed") == 0) {
	    level_prop->blockdef[current_block].speed = value;	// Float
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
	} else if (strcmp(item, "FastFallFlag") == 0) {
	    level_prop->blockdef[current_block].fastfall_flag = value;
	} else if (strcmp(item, "StackBlock") == 0) {
	    level_prop->blockdef[current_block].stack_block = value;
	} else if (strcmp(item, "OdoorBlock") == 0) {
	    level_prop->blockdef[current_block].odoor_block = value;
	} else if (strcmp(item, "GrassBlock") == 0) {
	    level_prop->blockdef[current_block].grass_block = value;
	} else if (strcmp(item, "DirtBlock") == 0) {
	    level_prop->blockdef[current_block].dirt_block = value;
	}

    } else if (strncmp(section, "SelectionCuboid", 15) == 0) {
	if (strcmp(item, "SelectionID") == 0) {
	    current_cuboid = value;
	    if (current_cuboid>= 0 && current_cuboid <MAX_CUBES)
		level_prop->cuboid[current_cuboid].defined = 1;
	} else if (current_cuboid<0 || current_cuboid >= MAX_CUBES) {
	    // Bad cuboid
	} else if (strcmp(item, "StartX") == 0) {
	    level_prop->cuboid[current_cuboid].start_x = value;
	} else if (strcmp(item, "StartY") == 0) {
	    level_prop->cuboid[current_cuboid].start_y = value;
	} else if (strcmp(item, "StartZ") == 0) {
	    level_prop->cuboid[current_cuboid].start_z = value;
	} else if (strcmp(item, "EndX") == 0) {
	    level_prop->cuboid[current_cuboid].end_x = value;
	} else if (strcmp(item, "EndY") == 0) {
	    level_prop->cuboid[current_cuboid].end_y = value;
	} else if (strcmp(item, "EndZ") == 0) {
	    level_prop->cuboid[current_cuboid].end_z = value;
	} else if (strcmp(item, "Colour") == 0) {
	    level_prop->cuboid[current_cuboid].colour = value;
	} else if (strcmp(item, "Opacity") == 0) {
	    level_prop->cuboid[current_cuboid].opacity = value;
	}
	if (current_cuboid >= 0 && current_cuboid < MAX_CUBES) {
	    int c = -1;
	    if (strcmp(item, "R") == 0) c = 0;
	    if (strcmp(item, "G") == 0) c = 1;
	    if (strcmp(item, "B") == 0) c = 2;
	    if (c>=0) set_colour(&level_prop->cuboid[current_cuboid].colour, c, value);
	}
    }
}

LOCAL void
change_float_value(char * section, char * item, double value)
{
    // Values stored in the CW file as floats.
    if (strcmp(section, "EnvMapAspect") == 0) {
	if (strcmp(item, "CloudsSpeed") == 0) level_prop->clouds_speed = round(value * 256.0);
	if (strcmp(item, "WeatherSpeed") == 0) level_prop->weather_speed = round(value * 256.0);
	if (strcmp(item, "WeatherFade") == 0) level_prop->weather_fade = round(value * 128.0);
	if (strcmp(item, "SkyboxHor") == 0) level_prop->skybox_hor_speed = round(value * 1024.0);
	if (strcmp(item, "SkyboxVer") == 0) level_prop->skybox_ver_speed = round(value * 1024.0);
    } else if (strncmp(section, "Block", 5) == 0) {
	if (strcmp(item, "Speed") == 0) {
	    // NB: Commands an ini file use *1024.0 value as it's easy.
	    // Only convert to byte for transmission.
	    // Round to byte values when we write back.
	    level_prop->blockdef[current_block].speed = round(value * 1024.0);
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
	    if (len < 4) return;
	    for(int i=0; i<4; i++) {
		level_prop->blockdef[current_block].fog[i] = value[i];
	    }

	} else if (strcmp(item, "Coords") == 0) {
	    if (len < 6) return;
	    for(int i=0; i<6; i++) {
		level_prop->blockdef[current_block].coords[i] = value[i];
	    }

	} else if (strcmp(item, "UUID") == 0) {
	    for(int i=0; i<sizeof(level_prop->uuid) && i<len; i++)
		level_prop->uuid[i] = value[i];
	}
    }
}

LOCAL void
change_str_value(char * section, char * item, char * value)
{
    if (*section == 0 || strcmp(section, "ClassicWorld") == 0) {
	if (strcmp(item, "Name") == 0) {
	    cpy_nstr(level_prop->name, MB_STRLEN*2, value);
	}
    } else if (strcmp(section, "EnvMapAppearance") == 0 ||
	       strcmp(section, "EnvMapAspect") == 0) {
	if (strcmp(item, "TextureURL") == 0) {
	    cpy_nstr(level_prop->texname.c, MB_STRLEN, value);
	}
    } else if (strncmp(section, "Block", 5) == 0) {
	if (strcmp(item, "Name") == 0) {
	    if (current_block >= 0 && current_block < BLOCKMAX) {
		cpy_nstr(level_prop->blockdef[current_block].name.c, MB_STRLEN, value);
	    }
	}
    } else if (strncmp(section, "SelectionCuboid", 15) == 0) {
	if (strcmp(item, "Label") == 0) {
	    if (current_cuboid >= 0 && current_cuboid < MAX_CUBES) {
		cpy_nstr(level_prop->cuboid[current_cuboid].name.c, MB_STRLEN, value);
	    }
	}
    } else if (strcasecmp(section, "Ident") == 0) {
	if (strcasecmp(item, "Motd") == 0) {
	    cpy_nstr(level_prop->motd, MB_STRLEN*2, value);
	}
    } else if (strcasecmp(section, "MCCHost") == 0) {
	if (strcasecmp(item, "Theme") == 0) {
	    cpy_nstr(level_prop->theme, MB_STRLEN*2, value);
	}
	if (strcasecmp(item, "Seed") == 0) {
	    cpy_nstr(level_prop->seed, MB_STRLEN*2, value);
	}
    } else if (strcasecmp(section, "MapGenerator") == 0) {
	if (strcasecmp(item, "Software") == 0) {
	    cpy_nstr(level_prop->software, MB_STRLEN*2, value);
	}
	if (strcasecmp(item, "MapGeneratorName") == 0) {
	    cpy_nstr(level_prop->theme, MB_STRLEN*2, value);
	}
	if (strcasecmp(item, "Seed") == 0) {
	    cpy_nstr(level_prop->seed, MB_STRLEN*2, value);
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
cpy_nstr(char *buf, int buflen, char *str)
{
    char *d = buf;
    char * s;
    for(s=str; s && *s; s++) {
	*d++ = *s;
	if (d == buf+buflen) break;
    }
    *d = 0;
}

LOCAL void
define_mcg_physics_blocks()
{
    char * start = mcgalaxy_names;
    for(int i = 0; i<256; i++) {
	int blk = i + MCG_PHYSICS_0FFSET;
	char namebuf[64];
	*namebuf = 0;
	if (start) {
	    char * e = strchr(start, '@');
	    if (e) {
		if (e!=start) memcpy(namebuf, start, e-start);
		namebuf[e-start] = 0;
		start = e+1;
	    } else
		start = e;
	}

	if (blk <= 0) continue;
	if (!level_prop->blockdef[blk].defined && blk >= Block_CPE) {
	    level_prop->blockdef[blk] = level_prop->blockdef[mcg_physics[i]];
	    level_prop->blockdef[blk].defined = 1;
	    level_prop->blockdef[blk].no_save = 1;
	    level_prop->blockdef[blk].fallback = mcg_physics[i];
	    level_prop->blockdef[blk].inventory_order = 0;
	    if (*namebuf) {
		strcpy(level_prop->blockdef[blk].name.c, namebuf);
		if (strncmp(namebuf, "Door", 4) == 0) {
		    if (strstr(namebuf, "Air"))
			; // "Door..Air" blocks.
		    else {
			level_prop->blockdef[blk].door_flag = 1;
			// Door_Green and Door_TNT Air type is not Air.
		    }
		}
		else if (strncmp(namebuf, "Op", 2) == 0)
		    level_prop->blockdef[blk].block_perm = 3;
		else if (strstr(namebuf, "_Portal") == 0)
		    level_prop->blockdef[blk].portal_flag = 1;
		else if (strstr(namebuf, "_Message") == 0)
		    level_prop->blockdef[blk].mblock_flag = 1;
	    } else
		strcpy(level_prop->blockdef[blk].name.c, "unknown");
	}
    }
}

char mcgalaxy_names[] =
    "Air@Stone@Grass@Dirt@Cobblestone@Wood@Sapling@Bedrock@"
    "Active_Water@Water@Active_Lava@Lava@Sand@Gravel@Gold_Ore@Iron_Ore@"
    "Coal@Log@Leaves@Sponge@Glass@Red@Orange@Yellow@"
    "Lime@Green@Teal@Aqua@Cyan@Blue@Indigo@Violet@"
    "Magenta@Pink@Black@Gray@White@Dandelion@Rose@Brown_Shroom@"
    "Red_Shroom@Gold@Iron@DoubleSlab@Slab@Brick@TNT@BookShelf@"
    "MossyRocks@Obsidian@CobblestoneSlab@Rope@SandStone@Snow@Fire@LightPink@"
    "ForestGreen@Brown@DeepBlue@Turquoise@Ice@CeramicTile@MagmaBlock@Pillar@"
    "Crate@StoneBrick@@@@@FlagBase@@"
    "@Fast_Hot_Lava@C4@C4_Det@@@@@"
    "Door_Cobblestone@@@Door_Red@@Door_Orange@Door_Yellow@Door_LightGreen@"
    "@Door_AquaGreen@Door_Cyan@Door_LightBlue@Door_Purple@Door_LightPurple@Door_Pink@Door_DarkPink@"
    "Door_DarkGrey@Door_LightGrey@Door_White@@Op_Glass@Opsidian@Op_Brick@Op_Stone@"
    "Op_Cobblestone@Op_Air@Op_Water@Op_Lava@@Lava_Sponge@Wood_Float@Door@"
    "Lava_Fast@Door_Obsidian@Door_Glass@Door_Stone@Door_Leaves@Door_Sand@Door_Wood@Door_Green@"
    "Door_TNT@Door_Stair@tDoor@tDoor_Obsidian@tDoor_Glass@tDoor_Stone@tDoor_Leaves@tDoor_Sand@"
    "tDoor_Wood@tDoor_Green@White_Message@Black_Message@Air_Message@Water_Message@Lava_Message@tDoor_TNT@"
    "tDoor_Stair@tDoor_Air@tDoor_Water@tDoor_lava@Waterfall@Lavafall@@Water_Faucet@"
    "Lava_Faucet@Finite_Water@Finite_Lava@Finite_Faucet@oDoor@oDoor_Obsidian@oDoor_Glass@oDoor_Stone@"
    "oDoor_Leaves@oDoor_Sand@oDoor_Wood@oDoor_Green@oDoor_TNT@oDoor_Stair@oDoor_Lava@oDoor_Water@"
    "Air_Portal@Water_Portal@Lava_Portal@Custom_Block@Air_Door@Air_Switch@Door_Water@Door_Lava@"
    "oDoor_Air@oDoor_Obsidian_Air@oDoor_Glass_Air@oDoor_Stone_Air@oDoor_Leaves_Air@oDoor_Sand_Air@oDoor_Wood_Air@Blue_Portal@"
    "Orange_Portal@oDoor_Red@oDoor_TNT_Air@oDoor_Stair_Air@oDoor_Lava_Air@oDoor_Water_Air@Small_TNT@Big_TNT@"
    "TNT_Explosion@Lava_Fire@Nuke_TNT@RocketStart@RocketHead@Firework@Hot_Lava@Cold_Water@"
    "Nerve_Gas@Active_Cold_Water@Active_Hot_Lava@Magma@Geyser@Checkpoint@@@"
    "Air_Flood@Door_Air@Air_Flood_Layer@Air_Flood_Down@Air_Flood_Up@@@@"
    "@@@Door8_Air@Door9_Air@@@@"
    "@@@@Door_Iron@Door_Dirt@Door_Grass@Door_Blue@"
    "Door_Book@@@@@@Train@Creeper@"
    "Zombie@Zombie_Head@@Dove@Pidgeon@Duck@Phoenix@Red_Robin@"
    "Blue_Bird@@Killer_Phoenix@@@GoldFish@Sea_Sponge@Shark@"
    "Salmon@Betta_Fish@Lava_Shark@Snake@Snake_Tail@Door_Gold@@@";
