/** ZCT/smp2.c--Created 102806 **/

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
#include "smp.h"
#include "stats.h"
#include <math.h>

#ifdef SMP

/**
split():
In the tree, sets up the data for using multiple processors on a single node.
Created 081706; last modified 031909
**/
int split(SEARCH_BLOCK *sb, ID id)
{
	SMP_BLOCK *block;
	SPLIT_POINT *sp;
	SEARCH_BLOCK *new_sb;
	GAME_ENTRY *current;
	int ply;

	/* Back up the move stack until we are at the position of the new
	   split point. We don't unmake the moves as we're only copying the move
	   list, not the board position. */
	current = board.game_entry;
	new_sb = sb;
	while (new_sb > board.search_stack && new_sb->id != id)
	{
		/* Check if we have made a move that needs to be unmade at this ply. */
		if (new_sb->move_made)
			current--;
		new_sb--;
	}
	/* Make sure this is the intended split point. */
	if (new_sb->id != id)
		return -1;

	/* If there is a move made at this node, "back up" one more ply in the
	   history so we get the board position from the start of the node. */
	if (new_sb->move_made)
		current--;

	/* Sanity checks for the ply */
	ply = new_sb->ply;

	ASSERT(ply > 0 && ply < MAX_PLY);

	/* We don't split above existing split points, as it's far too complicated.
	   Check old versions of ZCT to see the code that attempted to do this. */
	if (*board.split_ply > ply)
		return -1;
	/* See if we already have split at the specified ply. */
	else if (*board.split_ply == ply)
		return (*board.split_point)->n;

	/* Check for overflow of split_point[] or board.split_point_stack[]. */
	block = &smp_block[board.id];

	/* Find a split point. */
	for (sp = &split_point[board.id * MAX_CPUS];
			sp < &split_point[(board.id + 1) * MAX_CPUS]; sp++)
	{
		if (sp->active == FALSE)
			break;
	}
	if (sp >= &split_point[(board.id + 1) * MAX_CPUS])
		return -1;

	/* We found a split point. Start to split. */
	LOCK(sp->lock);

	/* Each split point gets a unique id to handle asynchronous messages.
		We need to be able to handle messages from a processor at a split
		point that the current processor has already left. */
	sp->id = block->split_id;
	block->split_id += MAX_CPUS;

	SMP_DEBUG(print("cpu %i splitting, sp=%i id=%i %E\n",
		board.id, sp->n, sp->id, new_sb));
	
	/* Set the data in BOARD to tell where this split point is. */
	*++board.split_ply = new_sb->ply;
	*++board.split_point = sp;
	
	/* Copy the board position to shared memory. */
	smp_copy_to(sp, current, new_sb);

	/* Set up some search information for the split point. */
	sp->sb = sp->board.search_stack + (new_sb - board.search_stack);
	sp->sb->search_state = SEARCH_JOIN;
	sp->sb->move_made = FALSE;
	sp->pv[0] = NO_MOVE;

	/* Some SMP bookkeeping information... */
	sp->is_child[board.id] = TRUE;
	sp->child_count = 1;
	sp->no_moves_left = FALSE;

	/* We only change this after the split point is ready, as other processors
		will try to split here. */
	sp->active = TRUE;

	UNLOCK(sp->lock);

	/* Update global statistics. This is not atomic, so it could be bogus... */
	smp_data->splits_done++;

	/* Reset our split score. */
	update_best_sb(sb, TRUE);

	/* Now go back to the position we were searching. */
	while (current < board.game_entry)
		current++;
	return sp->n;
}

/**
attach():
This attaches an idle process onto an existing split point. It returns if the
split point is already dead.
Created 082806; last modified 090208
**/
SEARCH_BLOCK *attach(int sp)
{
	SPLIT_POINT **s;
	SEARCH_BLOCK *sb;

	LOCK(split_point[sp].lock);
	/* Check that the split point is still valid. */
	if (!split_point[sp].active || split_point[sp].no_moves_left)
	{
		UNLOCK(split_point[sp].lock);
		SMP_DEBUG(print("cpu %i failed attaching, sp %i no longer active...\n",
			board.id, sp));
		return NULL;
	}

	/* Lock all split points under this one as well, as we need to
		attach "atomically". */
	for (s = split_point[sp].board.split_point - 1; *s != NULL; s--)
		LOCK((*s)->lock);

	/* Copy the board from the split point to our local board by making all
		the moves in its game history. Then attach to all the split points
		below this one. Note that all of them are still locked. */
	smp_copy_from(&split_point[sp], split_point[sp].sb);
	
	/* Now step back through the tree, updating the child count on each
		split point and releasing its lock. */
	for (s = board.split_point_stack + 1; s <= board.split_point; s++)
	{
		(*s)->child_count++;
		(*s)->is_child[board.id] = TRUE;

		SMP_DEBUG(print("cpu %i attaching to %i, now %i children and "
			"%i splitpoints.\n%B", board.id, (*s)->id, (*s)->child_count,
			s - board.split_point_stack, &board));

		UNLOCK((*s)->lock);
	}

	/* We now take away our idle status.  */
	set_active();

	/* Correct our local information. */
	sb = board.search_stack +
		(split_point[sp].sb - split_point[sp].board.search_stack);

	return sb;
}

