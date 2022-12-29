#include <math.h>

#include "ini_struct.h"

#if INTERFACE

typedef int (*ini_func_t)(ini_state_t *st, char * fieldname, char **value);

typedef ini_state_t ini_state_t;
struct ini_state_t {
    FILE * fd;
    char * filename;

    char * curr_section;
    uint8_t all;	// Process all fields
    uint8_t write;	// Set to write fields
    uint8_t quiet;	// Don't comment to the remote (use stderr)
    uint8_t no_unsafe;	// Disable protected fields
};

#endif

/*HELP inifile
Empty lines are ignored.
Section syntax is normal [...]
    Section syntax uses "." as element separator within name.
    Elements may be numeric for array index.
    Other elements are identifiers.

Comment lines have a "#" or ";" at the start of the line
Labels are followed by "=" or ":"
Spaces and tabs before "=", ":", ";", "#" are ignored.

Value is trimmed at both ends for spaces and tabs.

Quotes are not special.
Backslashes are not special
Strings are generally limited to 64 cp437 characters by protocol.
Character set of file is UTF8
*/

#define WC(_x) WC2(!st->no_unsafe, _x)
#define WC2(_c, _x) (st->write && (_c)) ?"":_x

userrec_t * user_ini_tgt = 0;
server_ini_t * server_ini_tgt = 0;

int
system_ini_fields(ini_state_t *st, char * fieldname, char **fieldvalue)
{
    char *section = "", *fld;
    int found = 0;

    section = "server";
    if (st->all || strcmp(section, st->curr_section) == 0)
    {
	INI_STRARRAYCP437("Software", server->software);
	INI_STRARRAYCP437("Name", server->name);
	INI_STRARRAYCP437("Motd", server->motd);
	INI_STRARRAYCP437("Main", server->main_level);

	if (!st->no_unsafe)
	    INI_STRARRAY("Salt", server->secret);	//Base62
	else if (st->write)
	    INI_STRARRAY(WC("Salt"), "XXXXXXXXXXXXXXXX");

	INI_DURATION("KeyRotation", server->key_rotation);

	INI_BOOLVAL("OPFlag", server->op_flag);
	INI_BOOLVAL("NoCPE", server->cpe_disabled);
	INI_BOOLVAL("DisableWebServer", server->disable_web_server);
	INI_BOOLVAL("CheckWebClientIP", server->check_web_client_ip);

	if (st->write) fprintf(st->fd, "\n");

	server_ini_tgt = &server->shared_ini_settings;
	found |= system_x_ini_fields(st, fieldname, fieldvalue);
	server_ini_tgt = 0;

	INI_INTVAL("MaxPlayers", server->max_players);
	INI_STRARRAY(WC2(!*localnet_cidr, "Localnet"), localnet_cidr);

	INI_STRARRAY(WC2(!*logfile_pattern, "Logfile"), logfile_pattern);

	INI_DURATION("SaveInterval", server->save_interval);
	INI_DURATION("BackupInterval", server->backup_interval);
	INI_BOOLVAL("NoUnloadMain", server->no_unload_main);

	if (st->write) fprintf(st->fd, "\n");

	INI_BOOLVAL("FlagLogCommands", server->flag_log_commands);
	INI_BOOLVAL("FlagLogPlaceCommands", server->flag_log_place_commands);
	INI_BOOLVAL("FlagLogChat", server->flag_log_chat);

	if (st->write) fprintf(st->fd, "\n");

	INI_DURATION("AFKInterval", server->afk_interval);
	INI_DURATION("AFKKickInterval", server->afk_kick_interval);
	INI_BOOLVAL("AllowUserLevels", server->allow_user_levels);
	INI_INTVAL("PlayerUpdateMS", server->player_update_ms);
	INI_DURATION("IPConnectDelay", server->ip_connect_delay);

	if (st->write) fprintf(st->fd, "\n");

	INI_INTVAL("BlockSpamCount", server->block_spam_count);
	INI_DURATION("BlockSpamInterval", server->block_spam_interval);
	INI_INTVAL("CmdSpamCount", server->cmd_spam_count);
	INI_DURATION("CmdSpamInterval", server->cmd_spam_interval);
	INI_DURATION("CmdSpamBan", server->cmd_spam_ban);
	INI_INTVAL("ChatSpamCount", server->chat_spam_count);
	INI_DURATION("ChatSpamInterval", server->chat_spam_interval);
	INI_DURATION("ChatSpamBan", server->chat_spam_ban);

    }

    int tn = 0;
    char sectionbuf[128];
    do
    {
	if (st->all) {
	    if (!textcolour[tn].defined) { tn++; continue; }
	    sprintf(sectionbuf, "textcolour.%d", tn);
	    section = sectionbuf;
	} else {
	    strncpy(sectionbuf, st->curr_section, 11);
	    sectionbuf[11] = 0;
	    if (strcasecmp(sectionbuf, "textcolour.") == 0) {
		tn = atoi(st->curr_section+11);
		if (!tn && st->curr_section[11] == '0') break;
		if (tn < 0 || tn >= 256) break;
		sprintf(sectionbuf, "textcolour.%d", tn);
		section = sectionbuf;
		textcolour[tn].defined = 1;
	    } else
		break;
	}

	if (!st->write || textcolour[tn].name.c[0])
	    INI_NBTSTR("Name", textcolour[tn].name);
	if (!st->write || textcolour[tn].colour >= 0) {
	    INI_INTHEX("Colour", textcolour[tn].colour);
	    INI_INTVAL("Alpha", textcolour[tn].a);
	}
	if (!st->write || textcolour[tn].fallback)
	    INI_INTVAL("Fallback", textcolour[tn].fallback);

	tn++;
    } while(st->all && tn < 256);

    return found;
}

