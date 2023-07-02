#include <assert.h>

#include "random.h"

#if INTERFACE
#if defined(PCG32_INITIALIZER) && !defined(USE_MIXED) && !defined(USE_PCG32)
#define USE_MIXED
//#define USE_PCG32
#endif

#ifdef USE_PCG32
#define map_random_t pcg32_random_t
#define bounded_random_r pcg32_boundedrand_r
#define seed_rng PCG_seed_rng
#define bounded_random pcg32_boundedrand

#else
typedef struct unx_random_t unx_random_t;
struct unx_random_t {
    char statebuf[128]; // Default size
};

inline static uint32_t
bounded_random(int range)
{
#ifdef USE_MIXED
    return pcg32_boundedrand(range);
#else
    // Divide by RAND_MAX+1 to put the value into [0..1) then
    // multiply by the requested range (wait .. flip that)
    // GCC makes the division a shift even in -O0
    // Yes, it's still biased, by less than 0.0001% for small ranges.
    return ((uint64_t)range*random()) / (RAND_MAX+(uint64_t)1);
#endif
}

#ifndef USE_MIXED
#define map_random_t unx_random_t
#else
#define map_random_t mcc_random_t
typedef struct mcc_random_t mcc_random_t;
struct mcc_random_t {
    int rtype, ktype;
    union {
	pcg32_random_t pcg[1];		// ktype == 0
	uint64_t seed64;		// ktype == 1
	struct unx_random_t random_r;	// ktype == 2
	uint16_t state48[3];		// ktype == 3
	struct {			// ktype == 4
	    uint32_t seed32;
	    uint32_t seed32b;
	};
    };
};

#endif
#endif

// Standard Unix rand() implementation; note 2**15-1 max and 32 bit seed.
// The seed must be twos complement or unsigned.
// NB: Current POSIX requires "a period of at least 2^32"
// Generates visible artifacts in B/W version of "rainbow" (with % bound)
#define randr(_seed) (((_seed = _seed*1103515245 + 12345)>>16) & 0x7FFF)

// RANDU -- "a truly horrible random number generator" -- Knuth
// This is a m = 2**31 Lehmer PRNG.
// You MUST use high bits -- the LSB is always one.
#define randu(_seed) (_seed = ((_seed<<16) + (_seed<<1) + _seed) & 0x7fffffff)

// SVID UNIX rand48() functions are a 48bit LCG usually with
// a = 0x5DEECE66D, c = 11, as 11 is relativly prime to 2**48 and 'a-1' is
// divisible by four this is a maximum period LCG.
// This PRNG shows artifacts on a "BW" map from the low bits

// The BSD random() functions seem to be a better generator, but they use
// a static pointer to the state storage and so POSIX does not support
// them in for multi-threaded use.

//----------------------------------------------------------------------------

// Lehmer generator with constants from Park & Miller
// Seed is a positive int32_t and must not be zero or 0x7fffffff.
// Also used in C++11's minstd_rand (minstd_rand0 uses 16807 in place of 48271)
#define lehmer_pm(_seed) (_seed = (uint64_t)_seed * 48271 % 0x7fffffff)

// Lehmer generator with constants from the Sinclair zx81 (Only 64k cycle)
#define lehmer_81(_seed) (_seed = (((uint16_t)_seed+1L)*75%65537L)-1)
	// Using the zx81 Lehmer rng means that we have a short (64k)
	// sequence of blocks that will cover the floor of a 256x256
	// map just once. Neverthless a 1024x1024 BW map is needed
	// to see the repetition easily.
	// Unfortunatly 65537 is suspected to be the last Fermat Prime.
#endif

static int rand_init_done = 0;

// When we fork off for a new connection we want to restart the RNG.
void
reinit_rand_gen()
{
    rand_init_done = 0;
    init_rand_gen();
}

