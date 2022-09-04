#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <sys/time.h>

#include "random.h"

#if INTERFACE
#ifdef PCG32_INITIALIZER
#define map_random_t pcg32_random_t
#define bounded_random_r pcg32_boundedrand_r
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
	// the floor of a 512x512 map. It looks okay none the less.
#endif

void
map_init_rng(map_random_t *rng, char * seed)
{
#ifdef PCG32_INITIALIZER
    char sbuf[MB_STRLEN*2+1] = "";
    if (!seed) seed = sbuf;
    if (!*seed) {
	init_rand_gen();
	uint32_t n1 = pcg32_random(), n2 = pcg32_random();
	snprintf(seed, sizeof(sbuf), "%08x-%04x-%04x-%04x-%04x%08x",
	    pcg32_random(),
	    n1 & 0xFFFF,
	    0x4000 + ((n1>>16) & 0xFFF),
	    0x8000 + ((n2>>16) & 0x3FFF),
	    n2 & 0xFFFF,
	    pcg32_random());
    }

    // if it appears to be a guid, shuffle.
    char xbuf[MB_STRLEN*2+1], *sseed = seed;
    if (strlen(seed) == 36 && seed[8] == '-' && seed[13] == '-' &&
	    seed[18] == '-' && seed[23] == '-') {

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

    uint64_t v1, v2;
    char * com = 0;
    v1 = strtoumax(sseed, &com, 0);
    if (com && *com == ',' && com[1] != 0)
	v2 = strtoumax(com+1, 0, 0);
    else {
	// PCG32 likes distinct streams to have a large hamming distance.
	uint64_t seed = v1;
	jump_splitmix64(&seed, 0x75b4fb5cadd2212e); // Random jump for this app.
	v1 = next_splitmix64(&seed);
	v2 = next_splitmix64(&seed);
    }
    pcg32_srandom_r(rng, v1, v2);
    // printlog("Seed = %s, Rng = 0x%jx,0x%jx", seed, v1, v2);
#else
    char sbuf[MB_STRLEN*2+1] = "";
    if (!seed) seed = sbuf;
    if (!*seed) {
	init_rand_gen();
	uint64_t v0 = random();
	v0 = (v0<<31) + random();
	snprintf(seed, sizeof(sbuf), "0x%jx", (uintmax_t)v0);
    }
    uint64_t v1;
    if (strlen(seed) == 36 && seed[8] == '-')
	v1 = strtoumax(seed, 0, 16);
    else
	v1 = strtoumax(seed, 0, 0);
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
