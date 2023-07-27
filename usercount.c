
#include "usercount.h"

int
current_user_count()
{
    int flg = (shdat.client == 0);
    if(flg) open_client_list();
    if (!shdat.client) return 0;
    int users = 0;
    for(int i=0; i<MAX_USER; i++) {
	if (shdat.client->user[i].state.active == 1)
	    users ++;
    }
    if(flg) stop_client_list();
    return users;
}

int
unique_ip_count()
{
    int flg = (shdat.client == 0);
    if(flg) open_client_list();
    if (!shdat.client) return 0;

    // No need to lock -- unimportant statistic.
    int users = 0;
    int ip_addrs = 0;

    for(int i=0; i<MAX_USER; i++)
	shdat.client->user[i].ip_dup = 0;

    for(int i=0; i<MAX_USER; i++) {
	if (shdat.client->user[i].state.active == 1) {
	    users ++;
	    if (shdat.client->user[i].ip_dup == 0) {
		ip_addrs ++;
		for(int j = i+1; j<MAX_USER; j++) {
		    if (shdat.client->user[j].state.active == 1 &&
			shdat.client->user[i].ip_address ==
			shdat.client->user[j].ip_address)
		    {
			shdat.client->user[j].ip_dup = 1;
		    }
		}
	    }
	}
    }

    if(flg) stop_client_list();
    return ip_addrs;
}