void
init_rand_gen()
{
    if (rand_init_done == getpid()) return;
    rand_init_done = getpid();
    struct timeval now;
    gettimeofday(&now, 0);

    // A pretty trivial semi-random code, maybe 24bits of randomness.
    srandom(now.tv_sec ^ (now.tv_usec*4294U));

#if defined(USE_PCG32) || defined(USE_MIXED)
    // Somewhat better random seed, the whole time, pid and ASLR.
    // The "stream" needs to be different from the main seed so
    // use splitmix64 to mix the two parts.
    uint64_t seed =
	(now.tv_sec*(uint64_t)1000000 + now.tv_usec) ^
	((uintptr_t)&process_args) ^
	((int64_t)(getpid()) << (sizeof(uintptr_t)*4+8));
    uint64_t v1, v2;
    v1 = splitmix64_r(&seed);
    v2 = splitmix64_r(&seed);
    pcg32_srandom(v1, v2);

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
    // Nowhere to put seed ... okaaay
    if (!seed) return 0;
    // Seed exists and is not a "-" => good.
    if (*seed != 0 && (*seed != '-' || seed[1] != 0)) return 0;
    // If we have a fallback seed, use it.
    if (!seed[0] && fallback_seed) {
	sprintf(seed, "%jd", (uintmax_t)fallback_seed);
	return 0;
    }

    init_rand_gen();

#if defined(USE_PCG32) || defined(USE_MIXED)
    // Generate a random GUID to fill most of the bits.
    uint32_t n1 = pcg32_random(), n2 = pcg32_random();
    sprintf(seed, "%08x-%04x-%04x-%04x-%04x%08x",
	pcg32_random(),
	n1 & 0xFFFF,
	0x4000 + ((n1>>16) & 0xFFF),
	0x8000 + ((n2>>16) & 0x3FFF),
	n2 & 0xFFFF,
	pcg32_random());
#else
    // Just generate a 64bit integer;
    // But will have only 32 bits of randomness, at most, because of the
    // seed for srandom().
    uint64_t v0 = random();
    v0 = (v0<<31) + random();
    sprintf(seed, "0x%jx", (uintmax_t)v0);
#endif
    return 1;
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
	// The fixed bits are the top two bits of the first value
	// and the top four of the second.
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

#if defined(USE_PCG32) || defined(USE_MIXED)
#ifdef USE_MIXED
    rng->rtype = 0;
    if (*sseed == '!' && (sseed[2] == '/' || sseed[3] == '/')) {
	sseed++;
	if (*sseed >= '0' && *sseed <= '9' && sseed[1] == '/') {
	    rng->rtype = *sseed - '0';
	    sseed+=2;
	} else if (*sseed >= '0' && *sseed <= '9' && sseed[1] >= '0' && sseed[1] <= '9') {
	    rng->rtype = *sseed - '0';
	    rng->rtype *= 10;
	    rng->rtype += sseed[1] - '0';
	    sseed+=3;
	}
    }
    if (rng->rtype > 9) rng->rtype = 9;
    switch(rng->rtype) {
	case 0: case 9: rng->ktype = 0; break;
	case 1: case 2: case 3: rng->ktype = rng->rtype; break;
	default: rng->ktype = 4; break;
    }

    if (rng->ktype == 0)
#endif
    {
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
#ifdef USE_MIXED
	pcg32_srandom_r(rng->pcg, v1, v2);
#else
	pcg32_srandom_r(rng, v1, v2);
#endif
	// printlog("Seed = \"%s\", PCG32-Rng = 0x%jx,0x%jx", seed, v1, v2);
	return;
    }
#endif

#if !defined(USE_PCG32)
    {
	uint64_t v1;
	char * e = 0;
	v1 = strtoumax(sseed, &e, 0);
	// Anything after an initial number is hashed.
	for(uint8_t * p = (uint8_t*)e; *p; p++)
	    v1 = *p + ((v1<<6)+(v1<<16)-v1);
#ifndef USE_MIXED
	char * gblstate =
	    initstate((unsigned int)(v1^(v1>>32)), rng->statebuf, sizeof(rng->statebuf));
	setstate(gblstate);
	// printlog("Seed = %s, Rng = 0x%x", seed, (unsigned int)v1);
#else
	if (*e) {
	    if (v1 < 1000000000)
		printlog("Seed = \"%s\" -> Converted R%d = %jd", seed, rng->rtype, v1);
	    else
		printlog("Seed = \"%s\" -> Converted R%d = 0x%jx", seed, rng->rtype, v1);
	}
	if (rng->ktype == 1) {
	    rng->seed64 = v1;
	} else if (rng->ktype == 2) {
	    map_random_t t = {.rtype = rng->rtype, .ktype = rng->ktype};
	    *rng = t;
	    char * gblstate =
		initstate((unsigned int)(v1^(v1>>32)), rng->random_r.statebuf, sizeof(rng->random_r.statebuf));
	    setstate(gblstate);
	    // printlog("Seed = %s, Rng = 0x%x", seed, (unsigned int)v1);
	} else if (rng->ktype == 3) {
	    rng->state48[0] = 0x330E;
	    rng->state48[1] = (v1 & 0xFFFF);
	    rng->state48[2] = (v1 >> 16);
	    if (v1 >= 0x100000000)
		rng->state48[0] = (v1 >> 32);
	} else {
	    rng->seed32 = v1;
	    rng->seed32b = v1 ^ (v1>>32);
	    if (rng->seed32 == 0 || rng->seed32 == 0x7fffffff)
		rng->seed32 = 1;
	}
#endif
    }
#endif
}

