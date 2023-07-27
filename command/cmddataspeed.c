
#include "cmddataspeed.h"

/*HELP dataspeed H_CMD
&T/dataspeed
How much data is the server moving?
*/

#if INTERFACE
#define UCMD_DATASPEED {N"dataspeed", &cmd_dataspeed}
#endif

static
struct pkt_saved_t {
    uint64_t packets_rcvd;
    uint64_t packets_sent;
    uint64_t bytes_rcvd;
    uint64_t bytes_sent;
    struct timeval when;
} saved_pos;

void
cmd_dataspeed(char * UNUSED(cmd), char * UNUSED(arg))
{
    printf_chat("Just a moment.");

    ds_save_state();
    schedule_task(2, ds_say_since_saved);
}

void
ds_save_state()
{
    saved_pos.packets_rcvd = server->packets_rcvd;
    saved_pos.packets_sent = server->packets_sent;
    saved_pos.bytes_rcvd = server->bytes_rcvd;
    saved_pos.bytes_sent = server->bytes_sent;
    gettimeofday(&saved_pos.when, 0);
}

void
ds_say_since_saved()
{
    struct timeval now;
    gettimeofday(&now, 0);

    double interval = ((now.tv_sec*1000000.0+now.tv_usec)
	- (saved_pos.when.tv_sec*1000000.0+saved_pos.when.tv_usec))/1000000.0;

    double p_sent = (server->packets_sent - saved_pos.packets_sent)/interval;
    double p_rcvd = (server->packets_rcvd - saved_pos.packets_rcvd)/interval;
    double b_sent = (server->bytes_sent - saved_pos.bytes_sent)/interval;
    double b_rcvd = (server->bytes_rcvd - saved_pos.bytes_rcvd)/interval;

    printf_chat("Sent: %0.2f p/s (%0.0f b/s), Rcvd: %0.0f p/s (%0.2f b/s)",
	p_sent, b_sent, p_rcvd, b_rcvd);

    int users = 0;
    for(int i=0; i<MAX_USER; i++)
        if (shdat.client->user[i].state.active == 1)
            users ++;

    printf_chat("Total of %d users", users);
}