int
system_x_ini_fields(ini_state_t *st, char * fieldname, char **fieldvalue)
{
    char *section = "", *fld;
    int found = 0;
    int zero_tgt = 0;

    if (server_ini_tgt == 0) {
	zero_tgt = 1;
	server_ini_tgt = &server_ini_settings;
    }

    section = "server";
    if (st->all || strcmp(section, st->curr_section) == 0)
    {
	INI_BOOLVAL("tcp", server_ini_tgt->start_tcp_server);
	INI_INTVAL("Port", server_ini_tgt->tcp_port_no);
	INI_BOOLVAL("Detach", server_ini_tgt->detach_tcp_server);

	INI_STRARRAY("Heartbeat", server_ini_tgt->heartbeat_url);
	INI_BOOLVAL("PollHeartbeat", server_ini_tgt->enable_heartbeat_poll);
	INI_BOOLVAL("UseHttpPost", server_ini_tgt->use_http_post);
	INI_BOOLVAL("Private", server_ini_tgt->private);
	INI_STRARRAY("UserSuffix", server_ini_tgt->user_id_suffix);
	INI_BOOLVAL("DisallowIPAdmin", server_ini_tgt->disallow_ip_admin);
	INI_BOOLVAL("DisallowIPVerify", server_ini_tgt->disallow_ip_verify);
	INI_BOOLVAL("AllowPassVerify", server_ini_tgt->allow_pass_verify);
	INI_BOOLVAL("VoidForLogin", server_ini_tgt->void_for_login);

	// Do not store in ini file
	// INI_BOOLVAL("Runonce", server_ini_tgt->server_runonce);
	// INI_BOOLVAL("Inetd", server_ini_tgt->inetd_mode);

	if (st->write) fprintf(st->fd, "\n");
    }

    if (zero_tgt) server_ini_tgt = 0;
    return found;
}

