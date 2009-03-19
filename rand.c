/** ZCT/rand.c--Created 011806 **/

/** Copyright 2008 Zach Wegner **/
/*
 * This file is part of ZCT.
 *
 * ZCT is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ZCT is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ZCT.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "zct.h"
#include "functions.h"
#include "globals.h"

#define RSIZE			(32)
#define R1				(7)
#define R2				(3)

HASHKEY random_buffer[RSIZE] =
{
	0x4ba36abdd2101f45ULL, 0x6fe534cd2dfa03c1ULL, 0x64d3ace9af613b66ULL, 0xe084227d10b2e63bULL,
	0xf0891daafe3f9dc7ULL, 0x769dfe43bcfa4141ULL, 0xb6bf3b95b2df9b65ULL, 0x235673bbf5199f3aULL,
	0x899dc6230b46a815ULL, 0x3218aba366f1a731ULL, 0xcf5cd8980a7c06d7ULL, 0xc76089f5f18ef3f3ULL,
	0x40d5055ed352d4aeULL, 0xa5c15365158108bfULL, 0xd1e58c8c50e29e38ULL, 0x47b89b586e54bb01ULL,
	0x192a216dd430eb7fULL, 0x0f7dd278b7855a81ULL, 0x59f35d5ac1aa2981ULL, 0xc68b03f3e69e2149ULL,
	0x54e622b4fc83c00dULL, 0x92cf04d3c7a040f1ULL, 0x57e76935fc1a0c6eULL, 0x8f7f8b7fed89075dULL,
	0x0a38da42aa3bdcb3ULL, 0xafb12452313261f8ULL, 0x3bb14865eb39354aULL, 0x77e0ae034986274dULL,
	0x1cb7789893230d4cULL, 0x3789c9e12df2b254ULL, 0x1befbf17dd838805ULL, 0x78472f6734fa698dULL
};

/**
rotate_left():
Rotate the bits in the given hashkey to the left.
Created 011806; last modified 011806
**/
HASHKEY rotate_left(HASHKEY hashkey, int rotate)
{
	return (hashkey << rotate) | (hashkey >> (64 - rotate));
}

/**
random_hashkey():
Return a pseudo-randomly generated hashkey. Thanks to Vincent Diepeveen and Agner Fog for the algorithm.
Created 011806; last modified 021808
**/
HASHKEY random_hashkey(void)
{
	static int rp1 = 0, rp2 = 1;
	static HASHKEY x = 0;

	random_buffer[rp1] = rotate_left(random_buffer[rp1], R1) ^ rotate_left(random_buffer[rp2], R2);
	x ^= random_buffer[rp1];
	rp1 = (rp1 + 2) % RSIZE; /* rp1 is even */
	rp2 = (rp2 + 2) % RSIZE; /* rp2 is odd */
	return x;
}

/**
seed_random():
Provide a seed to be used in the RNG. This will only be used after program initilization, as ZCT
relies on some random numbers being the same every time it is executed.
Created 060208; last modified 060208
**/
void seed_random(HASHKEY hashkey)
{
	random_buffer[0] = hashkey;
}
