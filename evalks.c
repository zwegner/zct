/** ZCT/evalks.c--Created 121308 **/

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
evaluate_king_safety():
Evaluates the overall safety of the given color's king.
Created 110606; last modified 120508
**/
VALUE evaluate_king_safety(EVAL_BLOCK *eval_block, COLOR color)
{
	int r;
	BITBOARD king_area;
	BITBOARD pawn_attacks;
	BITBOARD piece_attacks[2][5];
	BOOL king_is_safe;
	COLOR c;
	PIECE piece;
	SHELTER shelter;
	SQUARE square;
	SQ_FILE file;
	VALUE eval_temp;
	VALUE eval;
	VALUE attack_value;

	eval = 0;
	square = board.king_square[color];
	/* Get the set of squares near the king: the 8 squares surrounding it
		and the 3 squares in front of them. */
	king_area = king_moves_bb[square];
	king_area |= SHIFT_FORWARD(king_area, color);

	good_squares[COLOR_FLIP(color)] |= king_area;
	/* piece attacks */
	for (r = 0; r < 2; r++)
	{
		c = (r == 0) ? color : COLOR_FLIP(color);

		pawn_attacks = SHIFT_FORWARD(board.piece_bb[PAWN] &
				board.color_bb[c], c);
		piece_attacks[r][PAWN] = SHIFT_LF(pawn_attacks) |
		   	SHIFT_RT(pawn_attacks);

		for (piece = KNIGHT; piece <= QUEEN; piece++)
			piece_attacks[r][piece] = attack_set[c][piece];
	}

	for (piece = PAWN; piece <= QUEEN; piece++)
		DEBUG_EVAL(print("%lI",piece_attacks[0][piece],piece_attacks[1][piece]));
	DEBUG_EVAL(print("king area:\n%I",king_area));
	/* Take the count of how many pieces of each type attack the area
		near the king. */
	r = 0;
	for (piece = PAWN; piece <= QUEEN; piece++)
		r += king_safety_att_weight[piece] *
		   	pop_count(piece_attacks[1][piece] & king_area);

	r = MIN(MAX(r, 0), 40);
	eval_temp = attack_value = king_safety_att_value[r];
	eval += eval_temp;
	DEBUG_EVAL(print("king safety: piece attacks[%C]=%V\n", color, eval_temp));

#if 0
	/* blocking */
	r = 0;
	for (piece = PAWN; piece <= QUEEN; piece++)
		r += king_safety_block_weight[piece] * pop_count(king_area &
			   	board.piece_bb[piece] & board.color_bb[color]);

	r = MIN(MAX(r, 0), 20);
	eval_temp = king_safety_block_value[r];
	eval += eval_temp;
	DEBUG_EVAL(print("king safety: blocking[%C]=%V\n", color, eval_temp));

	/* defending */
	r = 0;
//	king_area = king_moves_bb[square] & board.color_bb[color];
	for (piece = PAWN; piece <= QUEEN; piece++)
		r += king_safety_def_weight[piece] *
			pop_count(piece_attacks[0][piece] & king_area);

	r = MIN(MAX(r, 0), 40);
	eval_temp = (attack_value - king_safety_att_value[40]) *
	   	king_safety_def_percentage[r] / 100;
	eval += eval_temp;
	DEBUG_EVAL(print("king safety: defense factor[%C]=%i%%\n", color,
			   	king_safety_def_percentage[r]));
	DEBUG_EVAL(print("king safety: defense score[%C]=%V\n", color, eval_temp));
	DEBUG_EVAL(print("king safety: dynamic score[%C]=%V\n", color, eval));
#endif

	/* king shelter */
	eval_temp = 0;
	king_is_safe = FALSE;
	file = FILE_OF(square);
	for (shelter = KING_SIDE; shelter <= QUEEN_SIDE; shelter++)
	{
		if (file >= shelter_file[shelter][0] &&
			file <= shelter_file[shelter][1])
		{
			eval_temp += board.pawn_entry.
				king_shelter_value[color][shelter];
			/* King is only "safe" on the first rank. */
			if (RANK_OF(SQ_FLIP_COLOR(square, color)) == RANK_1)
				king_is_safe = TRUE;
		}
	}
	DEBUG_EVAL(print("king safety: king_is_safe[%C]=%i\n",
		color, king_is_safe));
	if (!king_is_safe)
	{
		/* If the king isn't safe, penalize for giving up castling rights. */
		if (!CAN_CASTLE(board.castle_rights, color))
			eval_temp += lost_castling_value[1];
		else
		{
			/* Check if we can still castle on one side. If we can,
				give a partial value for the pawn structure on that
				side, so we don't wreck it before castling. */
			/* King side */
			if (!CAN_CASTLE_KS(board.castle_rights, color))
				eval_temp += lost_castling_value[0];
			else
				eval_temp += board.pawn_entry.king_shelter_value
					[color][KING_SIDE] / 2;
			/* Queen side */
			if (!CAN_CASTLE_QS(board.castle_rights, color))
				eval_temp += lost_castling_value[0];
			else
				eval_temp += board.pawn_entry.king_shelter_value
					[color][QUEEN_SIDE] / 2;
		}
	}
	/* If the king is safe but got there without castling, we need to check for
		trapped rooks. Note that the king is on the first rank as a precondition
		for being safe, so we only need to look at the rook. */
	else if (trapped_rook_mask[color][file] &
		board.piece_bb[ROOK] & board.color_bb[color])
		eval_temp += trapped_rook_value;

	/* Interpolate shelter score: best in opening. */
	eval_temp = interpolate(eval_temp, phase, OPENING_PHASE);
	DEBUG_EVAL(print("king safety: shelter score[%C]=%V\n", color, eval_temp));

	eval += eval_temp;
	
	eval_block->king_safety[color] = eval;

	return eval;
}