int
level_ini_fields(ini_state_t *st, char * fieldname, char **fieldvalue)
{
    char *section = "", *fld;
    int found = 0;

    // When writing include a copy of some system stuff.
    // Skip it quietly on read.
    if (st->write) {
	section = "source";
	INI_STRARRAYCP437("Software", server->software);
	INI_STRARRAYCP437("Name", server->name);
	INI_STRARRAYCP437("Motd", server->motd);
	INI_STRARRAYCP437("Level", current_level_name);
	INI_TIME_T("TimeCreated", level_prop->time_created);

    } else if (st->curr_section && strcmp("source", st->curr_section) == 0)
	return 1;

    section = "level";
    if (st->all || strcmp(section, st->curr_section) == 0)
    {
	if (!st->no_unsafe) {
	    INI_INTVAL("Size.X", level_prop->cells_x);
	    INI_INTVAL("Size.Y", level_prop->cells_y);
	    INI_INTVAL("Size.Z", level_prop->cells_z);
	} else {
	    int x = 0, y = 0, z = 0;	// Ignore these fields without comment.
	    INI_INTVAL("Size.X", x);
	    INI_INTVAL("Size.Y", y);
	    INI_INTVAL("Size.Z", z);
	}

	INI_STRARRAYCP437("Motd", level_prop->motd);
	INI_STRARRAYCP437("Name", level_prop->name);
	INI_STRARRAYCP437("Software", level_prop->software);
	INI_STRARRAYCP437("Theme", level_prop->theme);
	INI_STRARRAYCP437("Seed", level_prop->seed);
	if (!st->write)
	    INI_TIME_T("TimeCreated", level_prop->time_created);

	INI_FIXEDP("Spawn.X", level_prop->spawn.x, 32);
	INI_FIXEDP("Spawn.Y", level_prop->spawn.y, 32);
	INI_FIXEDP("Spawn.Z", level_prop->spawn.z, 32);
	INI_FIXEDP2("Spawn.H", level_prop->spawn.h, 256, 360);
	INI_FIXEDP2("Spawn.V", level_prop->spawn.v, 256, 360);

	INI_FIXEDP("ClickDistance", level_prop->click_distance, 32);

	INI_INTHEX("HacksFlags", level_prop->hacks_flags);
	INI_INTVAL("HacksJump", level_prop->hacks_jump);

	INI_NBTSTR("Texture", level_prop->texname);
	INI_INTVAL("WeatherType", level_prop->weather);

	INI_INTHEX("SkyColour", level_prop->sky_colour);
	INI_INTHEX("CloudColour", level_prop->cloud_colour);
	INI_INTHEX("FogColour", level_prop->fog_colour);
	INI_INTHEX("AmbientColour", level_prop->ambient_colour);
	INI_INTHEX("SunlightColour", level_prop->sunlight_colour);
	INI_INTHEX("SkyboxColour", level_prop->skybox_colour);

	INI_INTVAL("SideBlock", level_prop->side_block);
	INI_INTVAL("EdgeBlock", level_prop->edge_block);
	INI_INTVAL("SideLevel", level_prop->side_level);
	INI_INTVAL("SideOffset", level_prop->side_offset);

	INI_INTVAL("CloudHeight", level_prop->clouds_height);
	INI_INTVAL("MaxFog", level_prop->max_fog);

	INI_FIXEDP("CloudsSpeed", level_prop->clouds_speed, 256);
	INI_FIXEDP("WeatherSpeed", level_prop->weather_speed, 256);
	INI_FIXEDP("WeatherFade", level_prop->weather_fade, 128);
	INI_INTVAL("ExpFog", level_prop->exp_fog);
	INI_FIXEDP("SkyboxHorSpeed", level_prop->skybox_hor_speed, 1024);
	INI_FIXEDP("SkyboxVerSpeed", level_prop->skybox_ver_speed, 1024);

	INI_BOOLVAL("ResetHotbar", level_prop->reset_hotbar);
	INI_BOOLVAL("DisallowChange", level_prop->disallowchange);
	INI_BOOLVAL("ReadOnly", level_prop->readonly);
	if (!st->write)
	    INI_BOOLVAL("DirtySave", level_prop->dirty_save);
    }

    int bn = 0;
    char sectionbuf[128];
    do
    {
	if (st->all) {
	    if (!level_prop->blockdef[bn].defined &&
		level_prop->blockdef[bn].block_perm == 0) { bn++; continue; }
	    sprintf(sectionbuf, "block.%d", bn);
	    section = sectionbuf;
	} else {
	    strncpy(sectionbuf, st->curr_section, 6);
	    sectionbuf[6] = 0;
	    if (strcasecmp(sectionbuf, "block.") == 0) {
		bn = atoi(st->curr_section+6);
		if (!bn && st->curr_section[6] == '0') break;
		if (bn < 0 || bn >= BLOCKMAX) break;
		sprintf(sectionbuf, "block.%d", bn);
		section = sectionbuf;
		level_prop->blockdef[bn].defined = 1;
	    } else
		break;
	}

	int tf = found;
	found = 0;

	if (level_prop->blockdef[bn].defined || !st->write) {
	    INI_BOOLVAL(WC("Defined"), level_prop->blockdef[bn].defined);
	    INI_BOOLVAL(WC("NoSave"), level_prop->blockdef[bn].no_save);
	    INI_NBTSTR("Name", level_prop->blockdef[bn].name);
	    INI_INTVAL("Collide", level_prop->blockdef[bn].collide);
	    INI_BOOLVAL("TransmitsLight", level_prop->blockdef[bn].transmits_light);
	    INI_INTVAL("WalkSound", level_prop->blockdef[bn].walksound);
	    INI_BOOLVAL("FullBright", level_prop->blockdef[bn].fullbright);
	    int shape = !level_prop->blockdef[bn].shape;
	    INI_BOOLVAL("Shape", shape);
	    level_prop->blockdef[bn].shape = shape?0:16;
	    INI_INTVAL("Draw", level_prop->blockdef[bn].draw);
	    INI_FIXEDP("Speed", level_prop->blockdef[bn].speed, 1024);
	    INI_BLKVAL("Fallback", level_prop->blockdef[bn].fallback);
	    INI_INTVAL("Texture.Top", level_prop->blockdef[bn].textures[0]);
	    INI_INTVAL("Texture.Left", level_prop->blockdef[bn].textures[1]);
	    INI_INTVAL("Texture.Right", level_prop->blockdef[bn].textures[2]);
	    INI_INTVAL("Texture.Front", level_prop->blockdef[bn].textures[3]);
	    INI_INTVAL("Texture.Back", level_prop->blockdef[bn].textures[4]);
	    INI_INTVAL("Texture.Bottom", level_prop->blockdef[bn].textures[5]);
	    INI_INTVAL("Fog.Den", level_prop->blockdef[bn].fog[0]);
	    INI_INTVAL("Fog.R", level_prop->blockdef[bn].fog[1]);
	    INI_INTVAL("Fog.G", level_prop->blockdef[bn].fog[2]);
	    INI_INTVAL("Fog.B", level_prop->blockdef[bn].fog[3]);
	    INI_INTVAL("Min.X", level_prop->blockdef[bn].coords[0]);
	    INI_INTVAL("Min.Y", level_prop->blockdef[bn].coords[1]);
	    INI_INTVAL("Min.Z", level_prop->blockdef[bn].coords[2]);
	    INI_INTVAL("Max.X", level_prop->blockdef[bn].coords[3]);
	    INI_INTVAL("Max.Y", level_prop->blockdef[bn].coords[4]);
	    INI_INTVAL("Max.Z", level_prop->blockdef[bn].coords[5]);

	    INI_BLKVAL("StackBlock", level_prop->blockdef[bn].stack_block);
	    INI_BLKVAL("GrassBlock", level_prop->blockdef[bn].grass_block);
	    INI_BLKVAL("DirtBlock", level_prop->blockdef[bn].dirt_block);
	    INI_BLKVAL("Order", level_prop->blockdef[bn].inventory_order);
	}
	if (found && !st->write) level_prop->blockdef_generation++;
	found |= tf;

	if (level_prop->blockdef[bn].block_perm || !st->write) {
	    INI_BLKVAL("Permission", level_prop->blockdef[bn].block_perm);
	    if (!level_prop->blockdef[bn].defined && st->write)
		INI_BOOLVAL("Defined", level_prop->blockdef[bn].defined);
	}

	bn++;
    } while(st->all && bn < BLOCKMAX);

    int cn = 0;
    do
    {
	if (st->all) {
	    if (!level_prop->cuboid[cn].defined) { cn++; continue; }
	    sprintf(sectionbuf, "cuboid.%d", cn);
	    section = sectionbuf;
	} else {
	    strncpy(sectionbuf, st->curr_section, 7);
	    sectionbuf[7] = 0;
	    if (strcasecmp(sectionbuf, "cuboid.") == 0) {
		cn = atoi(st->curr_section+7);
		if (cn < 0 || cn >= MAX_CUBES) break;
		sprintf(sectionbuf, "cuboid.%d", cn);
		section = sectionbuf;
		level_prop->cuboid[cn].defined = 1;
	    } else
		break;
	}

	INI_BOOLVAL(WC("Defined"), level_prop->cuboid[cn].defined);
	INI_NBTSTR("Label", level_prop->cuboid[cn].name);
	INI_INTVAL("Start.X", level_prop->cuboid[cn].start_x);
	INI_INTVAL("Start.Y", level_prop->cuboid[cn].start_y);
	INI_INTVAL("Start.Z", level_prop->cuboid[cn].start_z);
	INI_INTVAL("End.X", level_prop->cuboid[cn].end_x);
	INI_INTVAL("End.Y", level_prop->cuboid[cn].end_y);
	INI_INTVAL("End.Z", level_prop->cuboid[cn].end_z);
	INI_INTHEX("Colour", level_prop->cuboid[cn].colour);
	INI_INTVAL("Opacity", level_prop->cuboid[cn].opacity);

	cn++;
    } while(st->all && cn < MAX_CUBES);

    return found;
}

