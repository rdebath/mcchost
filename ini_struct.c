#include <math.h>

#include "ini_struct.h"

#if INTERFACE

typedef int (*ini_func_t)(ini_state_t *st, char * fieldname, char **value);

typedef ini_state_t ini_state_t;
struct ini_state_t {
    FILE * fd;
    ini_file_t * ini;
    char * filename;

    char * curr_section;
    uint8_t all;	// Process all fields
    uint8_t write;	// Set to write fields
    uint8_t quiet;	// Don't comment to the remote (use stderr)
    uint8_t no_unsafe;	// Disable protected fields
    uint8_t looped_read;
};

typedef struct textcolour_t textcolour_t;
struct textcolour_t {
    nbtstr_t name;
    int32_t colour;
    uint8_t a;
    char fallback;
    uint8_t defined;
};
#endif

#define WC(_x) WC2(!st->no_unsafe, _x)
#define WC2(_c, _x) (st->write && (_c)) ?"":_x

userrec_t * user_ini_tgt = 0;
server_ini_t * server_ini_tgt = 0;
textcolour_t textcolour[256];
map_info_t * level_ini_tgt = 0;

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

	server_ini_tgt = &server->shared_ini_settings;
	found |= system_x_ini_fields(st, fieldname, fieldvalue);
	server_ini_tgt = 0;

	INI_INTVAL("MaxPlayers", server->max_players);
	INI_STRARRAY("Localnet", localnet_cidr);

	INI_STRARRAY("Logfile", logfile_pattern);

	INI_DURATION("SaveInterval", server->save_interval);
	INI_DURATION("BackupInterval", server->backup_interval);
	INI_BOOLVAL("NoUnloadMain", server->no_unload_main);
	INI_BOOLVAL("NoSaveInuse", server->no_save_inuse);
	INI_BOOLVAL("UseUTCZone", server->use_utc_zone);

	INI_BOOLVAL("FlagLogCommands", server->flag_log_commands);
	INI_BOOLVAL("FlagLogPlaceCommands", server->flag_log_place_commands);
	INI_BOOLVAL("FlagLogChat", server->flag_log_chat);

	INI_DURATION("AFKInterval", server->afk_interval);
	INI_DURATION("AFKKickInterval", server->afk_kick_interval);
	INI_BOOLVAL("AllowUserLevels", server->allow_user_levels);
	INI_BOOLVAL("AllLevelsOwned", server->all_levels_owned);
	INI_INTVAL("PlayerUpdateMS", server->player_update_ms);
	INI_DURATION("IPConnectDelay", server->ip_connect_delay);

	INI_INTVAL("BlockSpamCount", server->block_spam_count);
	INI_DURATION("BlockSpamInterval", server->block_spam_interval);
	INI_INTVAL("BlockSpamKick", server->block_spam_kick);
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
	INI_BOOLVAL("Private", server_ini_tgt->private);
	INI_BOOLVAL("UseHttpPost", server_ini_tgt->use_http_post);
	INI_BOOLVAL("UseHttp09Arg", server_ini_tgt->use_http_0_9_arg);
	INI_BOOLVAL("UseIPv6", server_ini_tgt->use_ipv6);
	INI_BOOLVAL("OmitSoftwareVersion", server_ini_tgt->omit_software_version);
	INI_BOOLVAL("DisableWebClient", server_ini_tgt->disable_web_client);

	INI_BOOLVAL("NoMapPadding", server_ini_tgt->no_map_padding);
	INI_BOOLVAL("VoidForLogin", server_ini_tgt->void_for_login);
	INI_BOOLVAL("AdminOnlyLogin", server_ini_tgt->admin_only_login);

	INI_STRARRAY("UserSuffix", server_ini_tgt->user_id_suffix);
#ifdef UCMD_PASS
	INI_BOOLVAL("AllowPassVerify", server_ini_tgt->allow_pass_verify);
#endif
	INI_BOOLVAL("DisableSaltLogin", server_ini_tgt->disable_salt_login);
	INI_BOOLVAL("DisallowIPAdmin", server_ini_tgt->disallow_ip_admin);
	INI_BOOLVAL("DisallowIPVerify", server_ini_tgt->disallow_ip_verify);

	// Do not store in ini file
	// INI_BOOLVAL("Runonce", server_ini_tgt->server_runonce);
	// INI_BOOLVAL("Inetd", server_ini_tgt->inetd_mode);
    }

    if (zero_tgt) server_ini_tgt = 0;
    return found;
}

