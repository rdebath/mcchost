
#include "antispam.h"

#if INTERFACE
typedef struct aspam_t aspam_t;
struct aspam_t {
    int64_t ms_start;
    time_t banned_til;
    int32_t ban_flg;
    int32_t events;
    int32_t cheater;
};

#endif

/* Check for anti-spam counters.
 * Return 1 if this event is too soon, 0 otherwise.
 * There are allowed to be spam_count events in spam_time seconds.
 * If you stay below the rate you have spam_time seconds for your connection
 * lag to sort itself out. But if you stay faster it'll trigger.
 *
 * This means the rate can be much lower than a literal implementation of
 * "time between events must not be below N seconds" without false kicking.
 */
int
add_antispam_event(aspam_t * counter, int32_t spam_count, int32_t spam_time, int32_t spam_ban)
{
    if (spam_count <= 0) return 0; // NOPE, not dividing by zero

    if (!shdat.client || shdat.client->user[my_user_no].trusted) return 0;

    int rv = 0;

    struct timeval now;
    gettimeofday(&now, 0);
    if (counter->banned_til) {
	if (now.tv_sec >= counter->banned_til) counter->banned_til = 0;
	else rv = 1;
    }

    int64_t ms_now = (int64_t)now.tv_sec * 1000 + now.tv_usec / 1000;

    int64_t ms_deadline = counter->ms_start
	+ counter->events * (spam_time*1000) / spam_count;

    if (counter->ms_start == 0 || ms_deadline <= ms_now) {
	// nothing outstanding.
	counter->ms_start = ms_now;
	counter->events = 1;
	counter->cheater = 0;
	if (!rv) counter->ban_flg = 0;
	return rv;
    }

    if (counter->events >= spam_count) {
	counter->events -= spam_count;
	counter->ms_start += spam_time * 1000;
	// This checks if the overage is getting larger, generally the
	// "nothing outstanding" clear down should trigger at least every
	// few seconds; especially if the arrivals are uneven.
	if ((ms_deadline-ms_now) * spam_count / (spam_time*1000) > 3)
	    counter->cheater++;
    }
    counter->events++;

    if (ms_deadline < ms_now + spam_time * 1000 && counter->cheater < 3)
	return rv;

    if (counter->banned_til == 0 && spam_ban>0)
	counter->banned_til = now.tv_sec + spam_ban;

    // yesssss.
    if (counter->banned_til == 0 || counter->banned_til < ms_deadline/1000)
	counter->banned_til = ms_deadline/1000;

    return 1;
}
