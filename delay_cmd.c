
#include "delay_cmd.h"

#if INTERFACE
typedef void (*void_func)(void);

typedef struct delay_task_t delay_task_t;
struct delay_task_t {
    void_func to_run;
    time_t when;
};

#define MAX_DELAY_TASK	42
#endif

delay_task_t delay_queue[MAX_DELAY_TASK];
int active_delay_task_top = 0;

void
schedule_task(int in_secs, void_func what)
{
    if (in_secs < 0 || what == 0) return;
    for(int i = 0; i<MAX_DELAY_TASK; i++) {
	if (delay_queue[i].to_run == 0) {
	    delay_queue[i].to_run = what;
	    delay_queue[i].when = time(0) + in_secs;
	    if (i >= active_delay_task_top) active_delay_task_top = i+1;
	    return;
	}
    }
    printlog("WARNING: Delay task queue overflow 0x%08jx, %d", (uintmax_t)(intptr_t)what, (int)in_secs);
}

void
run_delay_task()
{
    time_t now = time(0);
    int ran = 0;
    for(int i = 0; i<active_delay_task_top; i++)
    {
	if (delay_queue[i].to_run != 0 && delay_queue[i].when < now) {
	    void_func what = delay_queue[i].to_run;
	    delay_queue[i].to_run = 0;
	    what();
	    ran = 1;
	}
    }
    if (ran) {
	int t = active_delay_task_top;
	active_delay_task_top = 0;
	for(int i = 0; i<t; i++)
	    if (delay_queue[i].to_run != 0)
		if (i >= active_delay_task_top) active_delay_task_top = i+1;
    }
}