int
level_ini_fields(ini_state_t *st, char * fieldname, char **fieldvalue)
{
    char *section = "", *fld;
    int found = 0;

    map_info_t *tgt = level_prop;
    if (level_ini_tgt) tgt = level_ini_tgt;

    // When writing include a copy of some system stuff.
    // Skip it quietly on read.
    if (st->write) {
	section = "source";
	INI_STRARRAYCP437("Software", server->software);
	INI_STRARRAYCP437("Name", server->name);
	INI_STRARRAYCP437("Motd", server->motd);
	INI_STRARRAYCP437("Level", current_level_name);
	INI_TIME_T("TimeCreated", tgt->time_created);

    } else if (st->curr_section && strcmp("source", st->curr_section) == 0)
	return 1;

    section = "level";
    if (st->all || strcmp(section, st->curr_section) == 0)
    {
	if (!st->no_unsafe) {
	    INI_INTVAL("Size.X", tgt->cells_x);
	    INI_INTVAL("Size.Y", tgt->cells_y);
	    INI_INTVAL("Size.Z", tgt->cells_z);
	} else {
	    int x = 0, y = 0, z = 0;	// Ignore these fields without comment.
	    INI_INTVAL("Size.X", x);
	    INI_INTVAL("Size.Y", y);
	    INI_INTVAL("Size.Z", z);
	}

	if (!st->looped_read) {
	    INI_STRARRAYCP437("Motd", tgt->motd);
	    INI_STRARRAYCP437("Name", tgt->name);
	    INI_STRARRAYCP437("Software", tgt->software);
	    INI_STRARRAYCP437("Theme", tgt->theme);
	    INI_STRARRAYCP437("Seed", tgt->seed);
	    INI_STRARRAYHEX("UUID", tgt->uuid);
	}
	if (!st->write)
	    INI_TIME_T("TimeCreated", tgt->time_created);

	INI_FIXEDP("Spawn.X", tgt->spawn.x, 32);
	INI_FIXEDP("Spawn.Y", tgt->spawn.y, 32);
	INI_FIXEDP("Spawn.Z", tgt->spawn.z, 32);
	INI_FIXEDP2("Spawn.H", tgt->spawn.h, 256, 360);
	INI_FIXEDP2("Spawn.V", tgt->spawn.v, 256, 360);

	INI_FIXEDP("ClickDistance", tgt->click_distance, 32);

	if (!st->write || tgt->mcg_physics_blocks)
	    INI_INTVAL("MCGPhysicsBlocks", tgt->mcg_physics_blocks);

	INI_INTHEX("HacksFlags", tgt->hacks_flags);
	INI_INTVAL("HacksJump", tgt->hacks_jump);

	INI_NBTSTR("Texture", tgt->texname);
	INI_INTVAL("WeatherType", tgt->weather);

	INI_INTHEX("SkyColour", tgt->sky_colour);
	INI_INTHEX("CloudColour", tgt->cloud_colour);
	INI_INTHEX("FogColour", tgt->fog_colour);
	INI_INTHEX("AmbientColour", tgt->ambient_colour);
	INI_INTHEX("SunlightColour", tgt->sunlight_colour);
	INI_INTHEX("SkyboxColour", tgt->skybox_colour);

	INI_INTVAL("SideBlock", tgt->side_block);
	INI_INTVAL("EdgeBlock", tgt->edge_block);
	if (!st->write || tgt->side_level != INT_MIN)
	    INI_INTVAL("SideLevel", tgt->side_level);
	if (!st->write || tgt->side_offset != INT_MIN)
	    INI_INTVAL("SideOffset", tgt->side_offset);
	if (!st->write || tgt->clouds_height != INT_MIN)
	    INI_INTVAL("CloudHeight", tgt->clouds_height);
	INI_INTVAL("MaxFog", tgt->max_fog);

	INI_FIXEDP("CloudsSpeed", tgt->clouds_speed, 256);
	INI_FIXEDP("WeatherSpeed", tgt->weather_speed, 256);
	INI_FIXEDP("WeatherFade", tgt->weather_fade, 128);
	INI_INTVAL("ExpFog", tgt->exp_fog);
	INI_FIXEDP("SkyboxHorSpeed", tgt->skybox_hor_speed, 1024);
	INI_FIXEDP("SkyboxVerSpeed", tgt->skybox_ver_speed, 1024);
    }

    if (!found && !st->write && !st->looped_read) {
	st->looped_read = 1;
	found = mcc_level_ini_fields(st, fieldname, fieldvalue);
	st->looped_read = 0;
    }

    int bn = 0;
    char sectionbuf[128];
    do
    {
	if (st->all) {
	    if (!tgt->blockdef[bn].defined &&
		tgt->blockdef[bn].block_perm == 0) { bn++; continue; }
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
		tgt->blockdef[bn].defined = 1;
	    } else
		break;
	}

	int tf = found;
	found = 0;

	if ( !st->write ||
	     (tgt->blockdef[bn].defined && !tgt->blockdef[bn].no_save)
	) {
	    INI_BOOLVAL(WC("Defined"), tgt->blockdef[bn].defined);
	    INI_BOOLVAL(WC("NoSave"), tgt->blockdef[bn].no_save);
	    INI_NBTSTR("Name", tgt->blockdef[bn].name);
	    INI_INTVAL("Collide", tgt->blockdef[bn].collide);
	    INI_BOOLVAL("TransmitsLight", tgt->blockdef[bn].transmits_light);
	    INI_INTVAL("WalkSound", tgt->blockdef[bn].walksound);
	    INI_BOOLVAL("FullBright", tgt->blockdef[bn].fullbright);
	    int shape = !tgt->blockdef[bn].shape;
	    INI_BOOLVAL("Shape", shape);
	    tgt->blockdef[bn].shape = shape?0:16;
	    INI_INTVAL("Draw", tgt->blockdef[bn].draw);
	    INI_FIXEDP("Speed", tgt->blockdef[bn].speed, 1024);
	    INI_BLKVAL("Fallback", tgt->blockdef[bn].fallback);
	    INI_INTVAL("Texture.Top", tgt->blockdef[bn].textures[0]);
	    INI_INTVAL("Texture.Left", tgt->blockdef[bn].textures[1]);
	    INI_INTVAL("Texture.Right", tgt->blockdef[bn].textures[2]);
	    INI_INTVAL("Texture.Front", tgt->blockdef[bn].textures[3]);
	    INI_INTVAL("Texture.Back", tgt->blockdef[bn].textures[4]);
	    INI_INTVAL("Texture.Bottom", tgt->blockdef[bn].textures[5]);
	    INI_INTVAL("Fog.Den", tgt->blockdef[bn].fog[0]);
	    INI_INTVAL("Fog.R", tgt->blockdef[bn].fog[1]);
	    INI_INTVAL("Fog.G", tgt->blockdef[bn].fog[2]);
	    INI_INTVAL("Fog.B", tgt->blockdef[bn].fog[3]);
	    INI_INTVAL("Min.X", tgt->blockdef[bn].coords[0]);
	    INI_INTVAL("Min.Y", tgt->blockdef[bn].coords[1]);
	    INI_INTVAL("Min.Z", tgt->blockdef[bn].coords[2]);
	    INI_INTVAL("Max.X", tgt->blockdef[bn].coords[3]);
	    INI_INTVAL("Max.Y", tgt->blockdef[bn].coords[4]);
	    INI_INTVAL("Max.Z", tgt->blockdef[bn].coords[5]);

	    INI_BOOLVAL("FastFallFlag", tgt->blockdef[bn].fastfall_flag);

	    INI_BLKVAL("StackBlock", tgt->blockdef[bn].stack_block);
	    INI_BLKVAL("GrassBlock", tgt->blockdef[bn].grass_block);
	    INI_BLKVAL("DirtBlock", tgt->blockdef[bn].dirt_block);
	    INI_BLKVAL("Order", tgt->blockdef[bn].inventory_order);
	}
	if (found && !st->write) tgt->blockdef_generation++;
	found |= tf;

	if (!st->write || (tgt->blockdef[bn].block_perm && !tgt->blockdef[bn].no_save)) {
	    INI_BLKVAL("Permission", tgt->blockdef[bn].block_perm);
	    if (!tgt->blockdef[bn].defined && st->write)
		INI_BOOLVAL("Defined", tgt->blockdef[bn].defined);
	}

	bn++;
    } while(st->all && bn < BLOCKMAX);

    int cn = 0;
    do
    {
	if (st->all) {
	    if (!tgt->cuboid[cn].defined) { cn++; continue; }
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
		tgt->cuboid[cn].defined = 1;
	    } else
		break;
	}

	INI_BOOLVAL(WC("Defined"), tgt->cuboid[cn].defined);
	INI_NBTSTR("Label", tgt->cuboid[cn].name);
	INI_INTVAL("Start.X", tgt->cuboid[cn].start_x);
	INI_INTVAL("Start.Y", tgt->cuboid[cn].start_y);
	INI_INTVAL("Start.Z", tgt->cuboid[cn].start_z);
	INI_INTVAL("End.X", tgt->cuboid[cn].end_x);
	INI_INTVAL("End.Y", tgt->cuboid[cn].end_y);
	INI_INTVAL("End.Z", tgt->cuboid[cn].end_z);
	INI_INTHEX("Colour", tgt->cuboid[cn].colour);
	INI_INTVAL("Opacity", tgt->cuboid[cn].opacity);

	cn++;
    } while(st->all && cn < MAX_CUBES);

    return found;
}

