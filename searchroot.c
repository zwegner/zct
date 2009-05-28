/** ZCT/searchroot.c--Created 091505 **/

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
#include "debug.h"
#include "eval.h"
#include "smp.h"

int idle_time;

/**
search_root():
Do an alpha-beta search at the root. This routine also implements iterative
deepening.
Created 091505; last modified 031709
**/
void search_root(void)
{
	VALUE alpha;
	VALUE beta;
	VALUE value;
	BITBOARD last_nodes;
	ROOT_MOVE *move;
	int depth;

	clear_search();

	/* Probe the book. If we get a hit, we don't have to search. */
	if (zct->use_book && zct->engine_state != ANALYZING &&
		zct->engine_state != PONDERING && zct->engine_state != INFINITE &&
		zct->engine_state != DEBUGGING && book_probe())
		return;

	initialize_search();

	display_search_header();

	/* The main iterative deepening loop. */
	for (zct->current_iteration = 1; ; zct->current_iteration++)
	{
		alpha = -MATE;
		beta = MATE;
		zct->next_root_move = &zct->root_move_list[0];
		while ((move = select_root_move()) != NULL)
		{
			last_nodes = zct->nodes + zct->q_nodes;
			make_move(move->move);

			depth = (zct->current_iteration - 1) * PLY;
			/* Set up the iterative search stack and call the search. */
			if (alpha > -MATE)
			{
				/* Zero window on moves after the first. */
				search_call(&board.search_stack[0], FALSE,
					depth, 1, -alpha - 1, -alpha,
					&board.move_stack[0], NODE_PV, SEARCH_RETURN);
				value = -search(&board.search_stack[1]);
				/* research */
				if (value > alpha)
				{
					search_call(&board.search_stack[0], FALSE,
						depth, 1, -beta, -alpha,
						&board.move_stack[0], NODE_PV, SEARCH_RETURN);
					value = -search(&board.search_stack[1]);
				}
			}
			else
			{
				search_call(&board.search_stack[0], FALSE,
					depth, 1, -beta, -alpha,
					&board.move_stack[0], NODE_PV, SEARCH_RETURN);
				value = -search(&board.search_stack[1]);
			}	
#ifdef DEBUG_SEARCH
			board.search_stack[0].move = move->move;
			board.search_stack[0].move_made = TRUE;
			debug_step(&board.search_stack[0]);
			debug_continue();
			board.search_stack[0].move_made = FALSE;
#endif
			unmake_move();

			sum_counters();
			move->nodes = (zct->nodes + zct->q_nodes - last_nodes) -
				move->last_nodes;
			move->last_nodes = (zct->nodes + zct->q_nodes - last_nodes);
			move->last_score = value;
			move->pv[0] = move->move;
			copy_pv(move->pv + 1, board.pv_stack[1]);

			/* Update and display the new PV. */
			if (value > alpha)
			{
				alpha = value;
				move->score = ++zct->root_pv_counter;
				zct->best_score_by_depth[zct->current_iteration] = alpha;
				zct->pv_changes_by_depth[zct->current_iteration]++;
				board.pv_stack[0][0] = move->move;
				copy_pv(board.pv_stack[0] + 1, board.pv_stack[1]);
				copy_pv(move->pv, board.pv_stack[0]);
				display_search_line(FALSE, board.pv_stack[0], alpha);
			}

			if (stop_search() || zct->current_iteration == MAX_PLY)
				goto done;
		}
		/* Add up search counters and print the PV for this iteration. */
		sum_counters();
		display_search_line(TRUE, board.pv_stack[0], alpha);

		/* If we're in EPD testing mode, check the move found against the
			solution(s). */
		if (zct->engine_state == TESTING &&
			check_solutions(board.pv_stack[0][0], zct->current_iteration))
			goto done;
	}

done:
	/* All done here, finish up and print out some crap. */
	finish_search();
}

/**
clear_search():
Clears out search info. This is separate from initialize_search() because
otherwise the ponder move isn't cleared (crash!).
Created 050909; last modified 050909
**/
void clear_search(void)
{
	/* Reset ponder move. */
	if (zct->engine_state != PONDERING)
		zct->ponder_move = NO_MOVE;
}

