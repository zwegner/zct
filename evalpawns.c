/** ZCT/evalpawns.c--Created 120508 **/

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
evaluate_passed_pawns():
Evaluate any passed pawns that might be on the board.
Created 110506; last modified 060308
**/
VALUE evaluate_passed_pawns(EVAL_BLOCK *eval_block, COLOR color)
{
	int distance;
	int multiplier;
	BITBOARD pawns;
	BITBOARD friendly;
	BITBOARD rook_defend;
	BITBOARD rook_attack;
	BITBOARD attackers[6];
	SQUARE square;
	SQUARE front_square;
	VALUE eval_temp;

	pawns = board.pawn_entry.passed_pawns & board.color_bb[color];
	eval_temp = 0;
	/* Get friendly and enemy rook attacks, from both the front and back. */
	rook_defend = SHIFT_FORWARD(fill_forward[color](board.piece_bb[ROOK] &
			board.color_bb[color], ~board.occupied_bb), color);
	rook_attack = SHIFT_FORWARD(fill_forward[color](board.piece_bb[ROOK] &
			board.color_bb[COLOR_FLIP(color)], ~board.occupied_bb), color) |
		SHIFT_FORWARD(fill_forward[COLOR_FLIP(color)](board.piece_bb[ROOK] &
			board.color_bb[COLOR_FLIP(color)], ~board.occupied_bb),
			COLOR_FLIP(color));
	while (pawns)
	{
		square = first_square(pawns);
		CLEAR_BIT(pawns, square);

		distance = 7 - RANK_OF(SQ_FLIP_COLOR(square, color));
		multiplier = 8;
		front_square = square;
//		attackers[KNIGHT] = fill_attacks_knight(board.piece_bb[KNIGHT] & board.color_bb[COLOR_FLIP(color)]);
//		attackers[BISHOP] = fill_attacks_bishop(board.piece_bb[BISHOP] & board.color_bb[COLOR_FLIP(color)], board.occupied_bb);
		while (TRUE)
		{
			front_square += pawn_step[color];

			good_squares[COLOR_FLIP(color)] |= MASK(front_square);

			if (board.color[front_square] != EMPTY)
			{
				multiplier -= 3;
				break;
			}
			if (is_attacked(front_square, COLOR_FLIP(color)) &&
				!is_attacked(front_square, color))
			{
				multiplier -= 6;
				break;
			}
			else if (RANK_OF(SQ_FLIP_COLOR(front_square, color)) == RANK_8)
			{
				multiplier = 24;
				if (is_attacked(front_square, color))
					multiplier += 4;
				break;
			}
			if (is_attacked(front_square, color))
				multiplier++;
			multiplier += 2;
		}
		if (rook_defend & MASK(square))
			multiplier += 6;
		if (rook_attack & MASK(square))
			multiplier -= 6;
		/* Connected passed pawns */
		friendly = board.piece_bb[PAWN] & board.color_bb[color];
		/* Side-by-side pawns: only give half a bonus, as it can be 
			added for both sides. */
		if (friendly & (SHIFT_LF(MASK(square)) | SHIFT_RT(MASK(square))))
		{
			eval_temp += connected_pp_value / 2;
			multiplier += 2;
		}
		/* Diagonally adjacent pawns */
		if (SHIFT_FORWARD(friendly, color) &
			(SHIFT_LF(MASK(square)) | SHIFT_RT(MASK(square))))
		{
			eval_temp += connected_pp_value;
			multiplier += 4;
		}
		front_square = SQ_FROM_RF(RANK_8, FILE_OF(square));
		front_square = SQ_FLIP_COLOR(front_square, color);
		multiplier -= DISTANCE(square, board.king_square[color]);
		multiplier += DISTANCE(front_square,
			board.king_square[COLOR_FLIP(color)]);

		multiplier = MAX(multiplier, 0);
		multiplier = MIN(multiplier, 32);
		eval_temp += multiplier * passed_pawn_value[distance] / 8;
	}

	eval_block->passed_pawn[color] = eval_temp;

	return interpolate(eval_temp, phase, ENDGAME_PHASE);
}