int
mcc_level_ini_fields(ini_state_t *st, char * fieldname, char **fieldvalue)
{
    char *section = "", *fld;
    int found = 0;

    if (st->looped_read && st->write) return 0; // Nope!

    // Info commands use level_ini_tgt
    map_info_t nil_tgt[1], *tgt = level_prop;
    if (level_ini_tgt) tgt = level_ini_tgt;

    add_ini_text_comment(st->ini, "# This file contains metadata about the cw file, the first section");
    add_ini_text_comment(st->ini, "# is loaded when the map is loaded and may be used to override values");
    add_ini_text_comment(st->ini, "# stored in the CW file. The second section is only for information.");

    // These are read/written on iload/isave, in the level.ini and info cmds.
    section = "level";
    if (st->all || strcmp(section, st->curr_section) == 0)
    {
	INI_BOOLVAL("NoUnload", tgt->no_unload);
	INI_BOOLVAL("ReadOnly", tgt->readonly);
	INI_BOOLVAL("DisallowChange", tgt->disallowchange);
	INI_TIME_T("LastBackup", tgt->last_backup);
	INI_BOOLVAL("ResetHotbar", tgt->reset_hotbar);
	INI_BOOLVAL("LevelChat", tgt->level_chat);
	INI_BOOLVAL("AnnounceMotd", tgt->announce_motd);
	INI_BOOLVAL("OtherUserOp", tgt->other_user_op);
	INI_STRARRAY("LevelOwnerList", tgt->op_user_list);

	if (!st->write)
	    INI_BOOLVAL("DirtySave", tgt->dirty_save);
    }

    // These are only written to the level.ini and used by info load
    section = "level";
    if (st->all || strcmp(section, st->curr_section) == 0)
    {
	map_info_t *utgt = tgt;

	if (!st->write && !level_ini_tgt) tgt = nil_tgt;
	if (st->no_unsafe || st->looped_read) tgt = nil_tgt;

	INI_TIME_T("TimeCreated", tgt->time_created);
	add_ini_text_comment(st->ini, "");
	add_ini_text_comment(st->ini, "# These fields are used by /minfo only");

	INI_TIME_T("LastModified", tgt->last_modified);
	INI_TIME_T("LastLoaded", tgt->last_loaded);
	INI_INTVAL("Size.X", tgt->cells_x);
	INI_INTVAL("Size.Y", tgt->cells_y);
	INI_INTVAL("Size.Z", tgt->cells_z);

	INI_STRARRAYCP437("Software", tgt->software);
	INI_STRARRAYCP437("Theme", tgt->theme);
	INI_STRARRAYCP437("Seed", tgt->seed);
	INI_NBTSTR("Texture", tgt->texname);
	tgt = utgt;
    }

    if (!found && !st->write && !st->looped_read) {
	st->looped_read = 1;
	found = level_ini_fields(st, fieldname, fieldvalue);
	st->looped_read = 0;
    }

    return found;
}

