/** ZCT/search2.c--Created 102906 **/

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

/**
heuristic():
Updates history, killer, and counter tables based on a move that failed high.
Created 092906; last modified 081007
**/
void heuristic(MOVE best_move, SEARCH_BLOCK *sb)
{
	int x;

	/* Update the history table. */
	zct->history_table[board.side_tm][MOVE_KEY(best_move)] +=
		sb->depth * sb->depth;
	zct->history_counter = MAX(zct->history_table[board.side_tm]
		[MOVE_KEY(best_move)], zct->history_counter);
	/* Check for an "overflow" of the history table, in which case we scale
		back all of the values. */
	if (zct->history_counter > 1 << 20)
	{
		for (x = 0; x < 2 * 4096; x++) /* This is not recommended ;) */
			zct->history_table[0][x] >>= 4;
		zct->history_counter >>= 4;
	}
	/* Update killer moves. */
	if (board.color[MOVE_TO(best_move)] == EMPTY)
	{
		zct->killer_move[sb->ply][1] = MOVE_COMPARE(best_move);
		zct->counter_move[board.side_tm][MOVE_KEY((sb - 1)->move)][1] =
			MOVE_COMPARE(best_move);
	}
}

/**
heuristic_pv():
Updates killer tables when the pv changes at a pv node.
Created 110606; last modified 081007
**/
void heuristic_pv(MOVE best_move, SEARCH_BLOCK *sb)
{
	if (board.color[MOVE_TO(best_move)] == EMPTY)
	{
		zct->killer_move[sb->ply][0] = MOVE_COMPARE(best_move);
		zct->counter_move[board.side_tm][MOVE_KEY((sb - 1)->move)][0] =
			MOVE_COMPARE(best_move);
	}
}

/**
moves_are_connected():
This function is used for LMR, testing if a reduced move actually contained a
threat. The function tests if the move that refuted null move was connected to
the first move. This idea is from Tord Romstad.
Created 101707; last modified 101807
**/
BOOL moves_are_connected(MOVE move_1, MOVE move_2)
{
	if (move_1 == NULL_MOVE || move_2 == NULL_MOVE)
		return FALSE;
	if (MOVE_TO(move_1) == MOVE_FROM(move_2))
		return TRUE;
	if (MOVE_FROM(move_1) == MOVE_TO(move_2))
		return TRUE;
	if (MASK(MOVE_FROM(move_1)) & in_between_bb[MOVE_FROM(move_2)]
		[MOVE_TO(move_2)])
		return TRUE;
	return FALSE;
}

/**
move_is_futile():
This function tests whether a move should be pruned using Ernst Heinz's Futility
Pruning. The idea is that, at horizon nodes, if a move cannot bring the material
score (plus a margin) to alpha, there is no point in even making it, because
the next ply in quiescence will have a stand-pat fail high.
Created 101707; last modified 101707
**/
BOOL move_is_futile(SEARCH_BLOCK *sb, MOVE move)
{
	return FALSE;
	return (sb->moves > 0 && sb->extension == 0 && sb->depth <= PLY &&
		material_balance() + piece_value[board.piece[MOVE_TO(sb->move)]] +
		100 < sb->alpha);
}

/**
null_reduction():
Determine the amount of reduction to apply to null move searches.
Created 103006; last modified 072108
**/
int null_reduction(SEARCH_BLOCK *sb)
{
	return sb->depth > 7 * PLY ? 4 * PLY : 3 * PLY;
}

/**
should_nullmove():
Decide whether or not null move pruning should be used on this node.
Created 052408; last modified 060108
**/
BOOL should_nullmove(SEARCH_BLOCK *sb)
{
	return (!sb->check && sb->beta < MATE &&
		board.piece_count[board.side_tm] >= 2);
}

