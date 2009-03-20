/** ZCT/zct.c--Created 060205 **/

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
#include "smp.h"

int main(void)
{
	FILE *init_file;

	/* Call all the initialization functions. These are in a specific order!! */
	initialize_settings();
	initialize_data();
	initialize_cmds();
	initialize_eval();
	initialize_attacks();
	initialize_board(NULL);
#ifdef SMP
	initialize_smp(MAX_CPUS);
#else
	zct->process_count = 1;
#endif

#ifdef CLUSTER
	initialize_cluster();
#endif

	/* Read config file. */
	init_file = fopen("ZCT.ini", "rt");
	if (init_file != NULL)
	{
		zct->source = TRUE;
		zct->input_stream = init_file;
	}

	/* The main processing loop. Check the game state, see if we should
		move, ponder, read input, process input, etc. */
	while (TRUE)
	{
		/* Book keeping... */
		generate_root_moves();
		if (zct->protocol != UCI)
			check_result(TRUE);
		if (zct->protocol != ANALYSIS && zct->protocol != ANALYSIS_X &&
			zct->protocol != DEBUG)
			update_clocks();
		if (zct->ping)
		{
			if (zct->protocol == XBOARD)
				print("pong %i\n", zct->ping);
			else
				print("readyok\n");
			zct->ping = 0;
		}
		/* Check if we should move. */
		if ((zct->engine_state == DEBUGGING || zct->protocol == ANALYSIS ||
			zct->protocol == ANALYSIS_X || board.side_tm == zct->zct_side) &&
			!zct->game_over)
		{
			/* Set the engine state and search for a move. */
			if (zct->protocol == ANALYSIS || zct->protocol == ANALYSIS_X)
				zct->engine_state = ANALYZING;
			else if (zct->engine_state != DEBUGGING &&
				zct->engine_state != INFINITE)
				zct->engine_state = NORMAL;
			search_root();
			zct->engine_state = IDLE;

			/* Output/make the move. */
			if (zct->protocol != DEBUG)
			{
				if (board.pv_stack[0][0] != NO_MOVE)
				{
					if (zct->protocol == DEFAULT)
					{
						prompt();
						print("%M\n", board.pv_stack[0][0]);
					}
					else if (zct->protocol == XBOARD)
						print("move %M\n", board.pv_stack[0][0]);
					else if (zct->protocol == UCI)
					{
						print("bestmove %M", board.pv_stack[0][0]);
						if (board.pv_stack[0][1] != NO_MOVE)
							print(" ponder %M", board.pv_stack[0][1]);
						print("\n");
						zct->zct_side = EMPTY;
					}
					if (zct->protocol != ANALYSIS &&
						zct->protocol != ANALYSIS_X && zct->protocol != UCI)
					{
						make_move(board.pv_stack[0][0]);
						continue;
					}
				}
				else
					print("No move.\n");
			}
		}
		/* See if we should ponder. */
		else if (zct->hard && zct->zct_side != EMPTY &&
			board.game_entry > board.game_stack &&
			zct->input_buffer[0] == '\0' && !zct->game_over &&
			zct->protocol != UCI)
			ponder();

		prompt();
		/* Read input. If input is already waiting, we print the line out
			to verify that we have read it. */
		if (zct->input_buffer == NULL || zct->input_buffer[0] == '\0')
			read_line();
		else if (zct->protocol != XBOARD && zct->protocol != UCI)
			print("%s\n", zct->input_buffer);
		
		/* Try to process the line. Read it as a command, and if it fails,
			assume it is a move. The input_move() function will barf if the
			input isn't really a move. */
		if (command(zct->input_buffer) == CMD_BAD && zct->protocol != UCI)
			input_move(zct->input_buffer, INPUT_USER_MOVE);
		/* Clear the input buffer. */
		zct->input_buffer[0] = '\0';
	}
}

/**
prompt():
If the user is using console mode, this prompts for input depending on the
engine mode.
Created 081906; last modified 110608
**/
void prompt(void)
{
	char *mode;

	if (zct->source)
		return;

	if (input_available())
	{
		print("\n");
		return;
	}

	/* Determine the prompt mode based on the protocol. */
	mode = NULL;
	if (zct->protocol == DEFAULT)
		mode = "";
	else if (zct->protocol == DEBUG)
		mode = ".debug";
	else if (zct->protocol == ANALYSIS)
		mode = ".analyze";

	/* Print the prompt. */
	if (mode != NULL)
		print("(zct%s)%i.%s ", mode, board.move_number,
			board.side_tm == WHITE ? "" : "..");
}