/**
merge():
This function is called when we get a value > alpha at a split point, meaning
that other processors might be doing unnecessary work, and need to check.
Created 082906; last modified 052707
**/
void merge(SEARCH_BLOCK *sb)
{
	int x;
	SPLIT_POINT *sp;

	sp = *board.split_point;
	LOCK(sp->lock);
	/* Update the best score. */
	if (sb->best_score >= sp->sb->best_score)
		sp->sb->best_score = sb->best_score;
	/* Make sure this value is still the best. If it is, we need some
		special handling so that the other processors know. */
	if (sb->best_score >= sp->sb->alpha)
	{
		sp->sb->alpha = sb->best_score;
		/* This is just used for the bound type on hash stores. */
		sp->sb->pv_found = sb->pv_found;
		copy_pv(sp->pv, board.pv_stack[sb->ply]);
		
		SMP_DEBUG(print("cpu %i merging score=%V pv=%lM %E\n",
			board.id, sb->best_score, board.pv_stack[sb->ply], sb));
		/* Tell the other processors to update. The other processors need to
			update their bounds to increase efficiency. Note that update() also
			handles fail highs at split points, which we might have here. */
		for (x = 0; x < zct->process_count; x++)
			if (x != board.id && sp->is_child[x])
			{
				sp->update[x] = TRUE;
				smp_message(x, SMP_UPDATE, sp->id);
			}
	}

	/* Check for beta cutoffs separately, against our local beta. While our
		best score might not be the best score for the whole split point
		anymore, it still causes a cutoff and thus requires us to clean up
		our local stuff. Note that in most cases our beta is best, and this
		code handles that too. */
	if (sb->best_score >= sb->beta)
	{
		SMP_DEBUG(print("cpu %i stopping at %E\n", board.id, sb));

		/* Make sure no other processors try to attach here. */
		sp->no_moves_left = TRUE;
	
		/* If our score is the best for the split point, we are officially
			causing a STOP. This is bad. Note that another processor could
			still back up another value greater than this beta, but luckily
			we are only updating a statistic here! */
		if (sb->best_score >= sp->sb->beta)
		{
			smp_data->stops_done++;

			/* Update some statistics. */
			STATA_INC("stops by ply", sb->ply);
			STATA_INC("stops by depth", sb->depth / PLY);
			STATA_INC("stops by node type", sb->node_type);
		}
	
		UNLOCK(sp->lock);

		/* Now release the split point, detach, and go away. */
		detach(sb);
	}
	else
		UNLOCK(sp->lock);
}