/**
initialize_search():
Initializes all of the data needed to run a search.
Created 031709; last modified 031709
**/
void initialize_search(void)
{
	/* Set up counters. */
	initialize_counters();

	if (zct->engine_state != BENCHMARKING)
		initialize_statistics();

	/* Set up root PV counter (used for move ordering). */
	zct->root_pv_counter = 0;

	root_entry = board.game_entry;
	generate_root_moves();
	start_time();
	set_time_limit();

	/* Start up child processors, as they are in a parked state right now. */
#ifdef SMP
	start_child_processors();
	set_active();
#endif
}

/**
finish_search():
Finalizes all information for the search and cleans up some stats.
Created 031709; last modified 031709
**/
void finish_search(void)
{
#ifdef SMP
	stop_child_processors();
	set_idle();
#endif

	/* We're in UCI infinite mode, so just wait until the UI asks us to stop. */
	if (zct->engine_state == INFINITE)
		while (!stop_search())
			;

	/* Determine the best score, for the next search. */
	if (zct->best_score_by_depth[zct->current_iteration] == -MATE)
		zct->last_root_score =
			zct->best_score_by_depth[zct->current_iteration - 1];
	else
		zct->last_root_score =
			zct->best_score_by_depth[zct->current_iteration];

	/* Print out the final search statistics. */
	print_search_info();

	/* If this was a normal search, i.e. not a ponder search, set the
		ponder move. This is so that during the opponent's thinking time,
		we can spend our time productively. */
	if (zct->engine_state == NORMAL)
		zct->ponder_move = MOVE_COMPARE(board.pv_stack[0][1]);

	/* Increment the search age for the hash table replacement scheme. */
	zct->search_age++;
}

/**
print_search_info():
After the search, print out some statistics. If we're in ICS mode, kibitz some
junk as well so everyone knows we're not cheating. (wink wink)
Created 031709; last modified 031709
**/
void print_search_info(void)
{
	int time;
#ifdef SMP
	int x;
#endif

	sum_counters();
	time = time_used();
	/* Avoid divide by zero... */
	time = MAX(1, time);

	/* Standard search output */
	if (zct->post && zct->protocol != XBOARD && zct->protocol != ANALYSIS_X &&
		zct->protocol != UCI)
	{
		print("search: nodes=%L q=%.1f%% time=%T nps=%i\n",
			zct->nodes + zct->q_nodes,
			(float)100.0 * zct->q_nodes / (zct->nodes + zct->q_nodes), time,
			(int)(1000 * (zct->nodes + zct->q_nodes) / time));
		print("        fail highs=%-10L pv nodes=%-10L fail lows=%-10L\n",
			zct->fail_high_nodes, zct->pv_nodes, zct->fail_low_nodes);
		print("        fh first=%3.1f%%        pv first=%.1f%%\n",
			(float)100.0 * zct->fail_high_first / zct->fail_high_nodes,
			(float)100.0 * zct->pv_first / zct->pv_nodes);
		print("ext:    check=%i one-rep=%i threat=%i passed-pawn=%i\n",
			zct->check_extensions_done, zct->one_rep_extensions_done,
			zct->threat_extensions_done, zct->passed_pawn_extensions_done);
		print("hash:   hits=%3.1f%% cutoffs=%3.1f%% full=%3.1f%% pawn=%3.1f%% "
			"eval=%3.1f%% qsearch=%3.1f%%\n",
			(float)100.0 * zct->hash_hits / zct->hash_probes,
			(float)100.0 * zct->hash_cutoffs / zct->hash_probes,
			(float)100.0 * zct->hash_entries_full /
				(zct->hash_size * HASH_SLOT_COUNT),
			(float)100.0 * zct->pawn_hash_hits / zct->pawn_hash_probes,
			(float)100.0 * zct->eval_hash_hits / zct->eval_hash_probes,
			(float)100.0 * zct->qsearch_hash_hits / zct->qsearch_hash_probes);
#ifdef SMP
		print("smp:    splits=%i stops=%i/%.1f%%\n",
			smp_data->splits_done, smp_data->stops_done,
			(float)100.0 * smp_data->stops_done / smp_data->splits_done);
		print("        nodes=(");
		for (x = 0; x < zct->process_count; x++)
		{
			print("%L", smp_block[x].nodes + smp_block[x].q_nodes);
			if (x < zct->process_count - 1)
				print(", ");
		}
		print(")\n");
		print("        idle=%T/%.1f%%\n", idle_time, (float)100.0 * idle_time /
			((float)zct->process_count * time_used()));

		if (zct->engine_state != BENCHMARKING)
			print_statistics();
#endif
	}
	/* ICS output */
	else if (zct->ics_mode && zct->engine_state != PONDERING)
	{
		/* Computer opponent: kibitz. Human: whisper. */
//		if (zct->computer)
			print("tellall ");
//		else
//			print("tellothers ");

		print("depth=%i/%i nodes=%L(q=%.1f%%) time=%T nps=%i score=%V pv=%lM",
			zct->current_iteration, zct->max_depth_reached,
			zct->nodes + zct->q_nodes,
			(float)100.0 * zct->q_nodes / (zct->nodes + zct->q_nodes), time,
			(int)(1000 * (zct->nodes + zct->q_nodes) / time),
			zct->last_root_score, board.pv_stack[0]);
#ifdef SMP
		print(" splits=%i stops=%i/%.1f%%", smp_data->splits_done,
			smp_data->stops_done, (float)100.0 * smp_data->stops_done /
			smp_data->splits_done);
		print(" idle=%.1f%%\n", (float)100.0 * idle_time /
			((float)zct->process_count * time_used()));
#endif
		print("\n");
	}
}

