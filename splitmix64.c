/*  Written in 2015 by Sebastiano Vigna (vigna@acm.org)

To the extent possible under law, the author has dedicated all copyright
and related and neighboring rights to this software to the public domain
worldwide. This software is distributed without any warranty.

See <http://creativecommons.org/publicdomain/zero/1.0/>. */

#include <stdint.h>

/* This is a fixed-increment version of Java 8's SplittableRandom generator
   See http://dx.doi.org/10.1145/2714064.2660195 and
   http://docs.oracle.com/javase/8/docs/api/java/util/SplittableRandom.html

   It is a very fast generator passing BigCrush, and it can be useful if
   for some reason you absolutely want 64 bits of state. */

/* static uint64_t x; / * The state can be seeded with any value. */

// Note the constant ...
// ((1+sqrt(5))/2-1)*(2^64) or (2/(1+sqrt(5)))*(2^64)
// Is 11400714819323198485.951610587621806949856...
// Or 0x9e3779b97f4a7c15.f39cc0605cedc8341082276bf3a...
// Need an odd value, so round down.
#define GOLDEN_GAMMA 0x9e3779b97f4a7c15

uint64_t splitmix64_r(uint64_t *x) {
	uint64_t z = (*x += GOLDEN_GAMMA);
	z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
	z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
	return z ^ (z >> 31);
}

/* This is the trivial jump function; this is NOT the "split" function
 * which attempts to compute a new replacement for the Weyl sequence
 * while avoiding the (few) bad choices.
 */
void jump_splitmix64(uint64_t *x, uint64_t steps) {
    *x += GOLDEN_GAMMA * steps;
}

/* Peek the generator to get the value "jump" steps ahead.
 * Can be used as a stateless variant by incrementing jump for each call.
 * On most current CPUs this is still constant time, but one more multiply.
 */
uint64_t fwd_splitmix64(uint64_t seed, uint64_t jump) {
    jump_splitmix64(&seed, jump);
    return splitmix64_r(&seed);
}


/*
    David Stafford's tested hash replacement values for MurmurHash3
    First mixer is original values, Mixer13 is splitmix64.

Mixer	33	0xff51afd7ed558ccd	33	0xc4ceb9fe1a85ec53	33
Mix01	31	0x7fb5d329728ea185	27	0x81dadef4bc2dd44d	33
Mix02	33	0x64dd81482cbd31d7	31	0xe36aa5c613612997	31
Mix03	31	0x99bcf6822b23ca35	30	0x14020a57acced8b7	33
Mix04	33	0x62a9d9ed799705f5	28	0xcb24d0a5c88c35b3	32
Mix05	31	0x79c135c1674b9add	29	0x54c77c86f6913e45	30
Mix06	31	0x69b0bc90bd9a8c49	27	0x3d5e661a2a77868d	30
Mix07	30	0x16a6ac37883af045	26	0xcc9c31a4274686a5	32
Mix08	30	0x294aa62849912f0b	28	0x0a9ba9c8a5b15117	31
Mix09	32	0x4cd6944c5cc20b6d	29	0xfc12c5b19d3259e9	32
Mix10	30	0xe4c7e495f4c683f5	32	0xfda871baea35a293	33
Mix11	27	0x97d461a8b11570d9	28	0x02271eb7c6c4cd6b	32
Mix12	29	0x3cd0eb9d47532dfb	26	0x63660277528772bb	33
Mix13	30	0xbf58476d1ce4e5b9	27	0x94d049bb133111eb	31
Mix14	30	0x4be98134a5976fd3	29	0x3bc0993a5ad19a13	31

https://zimbry.blogspot.com/2011/09/better-bit-mixing-improving-on.html
MurmurHash3 ...
https://github.com/aappleby/smhasher

*/