/**
update():
This function is called when another processor has merged a value greater than
alpha at a split point. We need to check if we are doing unnecessary work, and
update all the old bounds.
Created 082906; last modified 042007
**/
SEARCH_BLOCK *update(SEARCH_BLOCK *sb, int id)
{
	int side;
	int new_bound;
	SEARCH_BLOCK *new_sb;
	SPLIT_POINT **sp;

	/* Find the split point with the given id. */
	for (sp = board.split_point; *sp != NULL; sp--)
		if ((*sp)->id == id)
			break;
	/* Check if the split point is no longer active. */
	if (*sp == NULL)
		return sb;
	
	LOCK((*sp)->lock);
	/* Check if we've already updated here. This can happen if two processors
		get values > alpha one after another and then send two messages, when
		we only need to update once. */
	if ((*sp)->update[board.id] == FALSE)
	{
		UNLOCK((*sp)->lock);
		return sb;
	}
	(*sp)->update[board.id] = FALSE;
	/* From the search block of the split point, traverse down the tree and
		update all bounds. */
	new_sb = (*sp)->sb - (*sp)->board.search_stack + board.search_stack;
	new_bound = (*sp)->sb->alpha;
	UNLOCK((*sp)->lock);
	side = 0;
	for (; new_sb <= sb; new_sb++)
	{
		/* Check to see if we can narrow the window. If it's the other side to
			move, we negate the bound and check against beta, otherwise just
			check against alpha. */
		if (side == 0)
		{
			if (new_bound > new_sb->best_score)
				new_sb->best_score = new_bound;
			if (new_bound > new_sb->alpha)
				new_sb->alpha = new_bound;
		}
		else
		{
			if (-new_bound < new_sb->beta)
				new_sb->beta = -new_bound;
		}
		/* Somewhere down the tree, possibly the split point, we caused a
			beta cutoff, so back up a bit. */
		if (new_sb->alpha >= new_sb->beta)
		{
			new_sb->return_value = side == 0 ? new_bound : -new_bound;
			/* Right now, there is a node, possibly the split point that got
				updated, which has alpha>=beta. This is because alpha was
				raised at a split point, and the node in question either
				raised its alpha to meet the split point, or lowered its beta.
				We need to back up to the node right above this one, as if it
				returned beta in a fail high. We backup from our current node
				to the node that got cutoff. */
			while (sb >= new_sb)
			{
				/* We must check for a complicated situation where there are
					one or more split points below the node that has been
					updated. The change in alpha can cause the other split
					points to have beta cutoffs, so we can't just return, we
					have to clean up the split point. We don't send any
				   	messages in this situation, as all relevant processors
				   	will receive an UPDATE message from the PV node. We only
				   	detach ourselves from the split point, and in case we are
				   	the last child, unsplit it. */
				if (sb->ply == *board.split_ply)
				{
					SMP_DEBUG(print("cpu %i updating below split point\n",
						board.id));
					detach(sb);
					/* Check if we didn't unsplit, but instead we got kicked
						into the idle loop. This means there are other
						processors still attached to the split point that
						need to detach, and then one of them will back up
						the score. So we're all good here, find something
						else to do. */
					if (sb->search_state == SEARCH_WAIT)
						return sb;
				}
				/* Check if we have made a move that needs to be unmade at
					this ply. */
				if (sb->move_made)
				{
					sb->move_made = FALSE;
					unmake_move();
				}
				sb--;
			}
			SMP_DEBUG(print("cpu %i updated, cutoff at %E\n%B",
				board.id, new_sb, &board));
			/* Continue searching at the node below the beta cutoff. */
			return new_sb - 1;
		}
		if (new_sb->move_made)
			side ^= 1;
	}
	return sb;
}

/**
detach():
This function is called whenever a processor leaves a split point after
finishing all its moves. It checks if there is only one processor at the split
point and if so tells it to unsplit. Otherwise it detaches from every split
point it is attached to and enters a waiting state.
Created 082906; last modified 052708
**/
void detach(SEARCH_BLOCK *sb)
{
	int child_count;

	/* This block of code is used to prevent a deadlock that can occur when a
		processor joins the split point or a split point under it. */
	LOCK((*board.split_point)->lock);
	(*board.split_point)->no_moves_left = TRUE;
	child_count = (*board.split_point)->child_count;
	UNLOCK((*board.split_point)->lock);

	/* Now do the normal detaching. */
	if (child_count <= 1)
		unsplit(sb, (*board.split_point)->id);
	else
		stop(sb);
}

/**
stop():
This function is called when a processor needs to detach from every split point
it is in. This is called in three situations: Firstly, when the processor
finishes searching its moves at a ply and there are still one or more
processors working at the split point; secondly, when another processor gets a
beta cutoff at a split point we are working on; and lastly, when the search is
aborted and all child processors need to return to an idle state.
Created 082207; last modified 110608
**/
void stop(SEARCH_BLOCK *sb)
{
	int x;
	SPLIT_POINT **sp;

	/* Go through and detach from each split point. */
	for (sp = board.split_point; *sp != NULL; sp--)
	{
		LOCK((*sp)->lock);
		SMP_DEBUG(print("cpu %i detaching from sp %i, %i children left\n",
			board.id, (*sp)->id, (*sp)->child_count));

		/* Check if we are the last processor already. */
		if ((*sp)->child_count <= 1)
		{
			UNLOCK((*sp)->lock);
			unsplit(sb, (*sp)->id);
			return;
		}
		/* Otherwise, remove this processor from the split point. */
		(*sp)->child_count--;
		(*sp)->is_child[board.id] = FALSE;
	
		/* Correct our local split point information. */
		board.split_point--;
		board.split_ply--;
	
		/* Now check if there is only one other processor, and tell them to
			unsplit this split point. */
		if ((*sp)->child_count == 1)
		{
			for (x = 0; x < zct->process_count; x++)
				if ((*sp)->is_child[x])
				{
					SMP_DEBUG(print("cpu %i telling cpu %i to unsplit %i.\n",
						board.id, x, (*sp)->id));
					smp_message(x, SMP_UNSPLIT, (*sp)->id);
				}
		}
		UNLOCK((*sp)->lock);
	}
	/* Go back to the root position to get ready for the next split point. */
	while (board.game_entry > root_entry)
		unmake_move();

	/* Tell the other processors we are idle. */
	set_idle();
	sb->search_state = SEARCH_WAIT;
}