int
user_ini_fields(ini_state_t *st, char * fieldname, char **fieldvalue)
{
    char *section = "", *fld;
    int found = 0;

    if (!user_ini_tgt) return found;

    // If INI_STRPTR item is missing in the file it needs to be freed.
    if (!st->write && strcmp("#", st->curr_section) == 0) {
	if (user_ini_tgt->ignore_list) {
	    free(user_ini_tgt->ignore_list);
	    user_ini_tgt->ignore_list = 0;
	}
    }

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
	INI_INTVAL("Banned", user_ini_tgt->banned);
	if (user_ini_tgt->user_group < 0 || user_ini_tgt->banned || !st->write)
	    INI_STRARRAYCP437("BanMessage", user_ini_tgt->ban_message);
	if (user_ini_tgt->user_group < 0) {
	    user_ini_tgt->user_group = 1;
	    user_ini_tgt->banned = 1;
	    user_ini_tgt->ini_dirty = 1;
	}

	INI_STRARRAYCP437("Nick", user_ini_tgt->nick);
	INI_STRARRAYCP437("Colour", user_ini_tgt->colour);
	INI_STRARRAYCP437("Skin", user_ini_tgt->skin);
	INI_STRARRAYCP437("Model", user_ini_tgt->model);
	INI_STRARRAYCP437("Title", user_ini_tgt->title);
	INI_STRARRAYCP437("TitleColour", user_ini_tgt->title_colour);

	INI_TIME_T("FirstLogon", user_ini_tgt->first_logon);
	INI_TIME_T("LastLogon", user_ini_tgt->last_logon);
	INI_INTMAXVAL("LogonCount", user_ini_tgt->logon_count);
	INI_DURATION("TimeOnline", user_ini_tgt->time_online_secs);

	if (user_ini_tgt->click_distance >= 0 || !st->write)
	    INI_FIXEDP("ClickDistance", user_ini_tgt->click_distance, 32);
	INI_STRARRAY("LastIP", user_ini_tgt->last_ip);
	INI_STRARRAY("Timezone", user_ini_tgt->timezone);

	if (user_ini_tgt->saveable) {
	    INI_STRPTR("IgnoreList", user_ini_tgt->ignore_list);
	} else {
	    char tbuf[16] = {0};
	    INI_STRARRAY("IgnoreList", tbuf);
	}

	INI_INTMAXVAL("BlocksPlaced", user_ini_tgt->blocks_placed);
	INI_INTMAXVAL("BlocksDeleted", user_ini_tgt->blocks_deleted);
	INI_INTMAXVAL("BlocksDrawn", user_ini_tgt->blocks_drawn);

	INI_INTMAXVALNZ("MessageCount", user_ini_tgt->message_count);
	INI_INTMAXVALNZ("KickCount", user_ini_tgt->kick_count);
	INI_INTMAXVALNZ("DeathCount", user_ini_tgt->death_count);
	INI_INTMAXVALNZ("CoinCount", user_ini_tgt->coin_count);

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
    INI_INTMAXVAL("OtherCmdDisabled", command_limits.other_cmd_disabled);

    section = "cmdset";
    if (st->all || strcmp(section, st->curr_section) == 0)
    {
	for(int i = 0; command_list[i].name; i++) {
	    if(i>0 && !command_list[i].nodup && command_list[i].function == command_list[i-1].function)
		continue;

	    if (!st->write) {
		// Reading
		if (!st->all && strcmp(command_list[i].name, fieldname) != 0)
		    continue;

		INI_INTVAL(command_list[i].name, command_list[i].perm_okay);
		command_list[i].perm_set = 1;
		if (command_list[i].perm_okay == -1)
		    command_list[i].perm_okay = command_list[i].perm_def;
	    } else {
		if (command_limits.other_cmd_disabled == 1 && command_list[i].perm_okay == perm_token_disabled) {
		    add_ini_txt_line(st->ini, section, command_list[i].name, 0);
		} else
		if (command_list[i].perm_okay != command_list[i].perm_def) {
		    INI_INTVAL(command_list[i].name, command_list[i].perm_okay);
		} else if (command_limits.other_cmd_disabled != 0) {
		    int is_default = -1;
		    INI_INTVAL(command_list[i].name, is_default);
		} else
		    add_ini_txt_line(st->ini, section, command_list[i].name, 0);
	    }
	}
    }

    return found;
}