/**
extend_move():
Calculates the search extension to be awarded to the given move. This is based
on various information such as whether the move checks, if it is a threat or
not, etc. This also determines whether a move should be reduced or not.
Created 072007; last modified 081808
**/
int extend_move(SEARCH_BLOCK *sb)
{
	int extension;
	int lmr_moves;

	/* First, determine if the move should be extended (rather than reduced). */
	extension = 0;
	if (!(sb->move & MOVE_NOEXTEND))
	{
		/* check extension */
		if (in_check())
		{
			extension += zct->check_extension;
			zct->check_extensions_done++;
		}
		/* one-reply extension */
		if (sb->check && sb->last_move - sb->first_move == 1)
		{
			extension += zct->one_rep_extension;
			zct->one_rep_extensions_done++;
		}
		/* pawn to 7th extension */
		if (board.piece[MOVE_TO(sb->move)] == PAWN &&
			RANK_OF(SQ_FLIP_COLOR(MOVE_TO(sb->move), board.side_ntm)) == RANK_7)
		{
			extension += zct->passed_pawn_extension;
			zct->passed_pawn_extensions_done++;
		}
		extension = MIN(PLY, extension);
	}

	/* Now try to reduce the move. If the move is marked as not reducible, or
		if the depth is too low, don't bother reducing it. */
	if (extension > 0 || sb->depth < 2 * PLY || (sb->move & MOVE_NOREDUCE))
		return extension;

	lmr_moves = 0;
	switch (sb->node_type)
	{
		case NODE_PV:
			lmr_moves = 5;
			break;
		case NODE_CUT:
			lmr_moves = 2;
			break;
		case NODE_ALL:
			lmr_moves = 3;
			break;
	}

	/* Most LMR conditions are already covered in the move scoring routines.
		The only "dynamic" factor is the count of moves. */
	if (sb->moves > lmr_moves)
	{
		sb->move |= MOVE_ISREDUCED;
		extension -= PLY;
	}

	return extension;
}

/**
extend_node():
Calculate a new depth for this node, based on extensions, reductions, and any
possible pruning. This is done after eval, null move, and IID, so we have
some information to go on.
Created 081808; last modified 081808
**/
void extend_node(SEARCH_BLOCK *sb)
{
#if 0
	COLOR color;
	EVAL_BLOCK *old_eval;
	EVAL_BLOCK *new_eval;
	VALUE ks_delta[2];
	VALUE pp_delta[2];
	VALUE eval_delta[2];
	BOOL ks_def_good;
	BOOL ks_att_good;
	BOOL ks_good;
	BOOL pp_def_good;
	BOOL pp_att_good;
	BOOL pp_good;
	BOOL eval_own_good;
	BOOL eval_opp_good;
	BOOL eval_good;

	old_eval = &(sb - 1)->eval_block;
	new_eval = &sb->eval_block;

	for (color = WHITE; color <= BLACK; color++)
	{
		ks_delta[color] = new_eval->king_safety[color] -
			old_eval->king_safety[color];
		pp_delta[color] = new_eval->passed_pawn[color] -
			old_eval->passed_pawn[color];
		eval_delta[color] = -new_eval->eval[color] - old_eval->eval[color];
	}

	/* Take the values of the side not-to-move, as they just finished making
		a move. We look at the difference in eval caused by that move. */
	color = board.side_ntm;

	/* King Safety */
	ks_def_good = (ks_delta[color] > 10);
	ks_att_good = (ks_delta[COLOR_FLIP(color)] < 10);
	ks_good = (ks_delta[color] - ks_delta[COLOR_FLIP(color)] > -10);

	pp_def_good = (pp_delta[color] > 10);
	pp_att_good = (pp_delta[COLOR_FLIP(color)] < 10);
	pp_good = (pp_delta[color] - pp_delta[COLOR_FLIP(color)] > -10);

	eval_own_good = (eval_delta[color] > 0);
	eval_opp_good = (eval_delta[COLOR_FLIP(color)] < 0);
	eval_good = (eval_delta[color] - eval_delta[COLOR_FLIP(color)] > 0);

	/* If the last move wasn't reduced normally, try to reduce it now
		based on evaluation factors. */
	if (!((sb - 1)->move & MOVE_ISREDUCED) && //!(sb->move & MOVE_NOREDUCE) &&
	/*	!ks_att_good && */!ks_good &&
	/*	!pp_att_good &&*/ !pp_good &&
		!eval_good)
	{
		sb->depth -= PLY;
//		sb->move |= MOVE_ISREDUCED;
	}

	/* Extend the node by dynamic null-move-based threat detection. */
	if (sb->threat)
	{
		sb->depth += zct->threat_extension;
		zct->threat_extensions_done++;
	}
	/* XXX what to do now? Ideas:
		--"refuting" lmr based on eval
		--make null move AZW for cutoff vs. really bad
		--threat move, null move score, iid score? -> double check pruning
		--extension for moves that increase one term a lot
		--extension for uncertain moves, that increase one and
			decrease another
		--reductions based on this stuff
		--intelligent move sorting based on evaluation bitboard:
			mark squares in front of passed pawns, near king, maybe
			good squares. Bad square bitboard too?
		--make new file for selectivity stuff like this, extend_move, etc.
		--change this function to allow pruning and dropping into qsearch
	*/
#endif
}

