/** ZCT/search.c--Created 070905 **/

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
#include "smp.h"

#define SEARCH_CALL(d, p, a, b, fm, nt, ss) \
	do { \
		search_call((sb), FALSE, (d), (p), (a), (b), (fm), (nt), (ss)); \
		sb++; \
		goto end; \
	} while (FALSE); case ss:

#define QSEARCH_CALL(d, p, a, b, fm, nt, ss) \
	do { \
		search_call((sb), TRUE, (d), (p), (a), (b), (fm), (nt), (ss)); \
		sb++; \
		goto end; \
	} while (FALSE); case ss:

#define RETURN(r) \
	do { \
		search_return(sb, r); \
		sb--; \
		goto end; \
	} while (FALSE)

#define RETURN_VALUE (sb + 1)->return_value

#define SEARCH_CHECK() //\
	do { \
		if (search_maintenance(&sb, &r)) \
			return r; \
		if (sb->search_state == SEARCH_WAIT) \
			goto end; \
	} while (FALSE)
			
/**
search():
Do an iterative alpha-beta search.
Created 070905; last modified 110708
**/
VALUE search(SEARCH_BLOCK *sb)
{
	VALUE r;

	while (TRUE)
	{
#ifdef DEBUG_SEARCH
		verify();
		debug_step(sb);
#endif
#ifdef SMP
		if (*board.split_ply != -1)
			ASSERT(sb->ply >= *board.split_ply);
#endif
		if (search_maintenance(&sb, &r))
			return r;
		switch (sb->search_state)
		{
/**
The main search.
**/
case SEARCH_START:
#ifdef SMP
		smp_block[board.id].nodes++;
#else
		zct->nodes++;
#endif
		/* Check for timeout or SMP events. */
		SEARCH_CHECK();

		/* Do checks for search-ending conditions. */
		/* max ply */
		if (sb->ply >= zct->max_depth_reached)
			zct->max_depth_reached = sb->ply;
		if (sb->ply >= MAX_PLY - 1)
			RETURN(evaluate(&sb->eval_block));
		if (board.fifty_count >= 100 && !is_mate())
			RETURN(DRAW);
		if (is_repetition(sb->ply > 1 ? 2 : 3))
			RETURN(DRAW);

		/* mate-distance pruning */
		sb->alpha = MAX(sb->alpha, -MATE + sb->ply);
		sb->beta = MIN(sb->beta, MATE - sb->ply - 1);
		if (sb->alpha >= sb->beta)
			RETURN(sb->alpha);

		/* material draw pruning */
		if (!can_mate(board.side_tm))
		{
			sb->beta = MIN(sb->beta, DRAW);
			if (sb->alpha >= sb->beta)
				RETURN(sb->alpha);
		}
		if (!can_mate(board.side_ntm))
		{
			sb->alpha = MAX(sb->alpha, DRAW);
			if (sb->alpha >= sb->beta)
				RETURN(sb->alpha);
		}

		/* Probe the hash table. */
		if (hash_probe(sb, FALSE))
			RETURN(sb->alpha);

		sb->check = check_squares();
		/* Nullmove search. */
		if (should_nullmove(sb))
		{
			sb->move_made = TRUE;
			sb->move = NULL_MOVE;

			make_move(NULL_MOVE);
			
			SEARCH_CALL(sb->depth - null_reduction(sb) - PLY, sb->ply + 1,
				-sb->beta, -sb->beta + 1, sb->last_move, NODE_ALL,
				SEARCH_NULL_1)
			r = -RETURN_VALUE;
			
			unmake_move();
			sb->move_made = FALSE;

			if (r >= sb->beta)
			{
				hash_store(sb, NO_MOVE, r, HASH_LOWER_BOUND, FALSE);
				RETURN(r);
			}
			/* If the null move failed low, we can use the refutation for move
				ordering on the next ply. */
			else
			{
				board.threat_move[sb->ply + 1] = MOVE_COMPARE(board.pv_stack
					[sb->ply + 1][0]);
				if (moves_are_connected((sb - 1)->move,
					board.threat_move[sb->ply + 1]))
				{
					/* If we reduced on the last ply, check if the move was
						actually a threat that allowed the refutation. */
					if ((sb - 1)->extension < 0)
						RETURN(sb->alpha);
				}
			}
			/* Mate threat. The side that null moved was mated immediately. */
			if (r == -MATE + sb->ply + 2)
				sb->threat = TRUE;
		}

		/* Internal iterative deepening */
		if (sb->hash_move == NO_MOVE && sb->depth >= 3 * PLY &&
			(sb->node_type == NODE_PV || sb->node_type == NODE_CUT))
		{
			SEARCH_CALL(sb->depth - 2 * PLY, sb->ply, sb->alpha, sb->beta,
				sb->last_move, sb->node_type, SEARCH_IID)
			sb->iid_value = RETURN_VALUE;
			if (sb->iid_value > sb->alpha)
				sb->hash_move = board.pv_stack[sb->ply][0];
		}

		/* Run the evaluation in order to get some search information. */
		evaluate(&sb->eval_block);

		/* Extend, reduce, prune, etc. for this node. */
		extend_node(sb);

		/* The main move loop. */
		while ((sb->move = select_move(sb)) != NO_MOVE)
		{
			/* Futility pruning. Check that we have at least one move searched
				so that we don't return a false mate. */
			if (sb->moves > 0 && move_is_futile(sb, sb->move))
				continue;
			if (!make_move(sb->move))
				continue;
			sb->moves++;
			sb->move_made = TRUE;

			/* extensions/reductions */
			sb->extension = extend_move(sb);

			/* Moves after the first in PV nodes are searched with null windows.
				This is known as Principal Variation Searching. */
			if (sb->moves > 1 && sb->node_type == NODE_PV)
			{
				SEARCH_CALL(sb->depth + sb->extension - PLY, sb->ply + 1,
					-sb->alpha - 1, -sb->alpha, sb->last_move, NODE_CUT,
					SEARCH_1)
				r = -RETURN_VALUE;
				if (r > sb->alpha)
				{
					/* Reset the extension if this move has been reduced. */
					sb->extension = MAX(sb->extension, 0);
					SEARCH_CALL(sb->depth + sb->extension - PLY, sb->ply + 1,
						-sb->beta, -sb->alpha, sb->last_move, NODE_PV, SEARCH_2)
					r = -RETURN_VALUE;
				}
			}
			else
			{
				SEARCH_CALL(sb->depth + sb->extension - PLY, sb->ply + 1,
					-sb->beta, -sb->alpha, sb->last_move,
					NODE_FLIP(sb->node_type), SEARCH_3)
				r = -RETURN_VALUE;
				/* reduced move research */
				if (r > sb->alpha && sb->extension < 0)//(sb->move & MOVE_ISREDUCED))
				{
					/* Since the move could be extended and reduced at the
						same time, we increase the depth rather than zero it. */
					sb->extension += PLY;
					SEARCH_CALL(sb->depth + sb->extension - PLY, sb->ply + 1,
						-sb->beta, -sb->alpha, sb->last_move,
						NODE_FLIP(sb->node_type), SEARCH_4)
					r = -RETURN_VALUE;
				}
			}
			unmake_move();
			sb->move_made = FALSE;

			/* Check the score against the alpha + beta bounds. */
			if (r > sb->best_score)
				sb->best_score = r;
			if (r > sb->alpha)
			{
				sb->alpha = r;
				sb->pv_found = sb->moves;
				board.pv_stack[sb->ply][0] = sb->move;
				copy_pv(board.pv_stack[sb->ply] + 1,
					board.pv_stack[sb->ply + 1]);
				heuristic(sb->move, sb);
#ifdef SMP
				/* Update the bounds in the split point for our siblings. */
				if (*board.split_ply == sb->ply)
				{
					merge(sb);
					/* When we get a cutoff in a split point, then there is no
						work left, so we must wait for another split point. */
					if (sb->search_state == SEARCH_WAIT)
						goto end;
				}
#endif
				/* Fail high. */
				if (r >= sb->beta)
				{
					zct->fail_high_nodes++;
					if (sb->moves == 1)
						zct->fail_high_first++;
					hash_store(sb, sb->move, r, HASH_LOWER_BOUND, FALSE);
					RETURN(r);
				}
			}

			/* Apply correction for badly predicted nodes: if we've searched
				at least three moves, we're pretty damn sure it's not a CUT. */
			if (sb->node_type == NODE_CUT && sb->moves >= 3)
				sb->node_type = NODE_ALL;

#ifdef SMP

#ifdef CLUSTER
			/* Check if we should split our cluster node. */
#endif
			/* Child processors jump in here so that we go back to the
				beginning of the loop and grab another move in parallel. */
case SEARCH_JOIN:
			;
#endif
		}
#ifdef SMP
		/* If this is a split ply, we have to detach from the split point. */
		if (*board.split_ply == sb->ply)
		{
			detach(sb);
			SMP_DEBUG(print("cpu %i detached %E\n", board.id, sb));
			if (sb->search_state == SEARCH_WAIT)
				goto end;
		}
#endif
		/* No moves, check or stale mate. */
		if (sb->moves == 0)
		{
			if (sb->check)
				r = -MATE + sb->ply;
			else
				r = DRAW;
			/* Is this worth it? */
			hash_store(sb, NO_MOVE, r, HASH_EXACT_BOUND, FALSE);
			RETURN(r);
		}
		/* Store in the hash table, plus update some statistics/heuristics. */
		if (sb->pv_found)
		{
			zct->pv_nodes++;
			if (sb->pv_found == 1)
				zct->pv_first++;
			heuristic_pv(board.pv_stack[sb->ply][0], sb);
			hash_store(sb, board.pv_stack[sb->ply][0], sb->alpha,
				HASH_EXACT_BOUND, FALSE);
		}
		else
		{
			zct->fail_low_nodes++;
			hash_store(sb, NO_MOVE, sb->best_score, HASH_UPPER_BOUND, FALSE);
		}
		RETURN(sb->best_score);
/**
The quiescence search.
**/
case QSEARCH_START:
#ifdef SMP
		smp_block[board.id].q_nodes++;
#else
		zct->q_nodes++;
#endif
		/* Check for timeout or SMP events. */
		SEARCH_CHECK();

		if (sb->ply >= zct->max_depth_reached)
			zct->max_depth_reached = sb->ply;
		if (sb->ply >= MAX_PLY - 1)
			RETURN(evaluate(&sb->eval_block));
		if (board.fifty_count >= 100 && !is_mate())
			RETURN(DRAW);
		if (is_repetition(sb->ply > 1 ? 2 : 3))
			RETURN(DRAW);

		/* mate-distance pruning */
		sb->alpha = MAX(sb->alpha, -MATE + sb->ply);
		sb->beta = MIN(sb->beta, MATE - sb->ply - 1);
		if (sb->alpha >= sb->beta)
			RETURN(sb->alpha);

		/* material draw pruning */
		if (!can_mate(board.side_tm))
		{
			sb->beta = MIN(sb->beta, DRAW);
			if (sb->alpha >= sb->beta)
				RETURN(sb->alpha);
		}
		if (!can_mate(board.side_ntm))
		{
			sb->alpha = MAX(sb->alpha, DRAW);
			if (sb->alpha >= sb->beta)
				RETURN(sb->alpha);
		}

		/* Probe the hash table. */
		if (hash_probe(sb, TRUE))
			RETURN(sb->alpha);

		sb->check = check_squares();
		/* We can only stand pat if we're not in check. If we're in check,
			we must search all moves (now handled in the move sorting function),
			and we can't take the stand pat score, which is basically a null
			move score. */
		if (!sb->check)
		{
			/* Evaluate for the stand pat. */
			evaluate(&sb->eval_block);
			r = sb->best_score = sb->eval_block.full_eval;
			if (r > sb->alpha)
			{
				sb->alpha = r;
				if (r >= sb->beta)
				{
					/* Is this worth it? Probably not. */
					hash_store(sb, NO_MOVE, r, HASH_LOWER_BOUND, TRUE);
					RETURN(r);
				}
			}
		}

moves:	/* Main quiescence move loop */
		while ((sb->move = select_qsearch_move(sb)) != NO_MOVE)
		{
			/* Make the move while checking legality. */
			if (!make_move(sb->move))
				continue;
			sb->moves++;
			sb->move_made = TRUE;

			/* Search the next ply. */
			QSEARCH_CALL(sb->depth - 1, sb->ply + 1, -sb->beta, -sb->alpha,
				sb->last_move, NODE_FLIP(sb->node_type), QSEARCH_1)
			r = -RETURN_VALUE;

			unmake_move();
			sb->move_made = FALSE;

			/* Check the score against the alpha + beta bounds. */
			if (r > sb->best_score)
				sb->best_score = r;

			if (r > sb->alpha)
			{
				sb->alpha = r;
				sb->pv_found = sb->moves;
				board.pv_stack[sb->ply][0] = sb->move;
				copy_pv(board.pv_stack[sb->ply] + 1,
					board.pv_stack[sb->ply + 1]);

				if (r >= sb->beta)
				{
					heuristic(sb->move, sb);
					hash_store(sb, sb->move, r, HASH_LOWER_BOUND, TRUE);
					RETURN(r);
				}
			}
		}
		/* If all captures were unsuccessful at improving alpha, and the depth
			is sufficient, try some checking moves. We set the state of the
			move selector and return to the move loop. */
		if (!sb->pv_found && !sb->check && !sb->threat && sb->depth > 0)
		{
			sb->select_state = SELECT_GEN_CHECKS;
			sb->threat = TRUE;
			goto moves;
		}

		/* We can detect checkmate if all quiescence nodes up to now have
			included check evasions. */
		if (sb->moves == 0 && sb->check && sb->depth > 0)
		{
			hash_store(sb, NO_MOVE, -MATE + sb->ply, HASH_EXACT_BOUND, TRUE);
			RETURN(-MATE + sb->ply);
		}
		
		/* Store the result in the hash table and return. */
		if (sb->pv_found)
			hash_store(sb, board.pv_stack[sb->ply][0], sb->best_score,
				HASH_EXACT_BOUND, TRUE);
		else
			hash_store(sb, NO_MOVE, sb->best_score, HASH_UPPER_BOUND, TRUE);

		RETURN(sb->best_score);

#ifdef SMP

case SEARCH_WAIT:

		/* We're idle at this point, so call smp_wait() to try to find work. */
		smp_wait(&sb);

		/* Check for timeout or SMP events. */
		SEARCH_CHECK();

		goto end;

case SEARCH_CHILD_RETURN:

		/* A child has returned to this point, which means that we finished
			searching a root move. */
		SMP_DEBUG(print("cpu %i flagging return, %E\n", board.id, sb + 1));
		ASSERT(board.game_entry == root_entry + 1);
		ASSERT(board.split_point == board.split_point_stack);
		ASSERT(board.split_ply == board.split_ply_stack);
		unmake_move();

		/* Notify the main processor that the search is done and it needs to
			return. This lock should never be needed, as only this processor
			should be active, but just in case... */
		LOCK(smp_data->lock);
		smp_data->return_value = RETURN_VALUE;
		copy_pv(smp_data->return_pv, board.pv_stack[1]);
		smp_data->return_flag = TRUE;
		UNLOCK(smp_data->lock);

		/* Set up the search stack again and then start waiting. */
		sb++;
		sb->search_state = SEARCH_WAIT;
		set_idle();
		goto end;

#endif

/* The master process has finished searching. Presumably all other processes
	are in SEARCH_WAIT. */
case SEARCH_RETURN:

#ifdef SMP
		ASSERT(board.id == 0);

		set_active();
		/* This is kind of hacky... */
		if (board.game_entry == root_entry)
			make_move((zct->next_root_move - 1)->move);
#endif

		return RETURN_VALUE;

/* Do basic node processing: time, input, and messages in SMP. */
end:
		break;
default:
		fatal_error("ERROR: corrupted search_state. sb=%i\n", sb->search_state);
	} /* <-- This brace is from the switch, which isn't indented. */
	}
}
