/** ZCT/bit.c--Created 070105 **/

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

static BITBOARD attack_bb[64][4][64];
static BITBOARD attack_mask[64][4];

DIRECTION piece_dirs[][5] =
{
	{ -1 },					/* pawn */
	{ -1 },					/* knight */
	{ DIR_A1H8, DIR_A8H1, -1 },			/* bishop */
	{ DIR_HORI, DIR_VERT, -1 },			/* rook */
	{ DIR_HORI, DIR_A1H8, DIR_A8H1, DIR_VERT, -1},		/* queen */
	{ -1 }					/* king */
};

/**
attacks_bb():
Returns an attack bitboard for the given sliding piece on the given square.
Created 070305; last modified 090108
**/
BITBOARD attacks_bb(PIECE piece, SQUARE square)
{
	DIRECTION *dir;
	BITBOARD attacks;

	attacks = 0;
	switch (piece)
	{
		case PAWN:
			break;
		case KNIGHT:
			attacks = knight_moves_bb[square];
			break;
		case BISHOP:
			attacks = 0;
			attacks |= dir_attacks(square, board.occupied_bb, DIR_A1H8);
			attacks |= dir_attacks(square, board.occupied_bb, DIR_A8H1);
			break;
		case ROOK:
			attacks = 0;
			attacks |= dir_attacks(square, board.occupied_bb, DIR_HORI);
			attacks |= dir_attacks(square, board.occupied_bb, DIR_VERT);
			break;
		case QUEEN:
			attacks = 0;
			attacks |= dir_attacks(square, board.occupied_bb, DIR_HORI);
			attacks |= dir_attacks(square, board.occupied_bb, DIR_VERT);
			attacks |= dir_attacks(square, board.occupied_bb, DIR_A1H8);
			attacks |= dir_attacks(square, board.occupied_bb, DIR_A8H1);
			break;
		case KING:
			attacks = king_moves_bb[square];
			break;
		default:
			break;
	}
	return attacks;
}

#define FILL_FUNC_SH(inc, SH)							\
	do {												\
		piece |= empty & (piece SH inc);				\
		empty &= (empty SH inc);						\
		piece |= empty & (piece SH (inc * 2));			\
		empty &= (empty SH (inc * 2));					\
		return piece | (empty & (piece SH (inc * 4)));	\
	} while (FALSE)

/* Generic filler: We use a macro to create each function. The compiler will
	optimize away all of the conditionals as they are based on constants. */
#define FILL_FUNC(dir, inc)								\
	BITBOARD fill_##dir(BITBOARD piece, BITBOARD empty)	\
	{													\
		const int shift = ABS(inc);						\
		/* Weird--find the rank offset of inc.*/		\
		if ((inc + 16) % 8 == 1)						\
			empty &= MASK_SHR;							\
		else if ((inc + 16) % 8 == 7)					\
			empty &= MASK_SHL;							\
		if (inc > 0)									\
			FILL_FUNC_SH(shift, <<);					\
		else											\
			FILL_FUNC_SH(shift, >>);					\
	}

/**
fill_*():
Does a Kogge-Stone fill in the given direction. Basically it "smears" the bits
in piece in the direction, but only over bits set in empty. These are
implemented with a macro, as the code is largely identical between them, yet
there are some things that don't translate well to one function (shifts and
masks).
Created 070305; last modified 111308
**/
FILL_FUNC(up_right, 9)
FILL_FUNC(up, 8)
FILL_FUNC(up_left, 7)
FILL_FUNC(right, 1)
FILL_FUNC(down_right, -7)
FILL_FUNC(down, -8)
FILL_FUNC(down_left, -9)
FILL_FUNC(left, -1)

/**
smear_*():
Smears the set bits in the given direction. Similar to a fill, but no blocking
squares are used.
Created 110806; last modified 110806
**/
BITBOARD smear_up(BITBOARD g)
{
	g |= g << 8;
	g |= g << 16;
	g |= g << 32;
	return g;
}

