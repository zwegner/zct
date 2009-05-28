/** ZCT/bit.h--Created 111208 **/

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

#ifndef BIT_H
#define BIT_H

/* This is pretty hacky looking, it's just done to get it in the declaration
	of a for loop. Just trust me that it works. */
#define FOR_BB(sq, bb)													\
	for (; (bb) != 0 && ((sq) = first_square(bb), TRUE); CLEAR_FIRST_BIT(bb))

/**
first_square():
Returns the first square that is set in a bitboard. This 32-bit friendly routine
was devised by Matt Taylor, and relies on de Bruijn multiplication.
Created 070105; last modified 071308
**/
static inline SQUARE first_square(BITBOARD bitboard)
{
#if defined(ZCT_POSIX) && defined(ZCT_64) && defined(ZCT_INLINE)

	BITBOARD dummy;

	asm("bsfq %1, %0"
		: "=r" (dummy)
		: "r" (bitboard));

	return dummy;

#else

	unsigned int folded;
	/* A DeBruijn index table. */
	static const SQUARE lsb_64_table[64] =
	{
		63, 30,  3, 32, 59, 14, 11, 33,
		60, 24, 50,  9, 55, 19, 21, 34,
		61, 29,  2, 53, 51, 23, 41, 18,
		56, 28,  1, 43, 46, 27,  0, 35,
		62, 31, 58,  4,  5, 49, 54,  6,
		15, 52, 12, 40,  7, 42, 45, 16,
		25, 57, 48, 13, 10, 39,  8, 44,
		20, 47, 38, 22, 17, 37, 36, 26
	};

	bitboard ^= (bitboard - 1);
	folded = (unsigned int)(bitboard ^ (bitboard >> 32));
	return lsb_64_table[folded * 0x78291ACF >> 26];

#endif
}

/**
last_square():
Returns the last square that is set in a bitboard. Thanks to Gerd Isenberg for
this nifty algorithm.
Created 113005; last modified 122205
**/
static inline SQUARE last_square(BITBOARD bitboard)
{
#if defined(ZCT_POSIX) && defined(ZCT_64) && defined(ZCT_INLINE)

	BITBOARD dummy;

	asm("bsrq %1, %0"
		: "=r" (dummy)
		: "r" (bitboard));

	return dummy;

#else

	unsigned int low;
	unsigned int high;
	unsigned int i;
	unsigned int m;

	high = (unsigned int)(bitboard >> 32);
	low = (unsigned int)bitboard;
	/* Determine the 2^5 bit. */
	m = (high != 0);
	i = m << 5;
	/* Take a certain half of the bitboard based on the 2^5 bit. */
	low = high | (low & (m - 1));
	/* Determine the 2^4 bit. */
	m = (low > 0xffff) << 4;
	i += m;
	/* Determine the 2^3 bit. */
	low >>= m;
	m = ((0xff - low) >> 16) & 8;
	i += m;
	/* Determine the 2^2 bit. */
	low >>= m;
	m = ((0x0f - low) >> 8) & 4;
	low >>= m;
	/* Get the 2^1 and 2^0 bits from in-register lookup. */
	return i + m + ((0xffffaa50 >> (low * 2)) & 3);

#endif
}

/**
pop_count():
Returns the number of bits set in a bitboard. This code is from somewhere.
Created 083106; last modified 110506
**/
static inline int pop_count(BITBOARD bitboard)
{
#if defined(ZCT_POSIX) && defined(ZCT_64) && defined(ZCT_INLINE)

	BITBOARD dummy, dummy2, dummy3;

	asm("          xorq    %0, %0" "\n\t"
		"          testq   %1, %1" "\n\t"
		"          jz      2f" "\n\t"
		"1:        leaq    -1(%1),%2" "\n\t"
		"          incq    %0" "\n\t"
		"          andq    %2, %1" "\n\t"
		"          jnz     1b" "\n\t"
		"2:                      " "\n\t"
		: "=&r" (dummy),
		"=&r" (dummy2),
		"=&r" (dummy3)
		: "1"((BITBOARD)(bitboard))
		: "cc");
	return dummy;

#else

	bitboard = bitboard - (bitboard >> 1 & 0x5555555555555555ull);
	bitboard = (bitboard & 0x3333333333333333ull) +
		(bitboard >> 2 & 0x3333333333333333ull);
	bitboard = bitboard + (bitboard >> 4) & 0x0F0F0F0F0F0F0F0Full;
	bitboard = bitboard + (bitboard >> 8);
	bitboard = bitboard + (bitboard >> 16);
	return (int)(bitboard + (bitboard >> 32) & 0x7F);

#endif
}

#endif /* INLINE_H */