int
user_ini_fields(ini_state_t *st, char * fieldname, char **fieldvalue)
{
    char *section = "", *fld;
    int found = 0;

    if (!user_ini_tgt) return found;

    section = "user";
    if (st->all || strcmp(section, st->curr_section) == 0)
    {
	INI_INTMAXVAL("UserNo", user_ini_tgt->user_no);
	INI_STRARRAYCP437("UserId", user_ini_tgt->user_id);
	if (!st->write) {
	    int user_perm = INT_MIN;
	    INI_INTVAL("UserPerm", user_perm);
	    if (user_perm != INT_MIN) {
		user_ini_tgt->user_group = !user_perm;
		user_ini_tgt->ini_dirty = 1;
	    }
	}
	INI_INTVAL("UserGroup", user_ini_tgt->user_group);
	if (user_ini_tgt->user_group < 0 || !st->write)
	    INI_STRARRAYCP437("BanMessage", user_ini_tgt->ban_message);
	INI_STRARRAYCP437("Nick", user_ini_tgt->nick);
	INI_STRARRAYCP437("Colour", user_ini_tgt->colour);
	INI_STRARRAYCP437("Skin", user_ini_tgt->skin);
	INI_STRARRAYCP437("Model", user_ini_tgt->model);
	INI_INTMAXVAL("BlocksPlaced", user_ini_tgt->blocks_placed);
	INI_INTMAXVAL("BlocksDeleted", user_ini_tgt->blocks_deleted);
	INI_INTMAXVAL("BlocksDrawn", user_ini_tgt->blocks_drawn);
	INI_TIME_T("FirstLogon", user_ini_tgt->first_logon);
	INI_TIME_T("LastLogon", user_ini_tgt->last_logon);
	INI_INTMAXVAL("LogonCount", user_ini_tgt->logon_count);
	INI_INTMAXVAL("KickCount", user_ini_tgt->kick_count);
	INI_INTMAXVAL("DeathCount", user_ini_tgt->death_count);
	INI_INTMAXVAL("MessageCount", user_ini_tgt->message_count);
	INI_INTMAXVAL("CoinCount", user_ini_tgt->coin_count);
	if (user_ini_tgt->click_distance >= 0 || !st->write)
	    INI_FIXEDP("ClickDistance", user_ini_tgt->click_distance, 32);
	INI_DURATION("TimeOnline", user_ini_tgt->time_online_secs);
	INI_STRARRAY("LastIP", user_ini_tgt->last_ip);
	INI_STRARRAY("Timezone", user_ini_tgt->timezone);

	INI_STRARRAYCP437("Title", user_ini_tgt->title);
	INI_STRARRAYCP437("TitleColour", user_ini_tgt->title_colour);
    }
    return found;
}

