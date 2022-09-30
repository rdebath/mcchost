

#include "mktime_t.h"

void
time_t_to_iso8601(char * timebuf, time_t t)
{
    struct tm tm;
    gmtime_r(&t, &tm);
    sprintf(timebuf, "%04d-%02d-%02dT%02d:%02d:%02dZ",
	tm.tm_year+1900, tm.tm_mon, tm.tm_mday,
	tm.tm_hour, tm.tm_min, tm.tm_sec);
}

time_t
iso8601_to_time_t(const char * iso8601_datetime)
{
    // Format should be: "YYYY-MM-DDTHH:MM:SSZ" no others allowed.
    if (!iso8601_datetime || strlen(iso8601_datetime) < 19) return 0;
    if (strlen(iso8601_datetime) > 30) return 0;
    char isodt[32];
    strcpy(isodt, iso8601_datetime);
    // Allow "local time zone" of UTC.
    if (isodt[19] != 0 && isodt[19] != 'z' && isodt[19] != 'Z') return 0;
    if (isodt[4] != '-' || isodt[7] != '-' || isodt[13] != ':' || isodt[16] != ':')
	return 0;
    // Slice it
    isodt[4]=isodt[7]=isodt[10]=isodt[13]=isodt[16]=isodt[19]=0;
    struct tm tm = {0};
    tm.tm_year = atoi(isodt+0) - 1900;
    tm.tm_mon = atoi(isodt+5);
    tm.tm_mday = atoi(isodt+8);
    tm.tm_hour = atoi(isodt+11);
    tm.tm_min = atoi(isodt+14);
    tm.tm_sec = atoi(isodt+17);
    // Dice it
    return mktime_t(&tm);
}

#define IsDivBy(y,n) (((y) % (n)) == 0)
#define IsLY(y) (IsDivBy((y),4) && (!IsDivBy((y),100) || IsDivBy((y),400)))
#define NumLY(y)  (((y) - 1969) / 4 - ((y) - 1901) / 100 + ((y) - 1601) / 400)

/* mktime cannot be reliably forced to use UTC, this only does UTC. */
LOCAL time_t
mktime_t(struct tm *tm)
{
static int mdays[] = {31,28,31,30,31,30,31,31,30,31,30,31};

    int years   = tm->tm_year + 1900;
    int months  = tm->tm_mon;
    int days    = tm->tm_mday - 1;
    int hours   = tm->tm_hour;
    int minutes = tm->tm_min;
    int seconds = tm->tm_sec;
    // tm->tm_isdst cannot be true for UTC.
    // tm_wday and tm_yday are ignored as duplication.

    // This normalises the year and month values.
    // Normalising the other fields happens automatically (except year zero).
    if (months>11 || months<0) {
	int mon = months % 12;
	if (mon < 0) mon += 12; // Need Euclidian division
	years += (months-mon) / 12;
	months = mon;
    }

    if ((months > 1) && IsLY(years)) ++days;
    while (months-- > 0) days += mdays[months];
    days += (365 * (years - 1970)) + NumLY(years);
    return (((time_t)days * 24 + hours) * 60 + minutes) * 60 + seconds;
}