/**
bench():
Makes a standard measure of performance on a set of positions.
Created 013008; last modified 031809
**/
void bench(void)
{
	BOOL old_post;
	BITBOARD total_nodes;
	int time;
	struct bench_position
	{
		char *fen;
		int depth;
	} position[] =
	{
		{ "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -", 15 },
		{ "2Q2bk1/5pp1/p3p2p/q1p1P2P/1pP5/1P2B1P1/P4PK1/8 w - -", 20 },
		{ "r1bqkb1r/pp1n1ppp/2p1pn2/8/2BP4/2N1PN2/PP3PPP/R1BQK2R b KQkq -",
			13 },
		{ "rnbqk2r/pp2ppbp/3p1np1/8/3NP3/2N5/PPP1BPPP/R1BQK2R w KQkq -", 12 },
		{ "4q1kr/p6p/1prQPppB/4n3/4P3/2P5/PP2B2P/R5K1 w - -", 16 },
		{ "k2N4/1qpK1p2/1p6/1P4p1/1P4P1/8/8/8 w - -", 26 },
		{ "5k2/1p4pP/p7/1p1p4/8/2r3K1/6PP/8 w - -", 15 },
		{ "8/2R5/p2P4/1kP2p2/6rb/P3pp2/1P6/2K4R w - - 0 1",  11 },
        { "r2q1rk1/4Qpp1/b6p/1p6/3pB1p1/8/PP4PP/R4RK1 w - - 2 1",  13 },
        { "r1b1Nn2/pp3p1k/2p3p1/q5Pp/8/1P4QP/P3PPB1/5RK1 b - - 1 1",  13 },
        { "q2knr2/2nb2pp/1p1p4/rPpPbpP1/P1B2N1P/2N1PR2/1BQK4/R7 w - - 1 1",
			13 },
        { "r1b2rk1/pp3ppB/4pq2/2n5/3N4/8/PPP2PPP/R2QR1K1 b - - 0 1", 16 },
        { "r2r2k1/1bq1bp1p/p1nppp2/8/4PP2/1NPBB2Q/P1P4P/4RRK1 b - - 0 1", 13 },
        { "r2q1rk1/1ppb1pb1/3p1npp/p2Pp3/2P1P2B/2NB4/PP3PPP/R2Q1RK1 b - - 0 1",
			14 },
        { "3rr1k1/2Rb1pp1/7p/1p1P4/1R2P3/P4N2/6PP/6K1 b - - 3 1",  14 },
		/*
        { "1r1q2k1/2n5/pp1p3p/2pP1Pp1/P4n2/1NP2P2/4rB1P/R2Q1R1K w - - 0 1",  12 },
        { "5k2/R7/5p2/2p1rNpp/2P2p2/5K2/P7/8 w - - 7 1",  12 },
        { "2rr2k1/4q1b1/b2pPpp1/4p3/1p2P1pP/1P4P1/PB1R1RB1/1Q5K w - - 3 1",  13 },
        { "r3rnk1/p2bqppp/1p6/2p5/3PN3/1Q2P3/PP1N1PPP/4RRK1 b - - 1 1",  11 },
        { "r4rk1/pR3ppp/2n5/2N1pb2/8/B1b3P1/P3PPBP/3R2K1 b - - 0 1",  11 },
        { "r4rk1/p2Qbppp/1p2p3/n3P3/2qN2n1/5NP1/PP3PKP/R1B1R3 b - - 0 1",  11 },
        { "3r4/3rqpkp/pn4p1/1ppBp2P/4P3/1PP3Q1/P2R1PP1/3R2K1 b - - 0 1",  13 },
        { "b4k2/br3p1p/2p3p1/p7/8/P1Q3PB/5P1P/6K1 b - - 1 1",  15 },
        { "rnbqkbr1/pp5p/2p1P1pn/4Q1B1/4p3/2N5/PPP2PPP/R3KB1R b KQq - 3 1",  13 },
        { "5R2/5P2/8/8/p1p2r2/k7/2K5/8 b - - 3 1",  15 },
        { "8/5ppp/2pnp3/3p4/1k6/1B1PP1P1/1P2KP1P/8 w - - 3 1",  13 },
        { "1r3rk1/pp2bqpp/1np1bp2/PP2p3/8/2NP1BP1/4PP1P/BRQR2K1 b - - 0 1",  13 },
        { "5rk1/5pp1/1bQ2n1p/4B3/1q2P3/5BP1/5P1P/R5K1 b - - 0 1",  13 },
        { "5rk1/7p/p2q4/1p4p1/2pB4/2P2bP1/PP5P/3BQ1K1 w - - 0 1",  15 },
        { "r3k2r/pp3pp1/1np1pnp1/1P6/5B2/6P1/1P2PPBP/R1R3K1 b kq - 0 1",  14 },
        { "r1r5/p2bppk1/2n3p1/1p6/3PP3/1B6/P3NPPK/3R3R w - - 0 1",  12 },
        { "6k1/5p2/1r2qbpp/1n1N4/2Q5/4P1P1/5P1P/3RN1K1 b - - 0 1",  13 },
        { "3rr1k1/1b1p1p1p/pqnR2p1/2p2p2/2P2B1N/P1Q1P3/1P3PPP/2K4R b - - 1 1",  11 },
        { "6n1/3k1r2/2n1p2p/3pP1p1/b2P2P1/2PpB2R/5PNK/1R6 b - - 0 1",  12 },
        { "1rqr4/6bk/5npp/2pb1p2/3N3P/4BBP1/3QPP2/2R2RK1 w - - 2 1",  12 },
        { "2q2rk1/4ppbp/6p1/3PP3/Q4B2/7P/5PP1/4R1K1 b - - 0 1",  11 },
        { "1r1r2k1/pbRnq1pp/1p2pp2/7Q/3P4/5NP1/PP2PPBP/5RK1 b - - 0 1",  12 },
        { "6k1/pp2qppp/1n1r4/Q4b2/1P2p3/P3P3/1Br1BPPP/R3K2R w KQ - 4 1",  11 },
        { "1rb1k2r/3nppb1/p1pp2p1/q3P3/3P1P2/1PN1B1Pp/2PQ1N1P/4RRK1 b k - 0 1",  11 },
        { "8/p5k1/1p2p1r1/4Pp1R/3P4/8/P4PP1/5K2 w - - 1 1",  12 },
        { "2r1rk2/1p1b1pR1/p6p/3p1q2/3B3P/P2P4/1P6/1K1R2Q1 b - - 2 1",  11 },
        { "rn2k3/pp2ppb1/2p3p1/q3P3/3PN3/2N1B2b/PPP2P2/R2QK3 b Qq - 0 1",  11 },
        { "r1b2rk1/ppb1qpp1/1np4p/3np3/3PP1P1/5NNP/PPQB1P2/2KR1B1R b - - 0 1",  13 },
        { "2r1kb1r/1p1n1pp1/pqn1p3/3pPbBp/1P1P3P/P1N5/3QNPP1/2R1KB1R b Kk - 0 1",  11 },
        { "r1b1k3/ppq2pp1/8/2p1n2r/2P1QBp1/P5P1/1P2PP1P/2KR3R w q - 2 1",  12 },
        { "r3r1k1/pp3p1p/2p1bbp1/7n/2PN4/4B3/PP1RBPPP/2KR4 b - - 5 1",  13 },
        { "2r3k1/1p3pbp/nB2p1p1/3p4/3P4/5PP1/5P1P/R4BK1 w - - 1 1",  12 },
        { "r5r1/p4pkp/6p1/1ppNR3/2p5/8/PP3PPP/2KR4 b - - 0 1",  13 },
        { "r1br2k1/ppq1bpp1/5n1p/4N3/8/3B4/P1P1QPPP/BR1R2K1 b - - 0 1",  11 },
        { "6rr/pp1bkp2/2p1pn2/6N1/2PPp1PP/4P3/1K2B1P1/5R1R b - - 0 1",  12 },
        { "2kr3r/pp2p3/1qp2npb/2N5/3Pp1bQ/2N5/PPP3B1/1K1R3R w - - 0 1",  11 },
        { "2bqk2r/4bppp/1Pp2n2/2Pp4/1p6/1N3PP1/2Q2P1P/B3KB1R b Kk - 0 1",  11 },
        { "r5k1/pp1b3r/3pp3/2n2p1q/3N4/5Q1P/PPP5/1K1R1B1R w - - 2 1",  12 },
        { "r4rk1/ppq3bp/1n1p1ppB/3p1b2/3P4/2P4P/PP2BPPN/R1Q2RK1 b - - 1 1",  13 },
		*/
		{ NULL, 0 }
	};
	struct bench_position *current_position;

	zct->input_buffer[0] = '\0';
	total_nodes = 0;
	time = get_time();
	old_post = zct->post;
	zct->use_book = FALSE;
	zct->post = TRUE;
	zct->engine_state = BENCHMARKING;
	print("Running benchmark");

	/* Loop through the positions. */
	for (current_position = &position[0]; current_position->fen != NULL;
		current_position++)
	{
		print(".");
		initialize_board(current_position->fen);
		generate_root_moves();
		zct->max_depth = current_position->depth - 1;
		search_root();
		total_nodes += zct->nodes + zct->q_nodes;
		if (zct->input_buffer[0])
			break;
	}
	time = get_time() - time;
	print("\n");
	print_statistics();
	print("Total nodes=%L\n", total_nodes);
	print("Total time=%T\n", time);
	print("Nodes per second=%L\n", total_nodes * 1000 / time);
	zct->engine_state = IDLE;
	zct->use_book = TRUE;
	zct->post = old_post;
}

/**
fatal_error():
When the program crashes, we call fatal_error() to print a message and die.
How sad...
Created 031709; last modified 031709
**/
void fatal_error(char *text, ...)
{
	char buffer[1<<16];
	va_list args;
	
	va_start(args, text);

	sprint_(buffer, sizeof(buffer), text, args);

	va_end(args);
	
	LOCK(smp_data->io_lock);
	fprintf(zct->output_stream, "%s", buffer);
	fprintf(zct->log_stream, "%s", buffer);
	UNLOCK(smp_data->io_lock);

	exit(EXIT_FAILURE);
}
