#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "secret.h"

static int rand_init_done = 0;

void
generate_secret()
{
    if (!rand_init_done)
	init_rand_gen();

    char sbuf[20] = {0};
    int keylen = 10;

    for(int i=0; i<sizeof(sbuf); i++) {
#ifdef PCG32_INITIALIZER
	int ch = pcg32_boundedrand(62);
#else
	int ch = random() % 62;
#endif
	sbuf[i] = ch;
    }

    int fd = open("/dev/urandom", O_RDONLY);
    if (fd > 0) {
	uint64_t randval[2] = {0};
	int cc = read(fd, randval, sizeof(randval));
	if (cc == sizeof(randval)) {
	    char * p = sbuf;
	    for(int v=0; v<2; v++) {
		for(int i=0; i<10; i++, p++) {
		    int ch = randval[v] % 62;
		    randval[v] /= 62;
		    *p = (*p + ch) % 62;
		}
	    }
	    keylen = 20;
	} else
	    printlog("Unable to read /dev/urandom for secret, using poorman's");
	close(fd);
    } else
	printlog("Unable to open /dev/urandom for secret, using poorman's");

    static char base62[] =
	"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    for(int i=0; i<keylen; i++)
	server->secret[i] = base62[sbuf[i]%62];
    server->secret[keylen] = 0;
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
	    convert_secret(sbuf, tick);
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
convert_secret(char sbuf[NB_SLEN], int tick)
{
    // Use secret prefix layout to avoid maybe attack on the hash algorithm.
    // This is used as a mixing function so HMAC extension is not available.
    MD5_CTX mdContext;
    MD5Init (&mdContext);
    unsigned char * s = (unsigned char *)server->secret;
    MD5Update (&mdContext, s, strlen(s));

    {
	char buf[sizeof(int)*3+10];
	sprintf(buf, "/%d/", tcp_port_no);
	s = (unsigned char *)buf;
	MD5Update (&mdContext, s, strlen(s));
    }

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

    // 128bits in hex
    for (int i = 0; i < 16; i++)
	sprintf(sbuf+i*2, "%02x", mdContext.digest[i]);
}

void
init_rand_gen()
{
    if (rand_init_done) return;
    rand_init_done = 1;
    struct timeval now;
    gettimeofday(&now, 0);

    // A pretty trivial semi-random code, maybe 24bits of randomness.
    srandom(now.tv_sec ^ (now.tv_usec*4294U));

#ifdef PCG32_INITIALIZER
    // Somewhat better random seed, the whole time, pid and ASLR
    // The "stream" needs to be different from the main seed so
    // don't include the time in that part.
    pcg32_srandom(
	(now.tv_sec*(uint64_t)1000000 + now.tv_usec) ^
	((uintptr_t)&process_args) ^
	((int64_t)(getpid()) << (sizeof(uintptr_t)*4+8)),
	(((uintptr_t)&process_args) >> 12) +
	((int64_t)(getpid()) << (sizeof(uintptr_t)*4))
	);

// NB: for x86/x64
//              0x88000888
// On 32bit     0x99XXX000 --> Only *8*bits of ASLR
// On 64bit 0x91XXXXXXX000 --> 28bits of ASLR
//      0x8888800000000888
#endif
}
