
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <sys/time.h>

#include "logfile.h"

static FILE * logfile = 0;
static int logfile_raw;
static char * file_name = "log/%s.log";
static struct tm file_tm;

void
set_logfile(char * logfile_name)
{
    file_name = logfile_name;
}

void
open_logfile()
{
    if (!file_name) return;

    if (strstr(file_name, "%s") != 0) {
	reopen_logfile();
    } else {
	if (logfile) fclose(logfile);
	logfile = fopen(file_name, "a");
	setlinebuf(logfile);
    }
}

LOCAL void
reopen_logfile()
{
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
    setlinebuf(logfile);
    free(fname);
}

LOCAL void
check_reopen_logfile(struct tm * tm)
{
    if (!file_name) return;
    if (file_tm.tm_year == tm->tm_year &&
        file_tm.tm_mon == tm->tm_mon &&
        file_tm.tm_mday == tm->tm_mday)
	return;

    reopen_logfile();
}

void
close_logfile()
{
    if (logfile)
	fclose(logfile);
    logfile = 0;
}

void
write_logfile(char * str, int len)
{
    if (!logfile) return;

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
	    if (isascii(str[i]) && isxdigit(str[i]))
		continue;
	    fputc('&', logfile);
	} else if (str[i] == '&' && !logfile_raw)
	    cf = 1;
	else if (str[i] >= ' ' && str[i] <= '~')
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
print_logfile(char * s)
{
    if (!logfile) return;

    time_t now;
    time(&now);
    struct tm * tm = localtime(&now);

    check_reopen_logfile(tm);

    fprintf(logfile, "%04d-%02d-%02d %02d:%02d:%02d %s\n",
        tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
        tm->tm_hour, tm->tm_min, tm->tm_sec,
        s);
}

void
cp437_prt(FILE* ofd, int ch)
{
    int uc = cp437rom[ch & 0xFF];
    int c1, c2, c3;

    c2 = (uc/64);
    c1 = uc - c2*64;
    c3 = (c2/64);
    c2 = c2 - c3 * 64;
    if (uc < 2048)
        fprintf(ofd, "%c%c", c2+192, c1+128);
    else if (uc < 65536)
        fprintf(ofd, "%c%c%c", c3+224, c2+128, c1+128);
    else
        fprintf(ofd, "%c%c%c", 0xef, 0xbf, 0xbd);
}