/**
search_call():
Sets up the internal stack for a search call.
Created 081205; last modified 081908
**/
void search_call(SEARCH_BLOCK *sb, BOOL is_qsearch, int depth, int ply,
	VALUE alpha, VALUE beta, MOVE *first_move, NODE_TYPE node_type,
	SEARCH_STATE search_state)
{
	/* Depth is negative--drop into qsearch. */
	if (depth <= 0 && !is_qsearch)
	{
		depth = 2;
		is_qsearch = TRUE;
	}
	sb->search_state = search_state;

	/* Move to the next node up the tree and set the parameters for
		a new search. */
	sb++;
	sb->id = sb_id++;

	if (is_qsearch)
		sb->search_state = QSEARCH_START;
	else
		sb->search_state = SEARCH_START;

	sb->depth = depth;
	sb->ply = ply;
	sb->alpha = alpha;
	sb->beta = beta;
	sb->node_type = node_type;

	sb->select_state = SELECT_HASH_MOVE;
	sb->moves = 0;
	sb->pv_found = 0;
	board.pv_stack[sb->ply][0] = NO_MOVE;
	
	sb->first_move = first_move;
	sb->next_move = first_move;
	sb->last_move = first_move;

	sb->hash_move = NO_MOVE;
	sb->move = NO_MOVE;
	sb->best_score = NO_VALUE;
	sb->threat = FALSE;

	sb->is_qsearch = is_qsearch;
	sb->move_made = FALSE;
}

/**
search_return():
In the iterative search framework, this function handles some bookkeeping
involved in backing up a value.
Created 110108; last modified 110108
**/
void search_return(SEARCH_BLOCK *sb, VALUE return_value)
{
	sb->return_value = return_value;
#ifdef SMP
	if (sb > board.search_stack)
		update_best_sb(sb - 1, FALSE);
#endif
}

/**
copy_pv():
Copies the pv from 'from' to 'to'. :)
Created 082906; last modified 091906
**/
void copy_pv(MOVE *to, MOVE *from)
{
	while (*from)
		*to++ = *from++;
	*to = NO_MOVE;
}

/**
search_maintenance():
In every node during a search, do some checks for SMP events, timeout, etc.
If the search needs to exit, we store the return value in return_value and
return TRUE, otherwise we return FALSE.
Created 031609; last modified 031609
**/
BOOL search_maintenance(SEARCH_BLOCK **sb, VALUE *return_value)
{
#ifdef SMP
	int r;

	/* Handle any asynchronous messages we have. */
	if (smp_block[board.id].message_count)
		handle_smp_messages(sb);

	/* Check for synchronous messages, and exit if necessary. */
	if (smp_block[board.id].input)
	{
		if (handle_smp_input(*sb))
		{
			*return_value = 0;
			return TRUE;
		}
	}

	/* If another processor has backed up to the root, they set a flag
	   because only the main processor can return to search_root(). */
	if (board.id == 0 && smp_data->return_flag)
	{
		ASSERT(board.split_point == board.split_point_stack);
		ASSERT(smp_data->split_count == 0);

		/* Back up the game board. */
		while (board.game_entry > root_entry + 1)
			unmake_move();

		/* This is kind of hacky... */
		if (board.game_entry == root_entry)
			make_move((zct->next_root_move - 1)->move);

		SMP_DEBUG(print("cpu 0 returning...\n%B",&board));
		smp_data->return_flag = FALSE;
		set_active();

		/* Return the score and PV. */
		copy_pv(board.pv_stack[1], smp_data->return_pv);
		*return_value = smp_data->return_value;
		return TRUE;
	}
#endif

	/* Do the standard periodic check for time or input. */
	if (search_check())
	{
#ifdef SMP
		/* Flag the other processors down. */
		for (r = 1; r < zct->process_count; r++)
			smp_tell(r, SMP_PARK, 0);
		/* Unsplit all of our split points, after waiting for the
		   child processors to detach. */
		while (*board.split_point != NULL)
		{
			while ((*board.split_point)->child_count > 1)
				;
			unsplit(*sb, (*board.split_point)->id);
		}
#endif
		/* We're good to go. Stop the search. */
		while (board.game_entry > root_entry + 1)
			unmake_move();

		/* This is kind of hacky... */
		if (board.game_entry == root_entry)
			make_move((zct->next_root_move - 1)->move);

		/* Return a really good score, so the root move appears to
		   be a mated-in-0. This just invalidates the score. */
		*return_value = MATE;
		return TRUE;
	}

	return FALSE;
}