/**
evaluate_pawns():
Evaluates the pawn structure.
Created 090106; last modified 082607
**/
VALUE evaluate_pawns(void)
{
	int pawn_color_count;
	COLOR color;
	COLOR sq_color;
	BITBOARD temp;
	BITBOARD pawns[2];
	BITBOARD pawn_attacks_left[2];
	BITBOARD pawn_attacks_right[2];
	BITBOARD pawn_attacks[2];
	BITBOARD smear[2];
	BITBOARD controlled[2];
	BITBOARD safe_path[2];
	BITBOARD weak[2];
	BITBOARD doubled_pawns;
	PAWN_HASH_ENTRY *ph_entry;
	SHELTER shelter;
	SQUARE square;
	SQ_RANK rank;
	SQ_FILE file;
	VALUE eval_temp;

	ph_entry = &pawn_hash_table[board.pawn_entry.hashkey %
		zct->pawn_hash_size];

	zct->pawn_hash_probes++;
	if (ph_entry->hashkey != 0 &&
		ph_entry->hashkey == board.pawn_entry.hashkey)
	{
		zct->pawn_hash_hits++;
		board.pawn_entry = *ph_entry;
		DEBUG_EVAL(print("pawn hash hit.\n"));
		return ph_entry->eval[board.side_tm] -
			ph_entry->eval[board.side_ntm];
	}

	board.pawn_entry.eval[WHITE] = board.pawn_entry.eval[BLACK] = 0;
//	board.pawn_entry.doubled_pawns = (BITBOARD)0;
	board.pawn_entry.passed_pawns = (BITBOARD)0;

#if 0
	for (color = WHITE; color <= BLACK; color++)
		pawns[color] = board.piece_bb[PAWN] & board.color_bb[color];

	/* Open files */
	for (color = WHITE; color <= BLACK; color++)
	{
		board.pawn_entry.open_files[color] =
			(unsigned char)smear_down(pawns[color]);
		/* Evaluate king-pawn shelters on king and queen side. */
		for (shelter = KING_SIDE; shelter <= QUEEN_SIDE; shelter++)
		{
			eval_temp = 0;
			for (file = shelter_file[shelter][0];
				file <= shelter_file[shelter][1]; file++)
			{
				temp = pawns[color] & MASK_FILE(file);
				if (!temp)
					eval_temp += 10;
				else
				{
					if (color == WHITE)
						square = first_square(temp);
					else
						square = SQ_FLIP(last_square(temp));

					rank = RANK_7 - RANK_OF(square);
					eval_temp += (64 - (1 << rank)) / 8;
				}
			}
			eval_temp = MIN(eval_temp, 30);
			board.pawn_entry.king_shelter_value[color][shelter] =
				king_shelter_value[eval_temp];
		}
		/* Count the number of pawns on each square color for bishop eval. */
		for (sq_color = WHITE; sq_color <= BLACK; sq_color++)
		{
			pawn_color_count = pop_count(board.piece_bb[PAWN] &
				board.color_bb[color] & SQ_COLOR_MASK(sq_color));
			/* Find the difference between pawn count of this color and
				total pawn count. */
			eval_temp = pawn_color_count -
				pop_count(board.piece_bb[PAWN] & board.color_bb[color]);
			board.pawn_entry.bishop_color_value[color][sq_color] =
				eval_temp * eval_temp;
		}
		/* Find the forward smears of pawns for calculating passed pawns. */
		smear[color] = smear_forward[color](pawns[color]);
	}
	/* Passed pawns */
	board.pawn_entry.passed_pawns |= ~(SHIFT_DL(smear[BLACK]) |
		SHIFT_DR(smear[BLACK]) | smear[BLACK]) & pawns[WHITE];
	board.pawn_entry.passed_pawns |= ~(SHIFT_UL(smear[WHITE]) |
		SHIFT_UR(smear[WHITE]) | smear[WHITE]) & pawns[BLACK];

	DEBUG_EVAL(print("passed pawns:\n%lI",
		board.pawn_entry.passed_pawns & board.color_bb[WHITE],
		board.pawn_entry.passed_pawns & board.color_bb[BLACK]));
#else
//	board.pawn_entry.blocked_pawns = (BITBOARD)0;
	/* First pass */
	for (color = WHITE; color <= BLACK; color++)
	{
		pawns[color] = board.piece_bb[PAWN] & board.color_bb[color];
		pawn_attacks_left[color] = SHIFT_LF(SHIFT_FORWARD(pawns[color], color));
		pawn_attacks_right[color] = SHIFT_RT(SHIFT_FORWARD(pawns[color], color));
		pawn_attacks[color] = pawn_attacks_left[color] | pawn_attacks_right[color];
		smear[color] = smear_forward[color](pawns[color]);
		/* Open files */
		if (color == WHITE)
			board.pawn_entry.open_files[WHITE] = (unsigned char)smear_down(smear[WHITE]);
		else
			board.pawn_entry.open_files[BLACK] = (unsigned char)smear[BLACK];
		/* Doubled pawns */
	//	board.pawn_entry.doubled_pawns |= pawns[color] & SHIFT_FORWARD(smear[color], color);
		doubled_pawns = pawns[color] & SHIFT_FORWARD(smear[color], color);
		eval_temp = pop_count(doubled_pawns & board.color_bb[color]) *
			doubled_pawn_value;
//		DEBUG_EVAL(print("doubled pawns[%C] = %V\n", color, eval_temp));
		board.pawn_entry.eval[color] += eval_temp;
		/* Count the number of pawns on each square color. */
		for (sq_color = WHITE; sq_color <= BLACK; sq_color++)
		{
			pawn_color_count = pop_count(board.piece_bb[PAWN] & board.color_bb[color] & SQ_COLOR_MASK(sq_color));
			eval_temp = pawn_color_count - pop_count(board.piece_bb[PAWN] & board.color_bb[color]);
			board.pawn_entry.bishop_color_value[color][sq_color] = eval_temp * eval_temp;
		}
	}
	/* Second pass */
	for (color = WHITE; color <= BLACK; color++)
	{
		/* Passed pawns */
		if (color == WHITE)
			board.pawn_entry.passed_pawns |= ~(SHIFT_DL(smear[BLACK]) |
			SHIFT_DR(smear[BLACK]) | smear[BLACK]) & pawns[WHITE];
		else
			board.pawn_entry.passed_pawns |= ~(SHIFT_UL(smear[WHITE]) |
			SHIFT_UR(smear[WHITE]) | smear[WHITE]) & pawns[BLACK];
		DEBUG_EVAL(print("passed pawns[%C]:\n%I", color, board.pawn_entry.passed_pawns & board.color_bb[color]));
		/* Find squares with more pawns of this color attacking it than the opposite color. */
		controlled[color] = (pawn_attacks[color] & ~pawn_attacks[COLOR_FLIP(color)]) |
			((pawn_attacks_left[color] & pawn_attacks_right[color]) &
			~(pawn_attacks_left[COLOR_FLIP(color)] & pawn_attacks_right[COLOR_FLIP(color)]));
		/* Find pawns blocked by pawns of the opposite color. */
//		board.pawn_entry.blocked_pawns |= SHIFT_FORWARD(pawns[COLOR_FLIP(color)], COLOR_FLIP(color)) & pawns[color];
		/* Remember squares not controlled by opponent pawns. */
		board.pawn_entry.not_attacked[color] = ~pawn_attacks[COLOR_FLIP(color)];
		/* Evaluate king-pawn shelters on king and queen side. */
		for (shelter = KING_SIDE; shelter <= QUEEN_SIDE; shelter++)
		{
			eval_temp = 0;
			for (file = shelter_file[shelter][0];
				file <= shelter_file[shelter][1]; file++)
			{
				temp = pawns[color] & MASK_FILE(file);
				if (!temp)
					eval_temp += 10;
				else
				{
					if (color == WHITE)
						square = first_square(temp);
					else
						square = SQ_FLIP(last_square(temp));

					rank = RANK_7 - RANK_OF(square);
					eval_temp += (64 - (1 << rank)) / 8;
				}
			}
			eval_temp = MIN(eval_temp, 30);
			board.pawn_entry.king_shelter_value[color][shelter] =
				king_shelter_value[eval_temp];
		}
	}

	/* Third pass */
	for (color = WHITE; color <= BLACK; color++)
	{
		/* Find paths that pawns can safely advance through. */
		safe_path[color] = fill_forward[color](pawns[color], ~(board.piece_bb[PAWN] | controlled[COLOR_FLIP(color)]));
		/* Find weak pawns. */
		temp = SHIFT_FORWARD(fill_forward[color](pawns[color], safe_path[color]), color);
		weak[color] = SHIFT_FORWARD(~board.piece_bb[PAWN] & controlled[COLOR_FLIP(color)], COLOR_FLIP(color)) &
			~(SHIFT_LF(temp) | SHIFT_RT(temp)) & pawns[color];

		eval_temp = pop_count(weak[color]) * weak_pawn_value;
		DEBUG_EVAL(print("weak pawns[%C] = %V\n", color, eval_temp));
		board.pawn_entry.eval[color] += eval_temp;
	}
#endif
	*ph_entry = board.pawn_entry;
	return board.pawn_entry.eval[board.side_tm] -
		board.pawn_entry.eval[board.side_ntm];
}