int
cmdset_ini_fields(ini_state_t *st, char * fieldname, char **fieldvalue)
{
    char *section = "", *fld;
    int found = 0;

    section = "cmdlimits";
    INI_INTMAXVAL("MaxUserBlock", command_limits.max_user_blocks);

    section = "cmdset";
    if (st->all || strcmp(section, st->curr_section) == 0)
    {
	for(int i = 0; command_list[i].name; i++) {
	    int c = i;
	    while(c>0 && command_list[c].dup && !command_list[c].nodup &&
		command_list[c].function == command_list[c-1].function)
		c--;
	    if (c != i) continue;

	    INI_INTVAL(command_list[c].name, command_list[c].perm_okay);
	}
    }

    return found;
}

int
save_ini_file(ini_func_t filetype, char * filename, char * oldfilename)
{
    ini_state_t st = (ini_state_t){.all=1, .write=1};
    char * comment_data = 0;
    if (!oldfilename) oldfilename = filename;
    FILE * fd = fopen(oldfilename, "r");
    if (fd) {
	comment_data = read_comment_data(fd);
	fclose(fd);
    }
    st.fd = fopen(filename, "w");
    if (!st.fd) {
	perror(filename);
	return -1;
    }
    if (comment_data) {
	fprintf(st.fd, "%s", comment_data);
	free(comment_data);
    }
    filetype(&st,0,0);
    fclose(st.fd);
    if (st.curr_section) free(st.curr_section);
    return 0;
}

LOCAL char *
read_comment_data(FILE * fd)
{
    char * comments = 0;
    int len = 0, sz = 0;
    char ibuf[BUFSIZ];
    while(fgets(ibuf, sizeof(ibuf)-1, fd)) {
	char * p;
	for(p=ibuf; *p == ' ' || *p == '\t'; p++);
	if (*p != '#' && *p != ';') continue;

	p = ibuf + strlen(ibuf);
	for(;p>ibuf && (p[-1] == '\n' || p[-1] == '\r' || p[-1] == ' ' || p[-1] == '\t');p--) { p[-1] = '\n'; *p = 0; }

	if (len+strlen(ibuf)+2 > sz) {
	    int n = len*2;
	    if (n == 0) n = 1024;
	    if (n < len+strlen(ibuf)+2) n += len+strlen(ibuf)+2;
	    comments = realloc(comments, sz=n);
	    if (!comments) return 0;
	}
	strcpy(comments+len, ibuf);
	len += strlen(ibuf);
    }

    return comments;
}

int
load_ini_file(ini_func_t filetype, char * filename, int quiet, int no_unsafe)
{
    int rv = 0;
    ini_state_t st = {.quiet = quiet, .no_unsafe=no_unsafe, .filename = filename};
    FILE *ifd = fopen(filename, "r");
    if (!ifd) {
	if (!quiet)
	    printf_chat("&WFile not found: &e%s", filename);
	return -1;
    }

    char ibuf[BUFSIZ];
    while(fgets(ibuf, sizeof(ibuf), ifd)) {
	int v;
	if ((v=load_ini_line(&st, filetype, ibuf)) != 0) {
	    if(v == 2) { rv = -1; break; }
	    rv++;
	}
    }

    fclose(ifd);
    if (st.curr_section) free(st.curr_section);
    return rv;
}