BITBOARD smear_down(BITBOARD g)
{
	g |= g >> 8;
	g |= g >> 16;
	g |= g >> 32;
	return g;
}

/**
fill_attacks_*():
Given a bitboard of the attacking pieces and the occupied state, computes a
bitboard containing the attacks in a certain direction.
Created 100807; last modified 011308
**/
BITBOARD fill_attacks_knight(BITBOARD attackers)
{
	/* This works, trust me */
	return SHIFT_UP(SHIFT_UL(attackers)) |
		SHIFT_UP(SHIFT_UR(attackers)) |
		SHIFT_DN(SHIFT_DL(attackers)) |
		SHIFT_DN(SHIFT_DR(attackers)) |
		SHIFT_LF(SHIFT_UL(attackers)) |
		SHIFT_LF(SHIFT_DL(attackers)) |
		SHIFT_RT(SHIFT_UR(attackers)) |
		SHIFT_RT(SHIFT_DR(attackers));
}

BITBOARD fill_attacks_bishop(BITBOARD attackers, BITBOARD occupied)
{
	occupied = ~occupied;
	return SHIFT_UL(fill_up_left(attackers, occupied)) |
		SHIFT_UR(fill_up_right(attackers, occupied)) |
		SHIFT_DR(fill_down_right(attackers, occupied)) |
		SHIFT_DL(fill_down_left(attackers, occupied));
}

BITBOARD fill_attacks_rook(BITBOARD attackers, BITBOARD occupied)
{
	occupied = ~occupied;
	return SHIFT_UP(fill_up(attackers, occupied)) |
		SHIFT_RT(fill_right(attackers, occupied)) |
		SHIFT_DN(fill_down(attackers, occupied)) |
		SHIFT_LF(fill_left(attackers, occupied));
}

/**
flood_fill_*():
Given a piece type, a piece bitboard, and an occupied bitboard, computes the set
of squares that are able to be reached in either a certain amount of moves or
infinite moves (if moves == 0).
Created 100807; last modified 051808
**/
BITBOARD flood_fill_king(BITBOARD attackers, BITBOARD occupied, int moves)
{
	BITBOARD bb_temp;

	if (moves == 0)
		moves = 65; /* No square can be reachable in more than 64 moves. */
	while (TRUE)
	{
		bb_temp = attackers;
		/* Fill all eight adjacent squares by duplicating up and down and
			then duplicating those left and right. */
		attackers |= SHIFT_UP(attackers) | SHIFT_DN(attackers);
		attackers |= SHIFT_RT(attackers) | SHIFT_LF(attackers);
		attackers &= ~occupied;
		moves--;
		if (bb_temp == attackers || moves <= 0)
			break;
	}
	return attackers;
}

/* A big table is the fastest for this... */
static const int lsb_table[256] =
{
	0, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 
	5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 
	6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 
	5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 
	7, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 
	5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 
	6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 
	5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0 
};

/**
first_bit_8():
Returns the first bit that is set in a byte.
Created 120205; last modified 120205
**/
int first_bit_8(unsigned char b)
{
	return lsb_table[b]; 
}

static const int mul[4] =
{
	0x02020202, 0x02020202, 0x02020202, 0x01041041
};
static const int shift[4] =
{
	0, 0, 0, 3
};

/**
attack_index():
Returns the state of the masked ray as a 6-bit integer. The bits of
the occupancy can simply be smeared upward. As the outermost bits (on the A and
H files) will be empty, we do an implicit shift by doubling the multiplication
factor. These six bits end up in the most significant 6 bits of the product.
Created 082206; last modified 082206
**/
int attack_index(BITBOARD occ, DIRECTION dir)
{
	unsigned int u = (unsigned int)(occ | (occ >> 32) << shift[dir]);
	return u * mul[dir] >> 26;
}

/**
dir_attacks():
Returns a bitboard of attacks in a given direction from the given square.
We take the inner six bits of the occupied state along that direction (as the
outermost squares don't matter) and use that as an array index.
Created 082206; last modified 102806
**/
BITBOARD dir_attacks(SQUARE from, BITBOARD occupied, DIRECTION dir)
{
	occupied &= attack_mask[from][dir];
	return attack_bb[from][dir][attack_index(occupied, dir)];
}

