#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <limits.h>

#include "secret.h"

void
generate_secret()
{
    struct timeval now;
    gettimeofday(&now, 0);
#ifdef PCG32_INITIALIZER
    // Somewhat better random seed, the whole time, pid and ASLR
    pcg32_srandom(
	now.tv_sec*(uint64_t)1000000 + now.tv_usec,
	(((uintptr_t)&process_args) >> 12) +
	((int64_t)(getpid()) << sizeof(uintptr_t)*4) );

// NB: for x86/x64
//              0x88000888
// On 32bit     0x99XXX000 --> Only *8*bits of ASLR
// On 64bit 0x91XXXXXXX000 --> 28bits of ASLR
//      0x8888800000000888
#else
    // A pretty trivial semi-random code, maybe 24bits of randomness.
    srandom(now.tv_sec ^ (now.tv_usec*4294U));
#endif

    static char base62[] =
	"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    for(int i=0; i<16; i++) {
#ifdef PCG32_INITIALIZER
	int ch = pcg32_boundedrand(62);
#else
	int ch = random() % 62;
#endif
	server->secret[i] = base62[ch];
    }
    server->secret[16] = 0;
    fprintf(stderr, "Generated server secret %s\n", server->secret);
}

int
check_mppass(char * mppass)
{
    for(int tick = -1; tick<2; tick++)
    {
	// NB: Not vulnerable to length extension attacks due to previous
	// checks on user_id character set and length. (ie NULs not allowed)
	char sbuf[NB_SLEN];
	unsigned char * s;
	MD5_CTX mdContext;
	MD5Init (&mdContext);
	if (tick < 0)
	    s = (unsigned char *)server->secret;
	else {
	    convert_secret(sbuf, heartbeat_url, tick);
	    s = sbuf;
	}
	MD5Update (&mdContext, s, strlen(s));
	s = (unsigned char *)user_id;
	MD5Update (&mdContext, s, strlen(s));
	MD5Final (&mdContext);

	char hashbuf[NB_SLEN];
	for (int i = 0; i < 16; i++)
	    sprintf(hashbuf+i*2, "%02x", mdContext.digest[i]);

	if (strcasecmp(hashbuf, mppass) == 0)
	    return 1;
    }

    return 0;
}

void
convert_secret(char sbuf[NB_SLEN], char * url, int tick)
{
    // Use secret prefix layout to avoid maybe attack on the hash algorithm.
    // This is used as a mixing function so HMAC extension is not available.
    MD5_CTX mdContext;
    MD5Init (&mdContext);
    unsigned char * s = (unsigned char *)server->secret;
    MD5Update (&mdContext, s, strlen(s));
    s = (unsigned char *)url;
    MD5Update (&mdContext, s, strlen(s));

    if (server->key_rotation) {
	// What's a reasonable minimum for this? One minute seems too short.
	time_t r = server->key_rotation;
	if (r<300) r = 300;
	time_t now = time(0);
	unsigned int period = now/r - tick;
	char nbuf[sizeof(int)*3+3];
	sprintf(nbuf, "%u", period);
	s = (unsigned char *)nbuf;
	MD5Update (&mdContext, s, strlen(s));
    }
    MD5Final (&mdContext);

    for (int i = 0; i < 16; i++)
	sprintf(sbuf+i*2, "%02x", mdContext.digest[i]);
}