// Load one ini line
// RV 0=> Line okay, 1=> Unknown option, 2=> Bad line.
int
load_ini_line(ini_state_t *st, ini_func_t filetype, char *ibuf)
{
    char * p = ibuf + strlen(ibuf);
    for(;p>ibuf && (p[-1] == '\n' || p[-1] == '\r' || p[-1] == ' ' || p[-1] == '\t');p--) p[-1] = 0;

    for(p=ibuf; *p == ' ' || *p == '\t'; p++);
    if (*p == '#' || *p == ';' || *p == 0) return 0;
    if (*p == '[') {
	p = ini_extract_section(st, p);
	if (p) for(; *p == ' ' || *p == '\t'; p++);
	if (!p || *p == 0)
	    return 0;
    }
    char label[64];
    int rv = ini_decode_lable(&p, label, sizeof(label));
    if (!rv) {
	if (st->quiet)
	    printlog("Invalid label %s in %s section %s", ibuf, st->filename, st->curr_section?:"-");
	else
	    printf_chat("&WInvalid label &S%s&W in &S%s&W section &S%s&W", ibuf, st->filename, st->curr_section?:"-");
	return 2;
    }
    if (!st->curr_section || !filetype(st, label, &p)) {
	if (st->quiet) {
	    printlog("Unknown item \"%s\" in file \"%s\" section \"%s\" -- label \"%s\" value \"%s\"",
		ibuf, st->filename, st->curr_section?:"-", label, p);
	} else
	    printf_chat("#&WUnknown item&S \"%s\" section \"%s\"", ibuf, st->curr_section?:"-");
	if (!st->curr_section) return 2;
	return 1;
    }
    return 0;
}

LOCAL char *
ini_extract_section(ini_state_t *st, char *line)
{
    if (st->curr_section) { free(st->curr_section); st->curr_section = 0; }

    char buf[BUFSIZ];
    char *d = buf, *p = line;
    if (*p != '[') return p;
    for(p++; *p; p++) {
	if (*p == ' ' || *p == '_' || *p == '\t') continue;
	if (*p == ']') { p++; break; }
	if (*p >= 'A' && *p <= 'Z') {
	    *d++ = *p - 'A' + 'a';
	    continue;
	}
	if ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'z') || *p == '.') {
	    *d++ = *p;
	    continue;
	}
	if (*p == '/' || *p == '\\' || *p == '-')
	    *d++ = '.';
	// Other chars are ignored.
    }
    *d = 0;
    if (*buf)
	st->curr_section = strdup(buf);
    return p;
}

LOCAL int
ini_decode_lable(char **line, char *buf, int len)
{
    char * d = buf, *p = *line;
    for( ; *p; p++) {
	if (*p == ' ' || *p == '_' || *p == '\t') continue;
	if (*p == '=' || *p == ':') break;
	if (*p >= 'A' && *p <= 'Z') {
	    if (d<buf+len-1)
		*d++ = *p - 'A' + 'a';
	    continue;
	}
	if ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'z') || *p == '.') {
	    if (d<buf+len-1)
		*d++ = *p;
	    continue;
	}
	// Other chars are ignored.
    }
    *d = 0;
    if (*buf == 0) return 0;
    if (*p != ':' && *p != '=') return 0;
    p++;

    while(*p == ' ' || *p == '\t') p++;
    *line = p;
    return 1;
}

LOCAL void
ini_write_section(ini_state_t *st, char * section)
{
    if (!st->write) return;
    if (!st->curr_section || strcmp(st->curr_section, section) != 0) {
	if (st->curr_section) free(st->curr_section);
	st->curr_section = strdup(section);
	fprintf(st->fd, "\n[%s]\n", section);
    }
}

LOCAL void
ini_write_str(ini_state_t *st, char * section, char *fieldname, char *value)
{
    if (!fieldname || !*fieldname) return;
    ini_write_section(st, section);
    fprintf(st->fd, "%s =%s%s\n", fieldname, *value?" ":"", value);
}

LOCAL void
ini_read_str(char * buf, int len, char *value)
{
    snprintf(buf, len, "%s", value);
}

LOCAL void
ini_write_cp437(ini_state_t *st, char * section, char *fieldname, char *value)
{
    if (!fieldname || !*fieldname) return;
    ini_write_section(st, section);
    fprintf(st->fd, "%s =%s", fieldname, *value?" ":"\n");
    if (*value == 0) return;
    while(*value)
	cp437_prt(st->fd, *value++);
    fputc('\n', st->fd);
}

LOCAL void
ini_read_cp437(char * buf, int len, char *value)
{
    int vlen = strlen(value);
    convert_to_cp437(value, &vlen);
    if (vlen >= len) vlen = len-1;
    memcpy(buf, value, vlen);
    buf[vlen] = 0;
}

