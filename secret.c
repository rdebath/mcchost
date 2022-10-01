#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "secret.h"

/*
 * MPPass secret handling.
 *
 * This uses MD5, while it would be nice to change that it would involve
 * altering existing programs for which the source is not available.
 * MD5 is not broken for this particular application.
 *
 * The server->secret value is stored unmasked in "server.ini" and
 * can be transmitted in the clear to the hearbeat server.
 *
 * The first should not be a problem, however, if it is suspected
 * that the secret has been compromised it should be replaced.
 *
 * The clear transmission is an issue, so the "salt" must be rotated
 * with a sufficiently small period that it's probably okay. This
 * code does that, sending a hash of the true secret and the time
 * to the server as the heartbeat "salt".
 *
 * For the "salt" to be considered secure it must be sent using https.
 * Failing that, the default rotation period of six hours should be
 * a reasonable compromise.
 */

static char base62[] =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

void
generate_secret()
{
    init_rand_gen();

    char sbuf[NB_SLEN] = {0};
    int keylen = 10;

    for(int i=0; i<sizeof(sbuf)-1 && i<keylen; i++) {
	int ch = bounded_random(62);
	sbuf[i] = base62[ch];
	sbuf[i+1] = 0;
    }

    // Try to use the kernel PRNG. This should have lots of physical randomness.
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd > 0) {
	uint8_t randval[16] = {0};
	int cc = read(fd, randval, sizeof(randval));
	close(fd);
	if (cc == sizeof(randval))
	    base62_128(randval, sbuf);
	else
	    printlog("Unable to read /dev/urandom for secret, using poorman's");
    } else
	printlog("Unable to open /dev/urandom for secret, using poorman's");

    strcpy(server->secret, sbuf);
    fprintf(stderr, "Generated server secret %s\n", server->secret);
}

int
check_mppass(char * userid, char * mppass)
{
    if (strcmp(mppass, server->secret) == 0) return 1; //WTF!
    for(int tick = -1; tick<2; tick++)
    {
	// NB: Not vulnerable to length extension attacks due to previous
	// checks on userid character set and length. (ie NULs not allowed)
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
	s = (unsigned char *)userid;
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

    // Include the port number so each "mppass server" has a different key.
    {
	char buf[sizeof(int)*3+10];
	sprintf(buf, "/%d/", tcp_port_no);
	s = (unsigned char *)buf;
	MD5Update (&mdContext, s, strlen(s));
    }

    // Key rotation is necessary as the mppass will be transmitted over
    // a cleartext connection. This period is user visible, they need to
    // refresh their mppass list, so it can't be *too* short.
    //
    // What's a reasonable minimum for this? One minute seems too short.
    if (server->key_rotation) {
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
    base62_128(mdContext.digest, sbuf);
}

void
base62_128(uint8_t * bits128, char * str)
{
#ifdef __SIZEOF_INT128__
    // Base62 128 bit number print with all leading zeros.
    // bits128 is assumed to be bigendian so hex printing is trivial.
    unsigned __int128 v = 0;
    for(int i=0; i<16; i++)
	v = (v<<8) + bits128[i];
    for(int i=0; i<22; i++) {
	int c = v % 62;
	v /= 62;
	str[21-i] = base62[c];
    }
    str[22] = 0;
#else
    // Note: log(2**64)/log(62) = 10.748721853250684
    // And: log(2**128)/log(62) = 21.497443706501369
    // So it's the same size if we convert as one 128 bit value or two 64's
    //
    // NB: Two Bigending 64bit integers concatenated.
    for(int set=0; set<2; set++) {
	uint64_t v = 0;
	for(int i=0; i<8; i++)
	    v = (v<<8) + bits128[i];
	for(int i=0; i<11; i++) {
	    int c = v % 62;
	    v /= 62;
	    str[10-i] = base62[c];
	}
	bits128 += 8;
	str += 11;
	*str = 0;
    }
#endif
}