int
save_ini_file(ini_func_t filetype, char * filename)
{
    ini_file_t ini[1] = {0};
    load_ini_txt_file(ini, filename, 1);

    ini_state_t st = (ini_state_t){.all=1, .write=1, .ini=ini};
    filetype(&st,0,0);

    int rv = save_ini_txt_file(ini, filename);
    clear_ini_txt(ini);
    return rv;
}

int
load_ini_file(ini_func_t filetype, char * filename, int quiet, int no_unsafe)
{
    ini_file_t ini[1] = {0};
    int rv = 0;
    if ((rv = load_ini_txt_file(ini, filename, quiet?3:2)) < 0) {
	clear_ini_txt(ini);
	return rv;
    }

    ini_state_t st[1] = {{.quiet = quiet, .no_unsafe=no_unsafe, .filename = filename}};

    {
	char ibuf[BUFSIZ], *p = ibuf;
	*ibuf = 0;
	// Mark the start of the load to free INI_STRPTR items.
	st->curr_section = "#";
	(void)filetype(st, "#", &p);
	st->curr_section = "";
    }

    for(int lno = 0; lno < ini->count; lno++) {
	if ((ini->lines[lno].line_type & ini_section) == ini_section) {
	    st->curr_section = ini->lines[lno].section;
	}
	if ((ini->lines[lno].line_type & ini_item) == ini_item) {

	    char * label = ini->lines[lno].name;
	    char * value = ini->lines[lno].value;
	    if (!st->curr_section || !filetype(st, label, &value))
	    {
		if (st->quiet) {
		    printlog("Unknown item \"%s\" in file \"%s\" section \"%s\" -- label \"%s\" value \"%s\"",
			ini->lines[lno].text_line, st->filename, st->curr_section?:"-", label, value);
		} else
		    printf_chat("#&WUnknown item&S \"%s\" section \"%s\"",
			ini->lines[lno].text_line, st->curr_section?:"-");
		rv = 1;
	    }

	}
    }

    st->curr_section = 0;
    return rv;
}