LOCAL void
ini_write_nbtstr(ini_state_t *st, char * section, char *fieldname, nbtstr_t *value)
{
    ini_write_section(st, section);
    fprintf(st->fd, "%s =%s", fieldname, value->c[0]?" ":"\n");
    if (value->c[0] == 0) return;
    for(int i = 0; value->c[i]; i++)
	cp437_prt(st->fd, value->c[i]);
    fputc('\n', st->fd);
}

LOCAL void
ini_read_nbtstr(nbtstr_t * buf, char *value)
{
    int vlen = strlen(value);
    nbtstr_t t = {0};
    convert_to_cp437(value, &vlen);
    if (vlen >= NB_SLEN) vlen = NB_SLEN-1;
    memcpy(t.c, value, vlen);
    t.c[vlen] = 0;
    *buf = t;
}

LOCAL void
ini_write_intmax(ini_state_t *st, char * section, char *fieldname, intmax_t value)
{
    ini_write_section(st, section);
    fprintf(st->fd, "%s = %jd\n", fieldname, value);
}

LOCAL void
ini_write_int(ini_state_t *st, char * section, char *fieldname, int value)
{
    ini_write_section(st, section);
    fprintf(st->fd, "%s = %d\n", fieldname, value);
}

LOCAL void
ini_write_int_blk(ini_state_t *st, char * section, char *fieldname, int value)
{
    if (value < 0 || value >= BLOCKMAX) return; // Skip illegal values.

    ini_write_section(st, section);
    fprintf(st->fd, "%s = %d\n", fieldname, value);
}

LOCAL void
ini_write_hexint(ini_state_t *st, char * section, char *fieldname, int value)
{
    ini_write_section(st, section);
    if (value < 0)
	fprintf(st->fd, "%s = %d\n", fieldname, value);
    else
	fprintf(st->fd, "%s = 0x%x\n", fieldname, value);
}

LOCAL void
ini_write_bool(ini_state_t *st, char * section, char *fieldname, int value)
{
    if (!fieldname || !*fieldname) return;
    ini_write_section(st, section);
    if (value < 0 || value > 1)
	fprintf(st->fd, "%s = %d\n", fieldname, value);
    else
	fprintf(st->fd, "%s = %s\n", fieldname, value?"true":"false");
}

int
ini_read_bool(char * value)
{
    int rv = 0;
    if (strcasecmp(value, "true") == 0) rv = 1; else
    if (strcasecmp(value, "yes") == 0) rv = 1; else
    if (strcasecmp(value, "on") == 0) rv = 1; else
    if ((*value >= '0' && *value <= '9') || *value == '-')
	rv = atoi(value);
    return rv;
}

LOCAL int
ini_read_int_scale(char * value, int scalefactor, int scalefactor2)
{
    double nval = strtod(value, 0);
    nval = round(nval * scalefactor / scalefactor2);
    return (int)nval;
}

LOCAL void
ini_write_int_scale(ini_state_t *st, char * section, char *fieldname, int value, int scalefactor, int scalefactor2)
{
    ini_write_section(st, section);
    double svalue = (double)value * scalefactor2 / scalefactor;
    int digits = 0, sd = 1;
    while (sd < scalefactor / scalefactor2) { digits++; sd *= 10; }
    fprintf(st->fd, "%s = %.*f\n", fieldname, digits, svalue);
}

static struct time_units_t { char id; int scale; } time_units[] = 
{
    {'s', 1},
    {'m', 60},
    {'h', 3600},
    {'d', 24*3600},
    {'w', 7*24*3600},
    {0, 0}
};

LOCAL int
ini_read_int_duration(char * value)
{
    char * unit = 0;
    int nval = strtod(value, &unit);
    int ch = tolower((uint8_t)*unit);
    for(int i = 0; time_units[i].id; i++)
	if (ch == time_units[i].id) {
	    nval *= time_units[i].scale;
	    break;
	}
    return nval;
}

LOCAL void
ini_write_int_duration(ini_state_t *st, char * section, char *fieldname, int value)
{
    ini_write_section(st, section);
    int unit = 0;
    if (value)
	for(int i = 0; time_units[i].id; i++)
	    if (value/time_units[i].scale*time_units[i].scale == value)
		unit = i;
    fprintf(st->fd, "%s = %d%c\n", fieldname,
	value/time_units[unit].scale, time_units[unit].id);
}

LOCAL time_t
ini_read_int_time_t(char * value)
{
    time_t nval = iso8601_to_time_t(value);
    if (nval == 0)
	nval = strtoimax(value, 0, 0);
    if (nval <= 9999 && strlen(value) > 4) nval = 0;
    return nval;
}