/**
unsplit():
If we are the last processor in a split point, then we need to destroy it.
Also we copy the pv from the split point into local memory if there is one,
so that we may return it. The function returns whether the unsplit was
successful or not.
Created 082906; last modified 110608
**/
void unsplit(SEARCH_BLOCK *sb, int id)
{
	SEARCH_BLOCK *new_sb;
	SPLIT_POINT **sp, **bottom;

	/* Find the split point with the given id. */
	for (bottom = board.split_point; *bottom != NULL; bottom--)
		if ((*bottom)->id == id)
			break;
	/* Check if the split point is no longer active. */
	if (*bottom == NULL)
		return;

	/* Update our local score if needed. */
	if ((*bottom)->update[board.id])
	{
		SMP_DEBUG(print("cpu %i updating bottom sp %i\n",
			board.id, (*bottom)->id));

		/* Find the search block for the split point. */
		new_sb = sb;
		while (new_sb->ply > (*bottom)->sb->ply)
			new_sb--;
		ASSERT(new_sb->ply == (*bottom)->sb->ply);

		/* Update the best score for fail soft. */
		if ((*bottom)->sb->best_score > new_sb->best_score)
			new_sb->best_score = (*bottom)->sb->best_score;
		
		/* Update alpha and the PV. */
		if ((*bottom)->sb->alpha > new_sb->alpha)
		{
			new_sb->alpha = (*bottom)->sb->alpha;
			copy_pv(board.pv_stack[(*bottom)->sb->ply], (*bottom)->pv);

			SMP_DEBUG(print("cpu %i updating, new alpha=%V\n",
				board.id, new_sb->alpha));
		}
	}

	/* Now go along split points, down the tree. We "recursively" lock the
		split point stack so that unsplitting is atomic with respect to
		each split point. */
	for (sp = board.split_point; sp >= bottom; sp--)
	{
		LOCK((*sp)->lock);

		SMP_DEBUG(print("cpu %i unsplitting sp %i\n", board.id, (*sp)->id));
		
		(*sp)->no_moves_left = TRUE;

		/* Another split point has attached to the split point since we got
			the message. While this cannot happen directly (a processor
			wouldn't attach to a split point with no moves left...), if there
			is a split point below this one, another processor could attach to
			that one in order for it to "back up". */
		if ((*sp)->child_count > 1)
		{
			SMP_DEBUG(print("cpu %i failed unsplitting, sp %i has children.\n",
				board.id, (*sp)->id));
			
			for (; sp <= board.split_point; sp++)
				UNLOCK((*sp)->lock);

			return;
		}
	}
	/* Now that we have control over the split point stack, copy the board
		state from the split point over (we only do this when we unsplit
		to save memory traffic. */
//	smp_copy_unsplit(*bottom, (*bottom)->sb);

	/* Now go back up the split point stack, cleaning each one up. */
	for (sp = bottom; sp <= board.split_point; sp++)
	{
		/* Clean up the split point... */
		(*sp)->is_child[board.id] = FALSE;
		(*sp)->child_count = 0;
		/* Now make sure the move list on this ply is empty. Trust me, this
			can be a big problem. */
		new_sb = sb;
		while (new_sb->ply > (*sp)->sb->ply)
			new_sb--;
		new_sb->next_move = new_sb->last_move;
		new_sb->select_state = SELECT_MOVE;

		/* Update split stack information. */
		(*sp)->active = FALSE;

		UNLOCK((*sp)->lock);
	}

	/* Correct our local split point information. */
	for (sp = board.split_point; sp >= bottom; sp--)
	{
		board.split_point--;
		board.split_ply--;
	}
	ASSERT(board.split_point >= board.split_point_stack);
	ASSERT(board.split_ply >= board.split_ply_stack);

	return;
}

