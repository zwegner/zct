/** ZCT/evalpiece.c--Created 121308 **/

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
#include "eval.h"
#include "bit.h"

/**
evaluate_knight():
Evaluates a knight on the given square.
Created 091306; last modified 120508
**/
VALUE evaluate_knights(COLOR color)
{
	BITBOARD knights;
	BITBOARD attacks;
	SQUARE square;
	VALUE r = 0;

	knights = board.color_bb[color] & board.piece_bb[KNIGHT];
	r = 0;

	FOR_BB(square, knights)
	{
		/* attacks */
		attacks = knight_moves_bb[square];
		attack_set[color][KNIGHT] |= attacks;

		/* mobility */
		r += evaluate_mobility(attacks, KNIGHT, color, square);
	}
	
	DEBUG_EVAL(print("knights[%C] = %V\n", color, r));
	
	return r;
}

/**
evaluate_bishop():
Evaluates a bishop on the given square.
Created 091306; last modified 120508
**/
VALUE evaluate_bishops(COLOR color)
{
	BITBOARD bishops;
	BITBOARD attacks;
	SQUARE square;
	VALUE r;

	bishops = board.color_bb[color] & board.piece_bb[BISHOP];
	r = 0;
	
	/* bishop pair */
	if (pop_count(bishops) >= 2)
		r += bishop_pair_value;

	FOR_BB(square, bishops)
	{
		/* pawn color value */
		r += board.pawn_entry.bishop_color_value[color][SQ_COLOR(square)];

		/* trapped on A7/H7 with pawn blocking */
		if (square == SQ_FLIP_COLOR(A7, color) &&
			board.piece[SQ_FLIP_COLOR(B6, color)] == PAWN &&
			board.color[SQ_FLIP_COLOR(B6, color)] == COLOR_FLIP(color))
			r += bishop_trapped_value;
		else if (square == SQ_FLIP_COLOR(H7, color) &&
			board.piece[SQ_FLIP_COLOR(G6, color)] == PAWN &&
			board.color[SQ_FLIP_COLOR(G6, color)] == COLOR_FLIP(color))
			r += bishop_trapped_value;
	
		/* attacks */
		attacks = BISHOP_ATTACKS(square, board.occupied_bb);
		attack_set[color][BISHOP] |= attacks;

		/* mobility */
		r += evaluate_mobility(attacks, BISHOP, color, square);
	}

	DEBUG_EVAL(print("bishops[%C] = %V\n", color, r));

	return r;
}

/**
evaluate_rook():
Evaluates a rook on the given square.
Created 110106; last modified 120508
**/
VALUE evaluate_rooks(COLOR color)
{
	BITBOARD rooks;
	BITBOARD attacks;
	SQUARE square;
	SQ_FILE f;
	VALUE r = 0;
	BOOL on_seventh;
	unsigned char open_file;

	rooks = board.color_bb[color] & board.piece_bb[ROOK];

	FOR_BB(square, rooks)
	{
		/* rook on seventh */
		on_seventh = (RANK_OF(SQ_FLIP_COLOR(square, color)) == RANK_7);
		r += rook_on_seventh_value[on_seventh];

		/* For the open file value, we need two bits, indicating whether the
			file has a pawn on it for each side. */
		f = FILE_OF(square);
		open_file = BIT_IS_SET(board.pawn_entry.open_files[color], f) |
			BIT_IS_SET(board.pawn_entry.open_files
				[COLOR_FLIP(color)], f) << 1;
		r += rook_open_file_value[open_file];

		/* Attacks: we ignore friendly rooks and queens because they can
		   be "attacked through". */
		attacks = ROOK_ATTACKS(square, board.occupied_bb &
			~((board.piece_bb[ROOK] | board.piece_bb[QUEEN]) &
				board.color_bb[color]));
		attack_set[color][ROOK] |= attacks;

		/* mobility */
		r += evaluate_mobility(attacks, ROOK, color, square);
	}

	DEBUG_EVAL(print("rooks[%C] = %V\n", color, eval_temp));

	return r;
}

/**
evaluate_queen():
Evaluates a queen on the given square.
Created 110106; last modified 120508
**/
VALUE evaluate_queens(COLOR color)
{
	BITBOARD queens;
	BITBOARD attacks;
	SQUARE square;
	VALUE r = 0;
	BOOL on_seventh;

	queens = board.color_bb[color] & board.piece_bb[QUEEN];

	FOR_BB(square, queens)
	{
		/* queen on seventh */
		on_seventh = (RANK_OF(SQ_FLIP_COLOR(square, color)) == RANK_7);
		r += rook_on_seventh_value[on_seventh];

		/* Attacks: we ignore friendly rooks and bishops (in the appropriate
		   direction) and queens because they can be "attacked through"  */
		attacks = ROOK_ATTACKS(square, board.occupied_bb &
				~((board.piece_bb[ROOK] | board.piece_bb[QUEEN]) &
					board.color_bb[color]));
		attacks |= BISHOP_ATTACKS(square, board.occupied_bb &
				~((board.piece_bb[BISHOP] | board.piece_bb[QUEEN]) &
					board.color_bb[color]));
		attack_set[color][QUEEN] |= attacks;

		/* mobility */
		r += evaluate_mobility(attacks, QUEEN, color, square);
	}

	DEBUG_EVAL(print("queens[%C] = %V\n", color, eval_temp));

	return r;
}

/**
evaluate_mobility():
Takes a given attack set and computes an evaluation based on what the
piece attacks.
Created 102706; last modifed 060108
**/
VALUE evaluate_mobility(BITBOARD attacks, PIECE piece, COLOR color,
	SQUARE square)
{
	VALUE r = 0;

	/* We score both "regular" and "safe" mobility. Safe mobility only includes
		squares that aren't attacked by opponent pawns. We look at the number
		of squares in each set and use that to index a value table. */
	r = mobility_value[piece][pop_count(attacks & ~board.color_bb[color])] +
		safe_mobility_value[piece][pop_count(attacks &
			board.pawn_entry.not_attacked[color])];
	return r;
}