LOCAL void
ini_write_str(ini_state_t *st, char * section, char *fieldname, char *value)
{
    if (!fieldname || !*fieldname) return;
    if (st->ini)
	add_ini_txt_line(st->ini, section, fieldname, value);
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
    if (st->ini) {
	char ubuf[NB_SLEN*2*3+16];
	convert_to_utf8(ubuf, sizeof(ubuf), value);
	add_ini_txt_line(st->ini, section, fieldname, ubuf);
    }
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
    if (st->ini) {
	char ubuf[NB_SLEN*3+16];
	convert_to_utf8(ubuf, sizeof(ubuf), value->c);
	add_ini_txt_line(st->ini, section, fieldname, ubuf);
    }
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
ini_write_bytes(ini_state_t *st, char * section, char *fieldname, char *value, int len)
{
    if (!fieldname || !*fieldname) return;
    if (st->ini) {
	char hexbuf[32*2+3];
	for(int i=0; i<len && i<32; i++)
	    sprintf(hexbuf+i*2, "%02x", (uint8_t)(*value++));
	add_ini_txt_line(st->ini, section, fieldname, hexbuf);
    }
}

LOCAL void
ini_read_bytes(char * buf, int len, char *value)
{
    char * vp = value;
    for(int i=0; i<len; i++) {
	char vbuf[4] = "00";
	while (*vp == '-') vp++;
	if (*vp && vp[1]) {
	    vbuf[0] = *vp++;
	    vbuf[1] = *vp++;
	}
	buf[i] = strtol(vbuf, 0, 16);
    }
}

LOCAL void
ini_write_strptr(ini_state_t *st, char * section, char *fieldname, char **value)
{
    char * s = value?*value:"";
    if (s && st->ini)
	add_ini_txt_line(st->ini, section, fieldname, s);
}

LOCAL void
ini_read_strptr(char ** buf, char *value)
{
    if (!buf) return;
    if (*buf) free(*buf);
    if (*value == 0) *buf = 0;
    else *buf = strdup(value);
}

LOCAL void
ini_write_intmax(ini_state_t *st, char * section, char *fieldname, intmax_t value)
{
    if (st->ini) {
	char sbuf[sizeof(uintmax_t)*3+3];
	sprintf(sbuf, "%jd", value);
	add_ini_txt_line(st->ini, section, fieldname, sbuf);
    }
}

LOCAL void
ini_write_int(ini_state_t *st, char * section, char *fieldname, int value)
{
    if (st->ini) {
	char sbuf[sizeof(int)*3+3];
	sprintf(sbuf, "%d", value);
	add_ini_txt_line(st->ini, section, fieldname, sbuf);
    }
}

LOCAL void
ini_write_int_blk(ini_state_t *st, char * section, char *fieldname, int value)
{
    if (value < 0 || value >= BLOCKMAX) return; // Skip illegal values.

    if (st->ini) {
	char sbuf[sizeof(int)*3+3];
	sprintf(sbuf, "%d", value);
	add_ini_txt_line(st->ini, section, fieldname, sbuf);
    }
}

LOCAL void
ini_write_hexint(ini_state_t *st, char * section, char *fieldname, int value)
{
    if (st->ini) {
	char sbuf[sizeof(int)*3+5];
	sprintf(sbuf, "%d", value);
	if (value < 0)
	    sprintf(sbuf, "%d", value);
	else
	    sprintf(sbuf, "0x%x", value);
	add_ini_txt_line(st->ini, section, fieldname, sbuf);
    }
}

LOCAL void
ini_write_bool(ini_state_t *st, char * section, char *fieldname, int value)
{
    if (!fieldname || !*fieldname) return;
    if (st->ini) {
	char sbuf[sizeof(int)*3+5];
	sprintf(sbuf, "%d", value);
	if (value < 0 || value > 1)
	    sprintf(sbuf, "%d", value);
	else
	    sprintf(sbuf, "%s", value?"true":"false");
	add_ini_txt_line(st->ini, section, fieldname, sbuf);
    }
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
    double svalue = (double)value * scalefactor2 / scalefactor;
    int digits = 0, sd = 1;
    while (sd < scalefactor / scalefactor2) { digits++; sd *= 10; }
    if (st->ini) {
	char sbuf[512];
	sprintf(sbuf, "%.*f", digits, svalue);
	add_ini_txt_line(st->ini, section, fieldname, sbuf);
    }
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
    int unit = 0;
    if (value)
	for(int i = 0; time_units[i].id; i++)
	    if (value/time_units[i].scale*time_units[i].scale == value)
		unit = i;
    if (st->ini) {
	char sbuf[sizeof(int)*3+8];
	sprintf(sbuf, "%d%c", value/time_units[unit].scale, time_units[unit].id);
	add_ini_txt_line(st->ini, section, fieldname, sbuf);
    }
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
    char timebuf[256] = "";
    time_t_to_iso8601(timebuf, value);
    if (st->ini)
	add_ini_txt_line(st->ini, section, fieldname, timebuf);
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

#define INI_STRARRAYHEX(_field, _var) \
    do{\
        fld = _field; \
        if (st->all || strcasecmp(fieldname, fld) == 0) { \
	    found = 1; \
            if (!st->write) \
                ini_read_bytes((_var), sizeof(_var), *fieldvalue); \
            else \
                ini_write_bytes(st, section, fld, (_var), sizeof(_var)); \
        } \
    }while(0)

#define INI_STRPTR(_field, _var) \
    do{\
        fld = _field; \
        if (st->all || strcasecmp(fieldname, fld) == 0) { \
	    found = 1; \
            if (!st->write) \
                ini_read_strptr(&(_var), *fieldvalue); \
            else \
                ini_write_strptr(st, section, fld, &(_var)); \
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

#define INI_INTMAXVALNZ(_field, _var) \
    do{\
        fld = _field; \
        if (st->all || strcasecmp(fieldname, fld) == 0) { \
	    found = 1; \
            if (!st->write) \
                _var = strtoimax(*fieldvalue, 0, 0); \
            else if (_var) \
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