/**
smp_wait():
While an idle processor is looping in search, waiting for some work, it will
have to look at the search state of other processors. We check if there are any
active split points and try to split there, otherwise we find the best node for
a split point and tell the processor that owns it to split.
Created 102608; last modified 123008
**/
void smp_wait(SEARCH_BLOCK **sb)
{
	static int help_counter = 0;
	static int help_counter_init = 2;
	SEARCH_BLOCK *temp;
	int x;

	/* If a split point already exists, attach to that. */
	for (x = 0; x < MAX_SPLIT_POINTS; x++)
		if (split_point[x].active && !split_point[x].no_moves_left &&
				split_point[x].sb->last_move -
				split_point[x].sb->first_move > 1)
			break;
	if (x < MAX_SPLIT_POINTS)
	{
		SMP_DEBUG(print("cpu %i attaching to known split point %i\n",
					board.id, x));
		goto found_split_point;
	}

	/* help_counter is one or more, meaning we just tried to join a split
		point but it was unsuccessful. We wait for a bit before we try to
		split again, mostly so that active processors aren't bogged down. */
	if (help_counter > 0)
	{
		help_counter--;
		return;
	}

	x = find_split_point();
	if (x == -2)
		return;
	SMP_DEBUG(print("Find: %i\n", x));
	if (x == -1)
	{
		help_counter = 5;//help_counter_init;
//		help_counter_init++;
		return;
	}

found_split_point:
	help_counter = 0;
	help_counter_init = 2;
	/* Here we have found a split point to attach to, so clean up a bit and
	   then try to attach to it. */
	temp = attach(x);
	/* Check if the split point is still good. */
	if (temp == NULL)
	{
		SMP_DEBUG(print("cpu %i received error on attach.\n", board.id));
		return;
	}
	/* Now, turn our search block to the split point and start searching! */
	*sb = temp;
	SMP_DEBUG(print("cpu %i successfully attached. x=%i %E\n",
		board.id, x, *sb));
}

/**
initialize_split_score():
Sets up a SPLIT_SCORE structure that is used for selecting split points.
Created 031709; last modified 031709
**/
void initialize_split_score(SPLIT_SCORE *ss)
{
	ss->id = 0;
	ss->score = 0;
	ss->ply = 0;
#ifdef USE_STATS
	ss->depth = 0;
	ss->moves = 0;
	ss->moves_to_go = 0;
	ss->fh_score = 0;
	ss->depth_score = 0;
	ss->moves_score = 0;
	ss->node_type = 4;
#endif
}

/**
score_split_point():
Take a SEARCH_BLOCK of a tree state and evaluate it as a potential split point.
Created 090408; last modified 031709
**/
float score_split_point(SEARCH_BLOCK *sb, SPLIT_SCORE *ss)
{
	int moves_to_go;
	float score;
	float fh_score;
	float depth_score;
	float moves_score;
	
	moves_to_go = sb->last_move - sb->next_move;
	/* Make sure this is a valid node to split at. */
	if (sb->is_qsearch ||
		sb->search_state == SEARCH_RETURN ||
		sb->search_state == SEARCH_CHILD_RETURN)
		return -1;
	if (sb->moves < 1 || moves_to_go <= 2 || sb->depth < 2 * PLY)
		return -1;

		//	STATA_INC("split scores by depth", sb->depth);
	/* Score based on probability of a fail high. */
	if (sb->node_type == NODE_PV)
		fh_score = .1 + MIN((float)sb->moves / 20., 5.);
	else if (sb->node_type == NODE_ALL)
		fh_score = 5 + MIN((float)sb->moves / 4, 5.);
	else
		fh_score = .1 + MIN((float)sb->moves / 10., 5.);


	/* Score based on depth remaining at this node. */
	depth_score = sb->depth > 3 * PLY ? MIN(sb->depth, 10 * PLY) : .1;
//	depth_score = sb->depth;

	/* Get a score for the expected number of moves at this node. */
	moves_score = MIN(moves_to_go, 10);

	/* Get a score based on the three factors. */
	score = fh_score * depth_score * moves_score;

	/* Copy split score info. */
	initialize_split_score(ss);
	ss->id = sb->id;
	ss->score = score;
	ss->ply = sb->ply;
#ifdef USE_STATS
	ss->depth = sb->depth;
	ss->moves = sb->moves;
	ss->moves_to_go = moves_to_go;
	ss->fh_score = fh_score;
	ss->depth_score = depth_score;
	ss->moves_score = moves_score;
	ss->node_type = sb->node_type;
#endif

	return score;
}