/**
initialize_attacks():
Sets up the attack_mask and attack_bb arrays for the bitboard attack generation
functions.
Created 082206; last modified 090108
**/
void initialize_attacks(void)
{
	BITBOARD mask;
	BITBOARD submask;
	SQUARE square;
	int diag;

	for (square = 0; square < OFF_BOARD; square++)
	{
		/* Calculate the attack mask for ranks. */
		attack_mask[square][DIR_HORI] = MASK_RANK(RANK_OF(square)) &
			~(MASK_FILE(FILE_A) | MASK_FILE(FILE_H));

		/* Calculate the attack mask for A1H8 diagonals. */
		diag = FILE_OF(square) - RANK_OF(square);
		if (diag > 0)
			attack_mask[square][DIR_A1H8] =
				0x0040201008040200ull >> (diag * 8);
		else
			attack_mask[square][DIR_A1H8] =
				0x0040201008040200ull << (-diag * 8);
		/* Mask off the outer bits. */
		attack_mask[square][DIR_A1H8] &=
			~(MASK_RANK(RANK_1) | MASK_RANK(RANK_8));

		/* Calculate the attack mask for A8H1 diagonals. */
		diag = FILE_OF(square) + RANK_OF(square) - 7;
		if (diag > 0)
			attack_mask[square][DIR_A8H1] =
				0x0002040810204000ull << (diag * 8);
		else
			attack_mask[square][DIR_A8H1] =
				0x0002040810204000ull >> (-diag * 8);
		/* Mask off the outer bits. */
		attack_mask[square][DIR_A8H1] &=
			~(MASK_RANK(RANK_1) | MASK_RANK(RANK_8));

		/* Calculate the attack mask for files. */
		attack_mask[square][DIR_VERT] = MASK_FILE(FILE_OF(square)) &
			~(MASK_RANK(RANK_1) | MASK_RANK(RANK_8));
	}

	/* For each direction, loop over all possible bit-states for the occupied
		set, produce an attack board using the Kogge Stone fill routines, and
		store it in the attack arrays. This uses Steffen Westcott's subset
		traversing algorithm. */
	for (square = 0; square < OFF_BOARD; square++)
	{
		/* Rank attacks. */
		mask = attack_mask[square][DIR_HORI];
		submask = 0;
		do
		{
			submask = (submask - mask) & mask;
			attack_bb[square][DIR_HORI][attack_index(submask, DIR_HORI)] =
				SHIFT_RT(fill_right(MASK(square), ~submask)) |
				SHIFT_LF(fill_left(MASK(square), ~submask));
		} while (submask);
		/* A8H1 attacks. */
		mask = attack_mask[square][DIR_A1H8];
		submask = 0;
		do
		{
			submask = (submask - mask) & mask;
			attack_bb[square][DIR_A1H8][attack_index(submask, DIR_A1H8)] =
				SHIFT_DL(fill_down_left(MASK(square), ~submask)) |
				SHIFT_UR(fill_up_right(MASK(square), ~submask));
		} while (submask);
		/* ADIR_A1H8H8 attacks. */
		mask = attack_mask[square][DIR_A8H1];
		submask = 0;
		do
		{
			submask = (submask - mask) & mask;
			attack_bb[square][DIR_A8H1][attack_index(submask, DIR_A8H1)] =
				SHIFT_DR(fill_down_right(MASK(square), ~submask)) |
				SHIFT_UL(fill_up_left(MASK(square), ~submask));
		} while (submask);
		/* File attacks. */
		mask = attack_mask[square][DIR_VERT];
		submask = 0;
		do
		{
			submask = (submask - mask) & mask;
			attack_bb[square][DIR_VERT][attack_index(submask, DIR_VERT)] =
				SHIFT_UP(fill_up(MASK(square), ~submask)) |
				SHIFT_DN(fill_down(MASK(square), ~submask));
		} while (submask);
	}
}
