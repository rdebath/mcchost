
#include "random.h"

#if INTERFACE
#ifdef PCG32_INITIALIZER
#define map_random_t pcg32_random_t
#define bounded_random_r pcg32_boundedrand_r
#define bounded_random pcg32_boundedrand
#else
#define map_random_t unx_random_t
typedef struct unx_random_t unx_random_t;
struct unx_random_t {
    struct random_data rand_data;
    char statebuf[128]; // Default size
};
#endif

// Standard Unix rand() implementation; note 2**15-1 max and 32 bit seed.
// The seed must be twos complement or unsigned.
// Generates visible artifacts in "rainbow"
#define randr(_seed) (((_seed = _seed*1103515245 + 12345)>>16) & 0x7FFF)

// Lehmer generator with constants from Park & Miller
// Seed is a positive int32_t and must not be zero or 0x7fffffff.
#define lehmer_pm(_seed) (_seed = (uint64_t)_seed * 48271 % 0x7fffffff)

// Lehmer generator with constants from the Sinclair zx81 (Only 64k cycle)
#define lehmer_81(_seed) (_seed = (((uint16_t)_seed+1L)*75%65537L)-1)
	// Using the zx81 Lehmer rng means that we have a short (64k)
	// sequence of blocks that will cover the floor of a 256x256
	// map just once. As such there should be four repeats across
	// the floor of a 512x512 map. A much larger map is needed to
	// see the repitition.
#endif

static int rand_init_done = 0;
void
reinit_rand_gen()
{
    rand_init_done = 0;
    init_rand_gen();
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

/* Returns 1 if we used a non-repeatable source for the seed.
 * If the seed is already populated, or we use fallback_seed
 * this returns 0.
 */
int
populate_map_seed(char * seed, uint64_t fallback_seed)
{
    if (!seed) return 0;
    // If we have a fallback seed, use it.
    if (!seed[0] && fallback_seed) {
	sprintf(seed, "%jd", (uintmax_t)fallback_seed);
	return 0;
    }
#ifdef PCG32_INITIALIZER
    // Generate a random GUID to fill most of the bits.
    if (!*seed || (*seed == '-' && seed[1] == 0)) {
	init_rand_gen();
	uint32_t n1 = pcg32_random(), n2 = pcg32_random();
	sprintf(seed, "%08x-%04x-%04x-%04x-%04x%08x",
	    pcg32_random(),
	    n1 & 0xFFFF,
	    0x4000 + ((n1>>16) & 0xFFF),
	    0x8000 + ((n2>>16) & 0x3FFF),
	    n2 & 0xFFFF,
	    pcg32_random());
	return 1;
    }
#else
    // Just generate a 64bit integer;
    // But will have only 32 bits of randomness, at most, because of the
    // seed for srandom().
    if (!*seed || (*seed == '-' && seed[1] == 0)) {
        init_rand_gen();
        uint64_t v0 = random();
        v0 = (v0<<31) + random();
        sprintf(seed, "0x%jx", (uintmax_t)v0);
	return 1;
    }
#endif
    return 0;
}

void
map_init_rng(map_random_t *rng, char * seed)
{
    if (!seed) seed = "1";

    int is_guid = 0;
    if (strlen(seed) == 36 && seed[8] == '-' && seed[13] == '-' &&
	    seed[18] == '-' && seed[23] == '-') {
	is_guid = 1; // Looks about right; check the digits.
	for(int i = 0; seed[i]; i++) {
	    if (i == 8 || i == 13 || i == 18 || i == 23) continue;
	    int ch = seed[i];
	    if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F'))) {
		is_guid = 0;
		break;
	    }
	}
    }

    // Shuffle a guid into two 64bit numbers.
    char xbuf[MB_STRLEN*2+1], *sseed = seed;
    if (is_guid) {
	// XXXXXXXX-XXXX-4XXX-YXXX-XXXXXXXXXXXX␀
	// 0123456789012345678901234567890123456
	// Y=[89ab] X=[0-9a-f]

	// You need an invalid guid to represent all possible seeds.
	char * p = xbuf;
	memcpy(p, "0x", 2); p += 2;
	memcpy(p, seed+19, 4); p += 4;	// Variant
	memcpy(p, seed+24, 12); p += 12;
	memcpy(p, ",0x", 3); p += 3;
	memcpy(p, seed+14, 4); p += 4;	// Version
	memcpy(p, seed+9, 4); p += 4;
	memcpy(p, seed, 8); p += 8;
	xbuf[37] = 0;
	sseed = xbuf;
    }

#ifdef PCG32_INITIALIZER
    uint64_t v1, v2;
    char * e = 0;
    v1 = strtoumax(sseed, &e, 0);
    if (e && *e == ',' && e[1] != 0)
	v2 = strtoumax(e+1, 0, 0);
    else {
	// Hash in leftovers; but only to 64bits.
	for(uint8_t * p = (uint8_t*)e; *p; p++)
	    v1 = *p + ((v1<<6)+(v1<<16)-v1);

	if (*e) {
	    if (v1 < 1000000000)
		printlog("Seed = \"%s\" -> Converted = %jd", seed, v1);
	    else
		printlog("Seed = \"%s\" -> Converted = 0x%jx", seed, v1);
	}

	// PCG32 likes distinct streams to have a large hamming distance.
	uint64_t seed = v1;
	jump_splitmix64(&seed, 0x75b4fb5cadd2212e); // Random jump for this app.
	v1 = splitmix64_r(&seed);
	v2 = splitmix64_r(&seed);
    }
    pcg32_srandom_r(rng, v1, v2);
    // printlog("Seed = \"%s\", PCG32-Rng = 0x%jx,0x%jx", seed, v1, v2);
#else
    uint64_t v1;
    char * e = 0;
    v1 = strtoumax(sseed, &e, 0);
    // Anything after an initial number is hashed.
    for(uint8_t * p = (uint8_t*)e; *p; p++)
	v1 = *p + ((v1<<6)+(v1<<16)-v1);
    map_random_t t = {0};
    *rng = t;
    initstate_r((unsigned int)(v1^(v1>>32)), rng->statebuf, sizeof(rng->statebuf), &rng->rand_data);
    // printlog("Seed = %s, Rng = 0x%x", seed, (unsigned int)v1);
#endif
}

#ifndef PCG32_INITIALIZER
uint32_t
bounded_random_r(map_random_t *rng, int mod_r)
{
    uint32_t res;
    random_r(&rng->rand_data, &res);
    return res % mod_r; // Yes, it's biased
}

uint32_t
bounded_random(int mod_r)
{
    return random() % mod_r; // Yes, it's biased
}
#endif

void
seed_rng(map_random_t *rng, map_random_t *new_rng)
{
#ifdef PCG32_INITIALIZER
    uint64_t v1, v2;
    uint64_t n1 = pcg32_random_r(rng);
    uint64_t n2 = pcg32_random_r(rng);
    v1 = (n1<<32) + n2;
    n1 = pcg32_random_r(rng);
    n2 = pcg32_random_r(rng);
    v2 = (n1<<32) + n2;
    pcg32_srandom_r(new_rng, v1, v2);
#else
    map_random_t t = {0};
    *new_rng = t;

    uint32_t res;
    random_r(&rng->rand_data, &res);
    uint64_t v1 = res;
    random_r(&rng->rand_data, &res);
    v1 = (v1<<31) + res;
    initstate_r((unsigned int)(v1^(v1>>32)), new_rng->statebuf, sizeof(new_rng->statebuf), &new_rng->rand_data);
#endif
}
