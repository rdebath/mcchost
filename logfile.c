
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <ctype.h>
#include <sys/time.h>

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
    logfile_raw = israw;
}

LOCAL void
open_logfile()
{
    if (!file_name) return;

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
    // Sigh: setlinebuf(logfile);
    setvbuf(logfile, NULL, _IOLBF, 0);
    free(fname);
}

LOCAL void
check_reopen_logfile(struct tm * tm)
{
    if (!file_name) return;
    if (logfile)
	if (file_tm.tm_year == tm->tm_year &&
	    file_tm.tm_mon == tm->tm_mon &&
	    file_tm.tm_mday == tm->tm_mday)
	    return;

    open_logfile();
}

void
close_logfile()
{
    if (logfile)
	fclose(logfile);
    logfile = 0;
}

void
log_chat_message(char * str, int len)
{
    if (!logfile && !file_name) return;

    int cf = 0;
    time_t now;
    time(&now);
    struct tm * tm = localtime(&now);

    check_reopen_logfile(tm);

    fprintf(logfile, "%04d-%02d-%02d %02d:%02d:%02d ",
	tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
	tm->tm_hour, tm->tm_min, tm->tm_sec);

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

void
printlog(char * fmt, ...)
{
    if (!logfile && !file_name) return;

    va_list ap;

    time_t now;
    time(&now);
    struct tm * tm = localtime(&now);

    check_reopen_logfile(tm);

    fprintf(logfile, "%04d-%02d-%02d %02d:%02d:%02d ",
        tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
        tm->tm_hour, tm->tm_min, tm->tm_sec);

    va_start(ap, fmt);
    vfprintf(logfile, fmt, ap);
    va_end(ap);

    fprintf(logfile, "\n");
}

void
fprintf_logfile(char * fmt, ...)
{
    if (!logfile && !file_name) return;

    char bufcp437[4096], bufutf8[8192];

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(bufcp437, sizeof(bufcp437), fmt, ap);
    va_end(ap);

    convert_to_utf8(bufutf8, sizeof(bufutf8), bufcp437);

    time_t now;
    time(&now);
    struct tm * tm = localtime(&now);

    check_reopen_logfile(tm);

    fprintf(logfile, "%04d-%02d-%02d %02d:%02d:%02d %s\n",
        tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
        tm->tm_hour, tm->tm_min, tm->tm_sec,
	bufutf8);
}