#ifndef USE_PCG32
uint32_t
bounded_random_r(map_random_t *rng, int mod_r)
{
#ifndef USE_MIXED
    uint32_t res;
    char * gblstate = setstate(rng->statebuf);
    res = random();
    setstate(gblstate);

    // Rescale so we can use fast 2^N division.
    return ((uint64_t)mod_r*res) / (RAND_MAX+(uint64_t)1);
#else
    switch(rng->rtype)
    {
    case 0:
    {
	uint32_t bound = mod_r;
	uint32_t threshold = -bound % bound;
	for (;;) {
	    uint32_t r = pcg32_random_r(rng->pcg);
	    if (r >= threshold)
		return r % bound;
	}
    }
    case 1:
	return splitmix64_bounded_r(&rng->seed64, mod_r);

    case 2:
    {
	uint32_t res;
	char * gblstate = setstate(rng->random_r.statebuf);
	res = random();
	setstate(gblstate);

	// Rescale so we can use fast 2^N division.
	return ((uint64_t)mod_r*res) / (RAND_MAX+(uint64_t)1);
    }
    case 3:
	// Range 0..0x7fffffff or -0x8000000..0x7fffffff
	// High bits are better than low bits.
	return ((uint64_t)mod_r * (uint32_t)jrand48(rng->state48)) >> 32;

    case 4:
	// Range 1..0x7ffffffe
	return lehmer_pm(rng->seed32) % mod_r;
    case 5:
	// Range 0..0xffff
	return lehmer_81(rng->seed32) % mod_r;
    case 6:
	// Range 0..0x7fff
	return ((uint64_t)mod_r * (uint32_t)randr(rng->seed32)) >> 15;

    case 7:
	// Range 1..0x7fffffff .. We must use the high bits
	// This gets results that don't actually *look* like junk,
	// but it's a slow 64bit div and a mult
	return ((uint64_t)mod_r * (randu(rng->seed32)-1)) / 0x7fffffff;

    case 8:
	// Some incs: 362437, 314159, 73939133
	return (rng->seed32 += 362437) % mod_r;

    case 9:
    {
        uint64_t r;
	uint32_t bound = mod_r;
	for(;;)
	{
	    r = (uint64_t)bound * pcg32_random_r(rng->pcg);
	    uint32_t l = r;
	    if (l >= bound) break;
	    uint32_t threshold = -bound % bound;
	    if (l >= threshold) break;
	}
	return r >> 32;
    }

    default: return 0;
    }
#endif
}

