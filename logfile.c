

#include "logfile.h"

/*
 * Logging, I want the logfile to be in UTF-8 so any CP437 strings need to
 * be converted. As the actual messages are in ASCII this is not a problem.
 *
 * The fprintf_logfile function converts it's arguments from CP437 to UTF-8
 * Function log_chat_message does CP437 converstion AND chat colour removal.
 *
 * The printlog function does NOT do any conversion.
 */

static FILE * logfile = 0;
static int logfile_raw;
static char * file_name = "log/%s.log";
static struct tm file_tm;

void
set_logfile(char * logfile_name, int israw)
{
    file_name = logfile_name;
    if (file_name && *file_name == 0) file_name = 0;
    logfile_raw = israw;
}

LOCAL void
open_logfile()
{
    if (log_to_stderr || !file_name) { logfile = stderr; return; }

    char * fname = malloc(strlen(file_name) + 16);
    strcpy(fname, file_name);
    char * p = strstr(fname, "%s");
    if (p) {
	time_t now;
	time(&now);
	file_tm = *localtime(&now);
	
	sprintf(p, "%04d-%02d-%02d",
	    file_tm.tm_year+1900, file_tm.tm_mon+1, file_tm.tm_mday);

	strcat(fname, file_name+(p-fname)+2);
    }

    if (logfile) fclose(logfile);
    logfile = fopen(fname, "a");
    // setlinebuf(logfile); // Sigh: BSD not Posix
    setvbuf(logfile, NULL, _IOLBF, 0);

    // Create a constant name for the most recent logfile (probably)
    char * basename = strrchr(fname, '/');
    if (basename && basename[1]) basename++; else basename = fname;
    (void)unlink(LOGFILE_SYMLINK_NAME);
    (void)symlink(basename, LOGFILE_SYMLINK_NAME);

    free(fname);
}

LOCAL int
check_reopen_logfile(struct tm * tm)
{
    if (logfile)
	if (file_tm.tm_year == tm->tm_year &&
	    file_tm.tm_mon == tm->tm_mon &&
	    file_tm.tm_mday == tm->tm_mday)
	    return 1;

    open_logfile();
    return logfile != 0;
}

void
close_logfile()
{
    if (log_to_stderr || !file_name) return;

    if (logfile)
	fclose(logfile);
    logfile = 0;
}

void
log_chat_message(char * str, int len, int where, int to_id, int type)
{
    if (!server || !server->flag_log_chat) return;

    int cf = 0;
    time_t now;
    time(&now);
    struct tm * tm = localtime(&now);

    if (!check_reopen_logfile(tm)) return;

    if (type == 100) return; // Will be logged as a command.

    if (!log_to_stderr)
	fprintf(logfile, "%04d-%02d-%02d %02d:%02d:%02d ",
	    tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
	    tm->tm_hour, tm->tm_min, tm->tm_sec);

    if (type == 100)
	fprintf(logfile, "%s announced: ", user_id);
    if (type == -1)
	fprintf(logfile, "%s couldn't say: ", user_id);
    if (where == 1 && to_id >= 0 && to_id < MAX_USER) {
	if (shdat.client && shdat.client->user[to_id].active)
	    fprintf(logfile, "@%s ", shdat.client->user[to_id].name.c);
    }

    for(int i=0; i<len; i++) {
	if (cf) {
	    cf = 0;
	    if (isascii(str[i]) && isalnum(str[i]))
		continue;
	    fputc('&', logfile);
	} else if (str[i] == '&' && !logfile_raw) {
	    cf = 1; continue;
	}
	if (str[i] >= ' ' && str[i] <= '~')
	    fputc(str[i], logfile);
	else if (!logfile_raw)
	    cp437_prt(logfile, str[i]);
	else if (str[i] != '\n')
	    fputc(str[i], logfile);
	else
	    fputc(' ', logfile); // Something.
    }
    fprintf(logfile, "\n");
}

#if INTERFACE
#define printlog_w __attribute__ ((format (printf, 1, 2)))
#endif

void printlog_w
printlog(char * fmt, ...)
{
    va_list ap;

    time_t now;
    time(&now);
    struct tm * tm = localtime(&now);

    if (!check_reopen_logfile(tm)) return;

    if (!log_to_stderr)
	fprintf(logfile, "%04d-%02d-%02d %02d:%02d:%02d ",
	    tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
	    tm->tm_hour, tm->tm_min, tm->tm_sec);

    va_start(ap, fmt);
    vfprintf(logfile, fmt, ap);
    va_end(ap);

    fprintf(logfile, "\n");
}

#if INTERFACE
#define fprintf_logfile_w __attribute__ ((format (printf, 1, 2)))
#endif

void fprintf_logfile_w
fprintf_logfile(char * fmt, ...)
{
    char bufcp437[4096], bufutf8[8192];

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(bufcp437, sizeof(bufcp437), fmt, ap);
    va_end(ap);

    convert_to_utf8(bufutf8, sizeof(bufutf8), bufcp437);

    time_t now;
    time(&now);
    struct tm * tm = localtime(&now);

    if (!check_reopen_logfile(tm)) return;

    if (!log_to_stderr)
	fprintf(logfile, "%04d-%02d-%02d %02d:%02d:%02d %s\n",
	    tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
	    tm->tm_hour, tm->tm_min, tm->tm_sec,
	    bufutf8);
    else
	fprintf(logfile, "%s\n", bufutf8);
}
