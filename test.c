/** ZCT/test.c--Created 110506 **/

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
#include "cmd.h"
#include "pgn.h"
#include <math.h>

BOOL correct;
int solution_time;
int solution_depth;
/* This is used to count how many plies in a row the correct answer was found.
	This is used for early cutoffs when the solution looks mostly correct. */
int correct_length;
int correct_count;
int depth_total;
int time_total;
int time_sq_total;
EPD_POS *epd_pos;

BOOL epd_test_pos_func(void *arg, POS_DATA *pos);
BOOL eval_test_pos_func(void *arg, POS_DATA *pos);

/**
test_epd():
Runs ZCT on a test suite from the given EPD file. Statistics are collected and
printed at the end.
Created 110506; last modified 111308
**/
void test_epd(char *file_name)
{
	int positions;

	/* Initialize. */
	zct->engine_state = TESTING;
	correct_count = 0;
	depth_total = 0;
	time_total = 0;
	time_sq_total = 0;

	/* Run through the EPD, using the test POS_FUNC to do the actual testing. */
	positions = epd_load(file_name, epd_test_pos_func, NULL);

	/* Print the results and some statistics. */
	print("score=%i/%i %.1f%%\n", correct_count, positions,
		(float)100.0 * correct_count / positions);
	print("avg.depth=%.1f avg.time=%T avg.time^2=%.2f\n",
		(float)depth_total / positions, time_total / positions,
		((float)10000 * time_sq_total / positions) / 1000);

	zct->engine_state = IDLE;
}

/**
epd_test_pos_func():
When running a test suite through an EPD file, this function is used to carry
out the actual processing, that is, searching and tabulating statistics.
Created 111208; last modified 111308
**/
BOOL epd_test_pos_func(void *arg, POS_DATA *pos)
{
	int x;

	epd_pos = pos->epd;

	/* Print out some header information. */
	print("-------------------------------------------\n");
	print("id=%s %s=", epd_pos->id,
		epd_pos->type == EPD_BEST_MOVE ? "bm" : "am");
	for (x = 0; x < epd_pos->solution_count; x++)
		print("%M ", epd_pos->solution[x]);
	print("fen=%s\n", epd_pos->fen);

	correct_length = 0;
	correct = FALSE;
	search_root();
	/* Break on any input. */
	if (zct->input_buffer[0])
		return TRUE;

	/* Update statistics. */
	if (correct)
		correct_count++;
	depth_total += solution_depth;
	time_total += solution_time;
	/* Convert to tenths of a second so as to not overflow the
		time_sq_total counter. */
	solution_time /= 100;
	time_sq_total += solution_time * solution_time;

	return FALSE;
}

/**
check_solutions():
During the search, check the best move to see if it is one of the EPD test's
solutions.
Created 110506; last modified 111308
**/
BOOL check_solutions(MOVE move, int depth)
{
	int x;

	if (correct)
		correct_length++;
	correct = FALSE;
	for (x = 0; x < epd_pos->solution_count; x++)
	{
		if (epd_pos->solution[x] == move)
		{
			correct = TRUE;
			break;
		}
	}
	solution_depth = depth;
	solution_time = time_used();
	if (!correct)
		correct_length = 0;
	else if (correct_length >= 2)
		return TRUE;
	return FALSE;
}

/**
test_eval():
Runs ZCT on a test suite from the given EPD file. Each position is evaluated,
flipped, and evaluated again to verify that the evaluation is symmetric.
Created 092907; last modified 111208
**/
void test_epd_eval(char *file_name)
{
	int positions;

	correct_count = 0;

	positions = epd_load(file_name, eval_test_pos_func, NULL);

	print("score=%i/%i %.1f%%\n", correct_count, positions,
		(float)100.0 * correct_count / positions);
}

/**
eval_test_pos_func():
When running through a test suite, take the current position and make sure
that it has the same evaluation after being flipped.
Created 111208; last modified 111208
**/
BOOL eval_test_pos_func(void *arg, POS_DATA *pos)
{
	EVAL_BLOCK eval_block;
	VALUE old_eval;
	VALUE new_eval;

	old_eval = evaluate(&eval_block);
	flip_board();
	new_eval = evaluate(&eval_block);
	/* Flip the board back, in case we're reading a PGN or something... */
	flip_board();

	if (old_eval != new_eval)
		print("failed: old=%V new=%V\nfen=%F\n%B",
			old_eval, new_eval, &board, &board);
	else
		correct_count++;

	return FALSE;
}

/**
flip_board():
This flips the board position w.r.t side to move as well as top-to-bottom. This creates a
completely symmetrically equivalent position.
Created 092507; last modified 092907
**/
void flip_board(void)
{
	COLOR color_temp;
	PIECE piece_temp;
	SQUARE square;

	board.side_tm = board.side_ntm;
	board.side_ntm = COLOR_FLIP(board.side_ntm);
	if (board.ep_square != OFF_BOARD)
		board.ep_square = SQ_FLIP(board.ep_square);
	board.castle_rights = (board.castle_rights & 5) << 1 |
		(board.castle_rights & 10) >> 1;
	for (square = A1; square < SQ_FLIP(square); square++)
	{
		piece_temp = board.piece[square];
		board.piece[square] = board.piece[SQ_FLIP(square)];
		board.piece[SQ_FLIP(square)] = piece_temp;

		color_temp = board.color[square];
		board.color[square] = board.color[SQ_FLIP(square)];
		board.color[SQ_FLIP(square)] = color_temp;
		if (board.color[square] != EMPTY)
			board.color[square] = COLOR_FLIP(board.color[square]);
		if (board.color[SQ_FLIP(square)] != EMPTY)
			board.color[SQ_FLIP(square)] =
				COLOR_FLIP(board.color[SQ_FLIP(square)]);
	}
	initialize_bitboards();
	initialize_hashkey();
}