#endif

#ifndef USE_PCG32
void
seed_rng(map_random_t *rng, map_random_t *new_rng)
{

#if !defined(USE_MIXED)
    {
	char * gblstate = setstate(rng->statebuf);
	uint32_t res;
	res = random();
	uint64_t v1 = res;
	res = random();
	v1 = (v1<<31) + res;

	initstate((unsigned int)(v1^(v1>>32)), new_rng->statebuf, sizeof(new_rng->statebuf));
	setstate(gblstate);
	return;
    }
#else
    new_rng->rtype = rng->rtype;
    new_rng->ktype = rng->ktype;
    if (rng->ktype == 0)
	return PCG_seed_rng(rng->pcg, new_rng->pcg);
    *new_rng = *rng;
    if (new_rng->ktype == 1) {
	// Use splitmix64 to make a 64 bit key.
	// This simply takes the next value from the source rng as the seed
	// for the new one. This relies on the hash.
	new_rng->seed64 = splitmix64_r(&rng->seed64);
    } else if (rng->ktype == 2) {
	char * gblstate = setstate(rng->random_r.statebuf);
	uint32_t res;
	res = random();
	uint64_t v1 = res;
	res = random();
	v1 = (v1<<31) + res;

	initstate((unsigned int)(v1^(v1>>32)), new_rng->random_r.statebuf, sizeof(new_rng->random_r.statebuf));
	setstate(gblstate);
    } else if (new_rng->ktype == 3) {
	// Just throw this in here for now.
	uint64_t v1 = (uint32_t)jrand48(rng->state48);
	new_rng->state48[0] = 0x330E;
	new_rng->state48[1] = (v1 & 0xFFFF);
	new_rng->state48[2] = (v1 >> 16);
	if (v1 >= 0x100000000)
	    new_rng->state48[0] = (v1 >> 32);
    } else {
	(void) bounded_random_r(rng, 1);
	rng->seed32b ^= rng->seed32;

	new_rng->seed32 = rng->seed32b;
	new_rng->seed32b = ~new_rng->seed32;
	if (new_rng->seed32 == 0 || new_rng->seed32 == 0x7fffffff)
	    new_rng->seed32 = 1;
    }
    return;
#endif
}
#endif

#if INTERFACE
#if defined(USE_PCG32) || defined(USE_MIXED)
inline static void
PCG_seed_rng(pcg32_random_t *rng, pcg32_random_t *new_rng)
{
    uint64_t v1, v2;
    uint64_t n1 = pcg32_random_r(rng);
    uint64_t n2 = pcg32_random_r(rng);
    v1 = (n1<<32) + n2;
    n1 = pcg32_random_r(rng);
    n2 = pcg32_random_r(rng);
    v2 = (n1<<32) + n2;
    pcg32_srandom_r(new_rng, v1, v2);
}
#endif
#endif

#if INTERFACE
// Ian C. Bullard -- "Gamerand"
inline static int randg_r(uint32_t * state)
{
    state[0] = (state[0] << 16) + (state[0] >> 16);
    state[0] += state[1];
    state[1] += state[0];
    return state[0];
}

inline static void srandg_r(uint32_t * state, unsigned int seed)
{
    state[1] = (state[0] = seed) ^ 0x49616E42;
}
#endif

#if INTERFACE
#ifdef __SIZEOF_INT128__
inline static uint64_t wyhash64(uint64_t * seed) {
  *seed += 0x60bee2bee120fc15;
  __uint128_t tmp;
  tmp = (__uint128_t) *seed * 0xa3b195354a39b70d;
  uint64_t m1 = (tmp >> 64) ^ tmp;
  tmp = (__uint128_t)m1 * 0x1b03738712fad5c9;
  uint64_t m2 = (tmp >> 64) ^ tmp;
  return m2;
}
#endif
#endif