LOCAL void
ini_write_int_time_t(ini_state_t *st, char * section, char *fieldname, time_t value)
{
    ini_write_section(st, section);
    char timebuf[256] = "";
    time_t_to_iso8601(timebuf, value);
    fprintf(st->fd, "%s = %s\n", fieldname, timebuf);
}


#if INTERFACE
#define INI_STRARRAY(_field, _var) \
    do{\
        fld = _field; \
        if (st->all || strcasecmp(fieldname, fld) == 0) { \
	    found = 1; \
            if (!st->write) \
                ini_read_str((_var), sizeof(_var), *fieldvalue); \
            else \
                ini_write_str(st, section, fld, (_var)); \
        } \
    }while(0)

#define INI_STRARRAYCP437(_field, _var) \
    do{\
        fld = _field; \
        if (st->all || strcasecmp(fieldname, fld) == 0) { \
	    found = 1; \
            if (!st->write) \
                ini_read_cp437((_var), sizeof(_var), *fieldvalue); \
            else \
                ini_write_cp437(st, section, fld, (_var)); \
        } \
    }while(0)

#define INI_NBTSTR(_field, _var) \
    do{\
        fld = _field; \
        if (st->all || strcasecmp(fieldname, fld) == 0) { \
	    found = 1; \
            if (!st->write) \
                ini_read_nbtstr(&(_var), *fieldvalue); \
            else \
                ini_write_nbtstr(st, section, fld, &(_var)); \
        } \
    }while(0)

#define INI_INTVAL(_field, _var) \
    do{\
        fld = _field; \
        if (st->all || strcasecmp(fieldname, fld) == 0) { \
	    found = 1; \
            if (!st->write) \
                _var = atoi(*fieldvalue); \
            else \
                ini_write_int(st, section, fld, (_var)); \
        } \
    }while(0)

#define INI_BLKVAL(_field, _var) \
    do{\
        fld = _field; \
        if (st->all || strcasecmp(fieldname, fld) == 0) { \
	    found = 1; \
            if (!st->write) \
                _var = atoi(*fieldvalue); \
            else \
                ini_write_int_blk(st, section, fld, (_var)); \
        } \
    }while(0)

#define INI_INTHEX(_field, _var) \
    do{\
        fld = _field; \
        if (st->all || strcasecmp(fieldname, fld) == 0) { \
	    found = 1; \
            if (!st->write) \
                _var = strtol(*fieldvalue, 0, 0); \
            else \
                ini_write_hexint(st, section, fld, (_var)); \
        } \
    }while(0)

#define INI_FIXEDP(_field, _var, _scale) \
    do{ \
        fld = _field; \
        if (st->all || strcasecmp(fieldname, fld) == 0) { \
	    found = 1; \
            if (!st->write) \
                _var = ini_read_int_scale(*fieldvalue, _scale, 1); \
            else \
                ini_write_int_scale(st, section, fld, (_var), _scale, 1); \
        }\
    }while(0)

#define INI_FIXEDP2(_field, _var, _scale, _scale2) \
    do{\
        fld = _field; \
        if (st->all || strcasecmp(fieldname, fld) == 0) { \
	    found = 1; \
            if (!st->write) \
                _var = ini_read_int_scale(*fieldvalue, _scale, _scale2); \
            else \
                ini_write_int_scale(st, section, fld, (_var), _scale, _scale2); \
        } \
    }while(0)

#define INI_INTMAXVAL(_field, _var) \
    do{\
        fld = _field; \
        if (st->all || strcasecmp(fieldname, fld) == 0) { \
	    found = 1; \
            if (!st->write) \
                _var = strtoimax(*fieldvalue, 0, 0); \
            else \
                ini_write_intmax(st, section, fld, (_var)); \
        } \
    }while(0)

#define INI_DURATION(_field, _var) \
    do{\
        fld = _field; \
        if (st->all || strcasecmp(fieldname, fld) == 0) { \
	    found = 1; \
            if (!st->write) \
                _var = ini_read_int_duration(*fieldvalue); \
            else \
                ini_write_int_duration(st, section, fld, (_var)); \
        } \
    }while(0)

#define INI_TIME_T(_field, _var) \
    do{\
        fld = _field; \
        if (st->all || strcasecmp(fieldname, fld) == 0) { \
	    found = 1; \
            if (!st->write) \
                _var = ini_read_int_time_t(*fieldvalue); \
            else \
                ini_write_int_time_t(st, section, fld, (_var)); \
        } \
    }while(0)

#define INI_BOOLVAL(_field, _var) \
    do{\
        fld = _field; \
        if (st->all || strcasecmp(fieldname, fld) == 0) { \
	    found = 1; \
            if (!st->write) \
                _var = ini_read_bool(*fieldvalue); \
            else \
                ini_write_bool(st, section, fld, (_var)); \
        } \
    }while(0)

#endif
