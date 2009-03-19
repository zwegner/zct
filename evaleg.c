/** ZCT/evaleg.c--Created 121308 **/

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
evaluate_endgame():
Evaluates all matters pertaining to the endgame.
Created 100807; last modified 052508
**/
VALUE evaluate_endgame(COLOR color)
{
	BITBOARD bb_temp;
	SQUARE square;
	VALUE eval_temp;
	VALUE eval;

	eval = 0;
	/* Evaluate single bishops, trying to force the opponent king into the
		appropriate corner. */
	if (pop_count(board.piece_bb[BISHOP] & board.color_bb[color]) == 1)
	{
		square = first_square(board.piece_bb[BISHOP] & board.color_bb[color]);
		/* Get a value for the king being pushed into the corner, based on the
			opponent's bishop square color. The king will thus avoid corners
			where the bishop can attack it. */
		eval_temp = king_bishop_square_value[
			SQ_FLIP_COLOR(board.king_square[COLOR_FLIP(color)],
				SQ_COLOR(square))];
		eval += eval_temp;
	}
	/* Evaluate how rooks restrict king movement. */
	if (board.piece_bb[ROOK] & board.color_bb[color])
	{
		/* Get a bitboard of all squares that enemy rooks attack. */
		bb_temp = fill_attacks_rook(board.piece_bb[ROOK] &
			board.color_bb[color], board.occupied_bb) |
			(board.piece_bb[ROOK] & board.color_bb[color]);

		/* Count the squares that the king can reach while remaining inside
			the rectangle formed by the enemy rook. If there are pieces
			blocking the rook's attacks, or if the king is being attacked,
			it can venture into the adjacent rectangles of course. */
		eval_temp = pop_count(flood_fill_king(
			MASK(board.king_square[COLOR_FLIP(color)]), bb_temp, 0));
		eval += eval_temp;
	}
	/* Evaluate king centralization. */
	eval_temp = king_endgame_square_value[board.king_square[color]];
	eval += eval_temp;
	/* Evaluate distance between the enemy king. We only evaluate if the we
		have less material, meaning that we want to keep the king away. */
	if (board.piece_count[color] < board.piece_count[COLOR_FLIP(color)])
	{
		eval_temp = 8 - DISTANCE(board.king_square[color],
			board.king_square[COLOR_FLIP(color)]);
		eval_temp = eval_temp * eval_temp;
		eval += -eval_temp;
	}

	return interpolate(eval, phase, ENDGAME_PHASE);
}

/**
can_mate():
This function determines if, based on the material configuration, it is
possible for the given side to force mate.
Created 011508; last modified 020208
**/
BOOL can_mate(COLOR color)
{
	/* If this side has at least one pawn, rook, or queen, it can mate. */
	if (board.piece_bb[PAWN] & board.color_bb[color])
		return TRUE;
	if (board.piece_bb[ROOK] & board.color_bb[color])
		return TRUE;
	if (board.piece_bb[QUEEN] & board.color_bb[color])
		return TRUE;
	/* If this side has no pawns, it must have at least two minors. */
	if (pop_count((board.piece_bb[KNIGHT] | board.piece_bb[BISHOP]) &
		board.color_bb[color]) > 1)
		return TRUE;
	return FALSE;
}
