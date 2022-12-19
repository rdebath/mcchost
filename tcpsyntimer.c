
#include "tcpsyntimer.h"

#if INTERFACE
#define IPCOUNT 16
#define TRYCOUNT 4

typedef struct ip_logger_t ip_logger_t;
struct ip_logger_t {
    int32_t ipaddr;
    time_t delay_til;
    time_t delay_queue[TRYCOUNT];
};
#endif

ip_logger_t slow_connect[IPCOUNT] = {0};

int
allow_connection()
{
    if ((client_trusted && !ini_settings->disallow_ip_verify) || server->ip_connect_delay <= 0) return 1;

    time_t now = time(0);
    int slotno = 0;
    if (client_ipv4_addr != 0)
	for(int i = 1; i<IPCOUNT; i++) {
	    if (slow_connect[i].ipaddr == client_ipv4_addr) {
		slotno = i;
		break;
	    }
	    if (slotno) continue;
	    if (now > slow_connect[i].delay_til)
		slotno = i;
	}

    if (now <= slow_connect[slotno].delay_til) {
	slow_connect[slotno].delay_til = now + server->ip_connect_delay;
	return 0;
    }

    time_t setto = now;
    for(int i = 0; i<TRYCOUNT; i++) {
	if (slow_connect[slotno].delay_queue[i] > setto)
	    setto = slow_connect[slotno].delay_queue[i];
    }

    for(int i = 0; i<TRYCOUNT; i++) {
	if (slow_connect[slotno].delay_queue[i] <= now) {
	    slow_connect[slotno].delay_queue[i] = setto + server->ip_connect_delay;
	    return 1;
	}
    }

    slow_connect[slotno].ipaddr = client_ipv4_addr;
    slow_connect[slotno].delay_til = setto + server->ip_connect_delay;
    return 0;
}