/**
update_best_sb():
Whenever a node is updated in the tree, we check it against our best split
point candidate. This way we maintain for each processor the best place to
split, so that other processors can quickly examine our search space.
Created 090308; last modified 031909
**/
void update_best_sb(SEARCH_BLOCK *sb, BOOL recalculate)
{
	SEARCH_BLOCK *nsb;
	SPLIT_SCORE ss;
	TREE_BLOCK *tb;
	float score;
	int ply;
	int sply;

	if (sb->depth < 4 * PLY)
		return;

	tb = &smp_block[board.id].tree;
	sply = *board.split_ply;

	/* Score the current node. */
	score = score_split_point(sb, &ss);

	/* Incremental update. */
	if (recalculate || score != tb->sb_score[sb->ply].score)
	{
		tb->sb_score[sb->ply] = ss;
		tb->best_score = -1;
		for (ply = sb->ply; ply > sply && ply > 0; ply--)
		{
			if (tb->sb_score[ply].score > tb->best_score)
			{
				tb->best_id = tb->sb_score[ply].id;
				tb->best_score = tb->sb_score[ply].score;
				tb->best_ply = ply;
			}
		}
		else if (score != -1)
			return;
	}

	initialize_split_score(&tb->sb_score);
	tb->sb_score.id = 0;
	/* Since we don't split above an existing split point, loop until we
		have reached a split ply or get to the root. */
	for (nsb = sb; nsb >= board.search_stack && nsb->ply > sply; nsb--)
	{
		score = score_split_point(nsb, &ss);
		if (score > tb->sb_score.score)
			tb->sb_score = ss;
	}
}

/**
find_split_point():
After a HELP command is issued and one or more search states are copied, this
function looks at each node in the trees to see if any makes a good candidate
split point. If so, it tells the processor to split there.
Created 110907; last modified 031709
**/
int find_split_point(void)
{
	TREE_BLOCK *tb;
	SPLIT_SCORE ss;
	SPLIT_SCORE best_ss;
	BOOL is_sb;
	float score;
	int process;
	int best_process;
	int x;

	/* Make the best score be a minimum score for split points. */
//	best_score = log(zct->current_iteration / 4.);
//	best_score = MAX(best_score, .5);
	best_process = -1;
//	initialize_split_score(&best_ss);
	best_ss.score = .1;//zct->current_iteration / 4;//.1;

	/* Iterate through all search stacks that might have been copied. */
	for (process = 0; process < zct->process_count; process++)
	{
		if (process == board.id)
			continue;
		if (smp_block[process].idle)
			continue;

		tb = &smp_block[process].tree;
		if (tb->best_id > 0 && tb->best_score > best_ss.score)
		{
			best_ss = tb->sb_score[tb->best_ply];
		//	best_ss.score = tb->best_score;
		//	best_ss.id = tb->best_id;
			best_process = process;
			is_sb = TRUE;
		}
	}
	if (best_process != -1)
	{
		if (is_sb)
		{
			/* Tell the processor to split by ply and by search_block id, so
				that the correct split point is found. */
			x = smp_tell(best_process, SMP_SPLIT, best_ss.id);

			/* Increment some stats in case this was a successful split. */
			if (x != -1)
			{
				STATA_INC("splits by iteration", zct->current_iteration);
				STATA_INC("splits by ply", best_ss.ply);
				STATA_INC("splits by depth", best_ss.depth / PLY);
				STATA_INC("splits by moves", best_ss.moves);
				STATA_INC("splits by moves to go", best_ss.moves_to_go);
				STATA_INC("splits by fh score * 10",
					(int)(best_ss.fh_score * 10));
				STATA_INC("splits by depth score",
					(int)(best_ss.depth_score / 4));
				STATA_INC("splits by move score * 10",
					(int)(best_ss.moves_score * 10));
				STATA_INC("splits by score / 10", (int)(best_ss.score / 10));
				STATA_INC("splits by node type", best_ss.node_type);
			}

			SMP_DEBUG(print("cpu %i telling cpu %i to split on sb %i\n",
				board.id, best_process, best_ss.id));
			return x;
		}
		else
			return best_ss.id;
	}
	return -2;
}

#endif /* SMP */