/**
sum_counters():
If we are using multiple processors, then add the counters from each processor
into the global stats.
Created 083006; last modified 031709
**/
void sum_counters(void)
{
#ifdef SMP
	int x;

	zct->nodes = zct->q_nodes = 0;
	idle_time = 0;
	for (x = 0; x < zct->process_count; x++)
	{
		zct->nodes += smp_block[x].nodes;
		zct->q_nodes += smp_block[x].q_nodes;
		idle_time += smp_block[x].idle_time;
		if (smp_block[x].last_idle_time)
			idle_time += get_time() - smp_block[x].last_idle_time;
	}
#endif
}

/**
initialize_counters():
Initialize all of the simple counters used during the search.
Created 092906; last modified 031709
**/
void initialize_counters(void)
{
	int x;

#ifdef SMP
	smp_data->splits_done = 0;
	smp_data->stops_done = 0;
	for (x = 0; x < zct->process_count; x++)
	{
		smp_block[x].nodes = 0;
		smp_block[x].q_nodes = 0;
		smp_block[x].idle_time = 0;
		if (x != 0)
			smp_block[x].last_idle_time = get_time();
		else
			smp_block[x].last_idle_time = 0;
	}
#endif
	zct->nodes = 0;
	zct->q_nodes = 0;
	zct->max_depth_reached = 0;
	zct->nodes_until_check = zct->nodes_per_check;
	zct->pv_nodes = 0;
	zct->fail_high_nodes = 0;
	zct->fail_low_nodes = 0;
	zct->pv_first = 0;
	zct->fail_high_first = 0;
	zct->hash_probes = 0;
	zct->hash_hits = 0;
	zct->hash_cutoffs = 0;
	zct->pawn_hash_probes = 0;
	zct->pawn_hash_hits = 0;
	zct->eval_hash_probes = 0;
	zct->eval_hash_hits = 0;
	zct->qsearch_hash_probes = 0;
	zct->qsearch_hash_hits = 0;
	zct->check_extensions_done = 0;
	zct->one_rep_extensions_done = 0;
	zct->threat_extensions_done = 0;
	zct->singular_extensions_done = 0;
	zct->passed_pawn_extensions_done = 0;
	
	for (x = 0; x < MAX_PLY; x++)
	{
		zct->best_score_by_depth[x] = -MATE;
		zct->pv_changes_by_depth[x] = 0;
	}
}