/**
search_check():
Does a periodic check if the search has ended. Note that this "node counter"
no longer represents nodes at all, just simply the iterations of our iterative
search function.
Created 122308; last modified 122308
**/
BOOL search_check(void)
{
#ifdef SMP
	if (board.id != 0)
		return FALSE;
#endif

#ifdef CLUSTER
//	if (cluster->id != 0)
//		return FALSE;
#endif

	zct->nodes_until_check--;
	if (zct->nodes_until_check > 0)
		return FALSE;

	zct->nodes_until_check = zct->nodes_per_check;
	return stop_search();
}

/**
stop_search():
This checks for time and input to see if need to stop searching.
Created 091006; last modified 030709
**/
BOOL stop_search(void)
{
	CMD_RESULT cmd_result;
	BOOL stop = FALSE;
	GAME_ENTRY *current;

	if (zct->stop_search)
		stop = TRUE;
	/* Check for input, because we might enter this function more than once
		before fully stopping when unwinding the stack. */
	else if (zct->input_buffer[0])
		stop = TRUE;
	else if (time_is_up())
		stop = TRUE;
	else if (input_available())
	{
		/* Return to the root position. Some commands, such as "display" work
			on the root position instead of the current one. */
		current = board.game_entry;
		while (board.game_entry > root_entry)
			unmake_move();
		/* When pondering, the root entry is after the ponder move. */
		if (zct->engine_state == PONDERING)
			unmake_move();
		/* Read the command and parse it. */
		read_line();
		cmd_result = command(zct->input_buffer);
		/* Command() returns CMD_STOP_SEARCH when we need to exit the search to
			handle a command. */
		if (cmd_result == CMD_STOP_SEARCH)
			stop = TRUE;
		/* If the command was a move, either stop or make the move. */
		else if (zct->engine_state != NORMAL && cmd_result == CMD_BAD &&
			input_move(zct->input_buffer, INPUT_CHECK_MOVE))
		{
			/* If we are pondering, check if the move entered was the ponder
				move. If so, we don't need to stop. */
			if (zct->engine_state == PONDERING && input_move(zct->input_buffer,
				INPUT_GET_MOVE) == zct->ponder_move)
			{
				zct->input_buffer[0] = '\0';
				make_move(zct->ponder_move);
				zct->engine_state = NORMAL;
				update_clocks();
			}
			else
				stop = TRUE;
		}
		else if (cmd_result == CMD_BAD)
		{
			zct->input_buffer[0] = '\0';
			if (zct->protocol != UCI)
				print("Error.\n");
		}

		/* Check for ping here in UCI mode. */
		if (zct->ping && zct->protocol == UCI)
		{
			print("readyok\n");
			zct->ping = 0;
		}
		/* Go back to the position we were in before checking. This is kind of
			hacky in that it relies on the game history after undoing all the
			moves to be the same. */
		while (board.game_entry < current)
			make_move(board.game_entry->move);
	}

	/* If we're stopping the search, set a flag to make sure we don't come
		back here and decide to keep going, thereby introducing bugs... */
	if (stop)
		zct->stop_search = TRUE;

	return stop;
}
