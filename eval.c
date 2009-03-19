/** ZCT/eval.c--Created 071505 **/

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

/* evaluation globals */
BITBOARD attack_set[2][6];
BITBOARD good_squares[2];
PHASE phase;

/**
material_balance():
Simply return the material advantage the side to move has.
Created 081608; last modified 081608
**/
VALUE material_balance(void)
{
	return board.material[board.side_tm] - board.material[board.side_ntm];
}

/**
interpolate():
Given an evaluation value, we interpolate it into a phase-appropriate
evaluation. We do this with the current game phase, as well as a "target" phase.
The target phase is the game phase in which the evaluation term should be at
its peak weight. For example, development bonuses should have a target of
around 0, and endgame bonuses a target of around MAX_PHASE.
Created 120508; last modified 120508
**/
VALUE interpolate(VALUE value, PHASE phase, PHASE target)
{
	return (value * (MAX_PHASE - ABS(target - phase))) / MAX_PHASE;
}

/**
evaluate():
Evaluates the board position.
Created 071505; last modified 091608
**/
VALUE evaluate(EVAL_BLOCK *eval_block)
{
	int r;
	BITBOARD pieces;
	COLOR color;
	EVAL_HASH_ENTRY *eval_hash_entry;
	PIECE piece;
	SQUARE square;
	VALUE eval[2];
	VALUE eval_temp;
	VALUE eval_temp_2;

	/* Look up this position in the eval hash table. */
	eval_hash_entry = &eval_hash_table[board.hashkey % zct->eval_hash_size];
	zct->eval_hash_probes++;
	if (eval_hash_entry && eval_hash_entry->hashkey == board.hashkey)
	{
		zct->eval_hash_hits++;
		DEBUG_EVAL(print("eval hash hit = %V\n", eval_hash_entry->eval.eval));
		*eval_block = eval_hash_entry->eval;
		return eval_hash_entry->eval.eval[board.side_tm] -
			eval_hash_entry->eval.eval[board.side_ntm];
	}

	eval[WHITE] = 0;
	eval[BLACK] = 0;

	/* Determine the game phase. */
	r = 0;
	/* 16 -- piece count */
	r += board.piece_count[WHITE] + board.piece_count[BLACK];
	/*  8 -- castle rights white */
	r += 8 * (CAN_CASTLE(board.castle_rights, WHITE) != 0);
	/*  8 -- castle rights black */
	r += 8 * (CAN_CASTLE(board.castle_rights, BLACK) != 0);
	/* 16 -- pawn count */
	r += board.pawn_count[WHITE] + board.pawn_count[BLACK];
	/* 16 -- minor piece count */
	r += 2 * MIN(pop_count(board.piece_bb[KNIGHT] | board.piece_bb[BISHOP]), 8);
	/* 16 -- rook count */
	r += 4 * MIN(pop_count(board.piece_bb[ROOK]), 4);
	/* 16 -- queen count */
	r += 8 * MIN(pop_count(board.piece_bb[QUEEN]), 2);
	/* 32 -- development: occupancy of first two ranks */
	r += 2*pop_count(board.occupied_bb &
		(MASK_RANK(RANK_1)/* | MASK_RANK(RANK_2) |
		 MASK_RANK(RANK_7)*/ | MASK_RANK(RANK_8)));

	phase = MAX_PHASE - r;
	phase = MAX(0, MIN(MAX_PHASE, phase));
	eval_block->phase = phase;
	ASSERT(phase >= 0 && phase <= MAX_PHASE);
	DEBUG_EVAL(print("phase = %i/%i\n", phase, MAX_PHASE));

	/* Initialize. */
	for (color = WHITE; color <= BLACK; color++)
	{
		for (piece = PAWN; piece <= KING; piece++)
			attack_set[color][piece] = 0;
		good_squares[color] = 0;
	}

	/* First pass: evaluate simple terms, material and development */
	for (color = WHITE; color <= BLACK; color++)
	{
		/* Material value and piece square tables. */
		pieces = board.color_bb[color] & ~board.piece_bb[KING];
		eval_temp = board.material[color];
		eval_temp_2 = 0;
		while (pieces)
		{
			square = first_square(pieces);
			CLEAR_BIT(pieces, square);
			piece = board.piece[square];
			eval_temp += piece_square_value[color][piece][square];
			eval_temp_2 += piece_endgame_value[piece];
		}
		DEBUG_EVAL(print("piece square[%C](op,eg) = %V %V\n", color,
			eval_temp, eval_temp_2));

		eval[color] += interpolate(eval_temp, phase, OPENING_PHASE);
		eval[color] += interpolate(eval_temp_2, phase, ENDGAME_PHASE);

		/* Development. */
		eval_temp = 0;
		eval_temp += development_value[pop_count(board.color_bb[color] &
			(board.piece_bb[KNIGHT] | board.piece_bb[BISHOP]) &
			development_mask[color])];
		if (eval_temp < development_value[1] &&
			board.piece[SQ_FLIP_COLOR(D1, color)] != QUEEN)
			eval_temp += early_queen_value;
		DEBUG_EVAL(print("development[%C] = %V\n", color, eval_temp));

		eval[color] += interpolate(eval_temp, phase, OPENING_PHASE);
	}

	/* Evaluate pawn structure. */
	eval_temp = evaluate_pawns();
	DEBUG_EVAL(print("pawns = %V\n", eval_temp));
	eval[board.side_tm] += eval_temp;

	/* Second pass: evaluate more complex terms */
	for (color = WHITE; color <= BLACK; color++)
	{
		eval[color] += evaluate_knights(color);
		eval[color] += evaluate_bishops(color);
		eval[color] += evaluate_rooks(color);
		eval[color] += evaluate_queens(color);
	}

	/* Third pass: evaluate terms based on previous calculations,
		primarily the attack tables. */
	for (color = WHITE; color <= BLACK; color++)
	{
		/* Evaluate king safety. */
		eval_temp = evaluate_king_safety(eval_block, color);
		DEBUG_EVAL(print("king safety[%C] = %V\n", color, eval_temp));
		eval[color] += eval_temp;

		/* Evaluate passed pawns. */
		eval_temp = evaluate_passed_pawns(eval_block, color);
		DEBUG_EVAL(print("passed pawns[%C] = %V\n", color, eval_temp));
		eval[color] += eval_temp;

		/* Evaluate endgame specific features. */
		eval_temp = evaluate_endgame(color);
		DEBUG_EVAL(print("endgame eval[%C] = %V\n", color, eval_temp));
		eval[color] += eval_temp;
	}

	/* Side to move bonus. */
	eval[board.side_tm] += interpolate(side_tm_value, phase, 0);

	/* Final score */
	for (color = WHITE; color <= BLACK; color++)
	{
		DEBUG_EVAL(print("total score[%C] = %V\n", color, eval[color]));

		eval_block->eval[color] = eval[color];

		/* Stuff some eval data into the evaluation block. */
		eval_block->good_squares[color] = good_squares[color];
	}

	/* Final evaluation. */
	eval_temp = eval[board.side_tm] - eval[board.side_ntm];

	/* Store the evaluation in the hash table. */
	eval_block->full_eval = eval_temp;
	eval_hash_entry->hashkey = board.hashkey;
	eval_hash_entry->eval = *eval_block;

	return eval_temp;
}
