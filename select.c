/** ZCT/select.c--Created 081505 **/

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

#define SCORE_HASH_MOVE		(6001)
#define SCORE_THREAT_MOVE	(6000)
#define SCORE_COUNTER		(4004)
#define SCORE_KILLER		(4002)
#define SCORE_WIN_CAP		(5200)
#define SCORE_EVEN_CAP		(4100)
#define SCORE_LOSE_CAP		(4000)
#define SCORE_EVEN_MAT		(2000)
#define SCORE_PPAWN			(500)

#define SCORE_PCSQ_SCALE	(64)

/**
score_move():
Scores a move based on standard move-ordering terms. Returns the result of the static exchange evaluator.
Created 101307; last modified 081608
**/
VALUE score_move(SEARCH_BLOCK *sb, MOVE *move)
{
	int x;
	int score;
	unsigned int flags;
	SQUARE from;
	SQUARE to;
	VALUE see_score;
	VALUE pcsq_score;

	score = 0;
	see_score = 0;
	flags = 0;
	/* Check if the move is a killer or a counter move. */
	for (x = 0; x < 2 && score == 0; x++)
	{
		if (sb->ply > 0 && *move == zct->counter_move[board.side_tm]
			[MOVE_KEY((sb - 1)->move)][x])
			score = SCORE_COUNTER - x;
	}
	for (x = 0; x < 2 && score == 0; x++)
	{
		if (*move == zct->killer_move[sb->ply][x])
			score = SCORE_KILLER - x;
	}
	/* If the move wasn't a killer or counter move, give a regular score. */
	if (score == 0)
	{
		from = MOVE_FROM(*move);
		to = MOVE_TO(*move);
		/* captures */
		see_score = see(to, from);
		/* Check for illegal move. */
		if (see_score == -MATE)
			return see_score;
		score = see_score;
		if (board.color[to] != EMPTY)
		{
			if (see_score >= 0)
			{
				flags |= MOVE_NODELPRUNE;
				flags |= MOVE_NOREDUCE;
				if (see_score == 0)
					score += SCORE_EVEN_CAP;
				else
					score += SCORE_WIN_CAP;
			}
			else
				score += SCORE_LOSE_CAP;
		}
		else
			score += SCORE_EVEN_MAT;

		/* Piece-square-based bonus. This can be positive or negative, so we
			scale it to have zero be the halfway point in the range. */
		pcsq_score = piece_square_value[board.side_tm][board.piece[from]][to] -
			piece_square_value[board.side_tm][board.piece[from]][from] +
			SCORE_PCSQ_SCALE / 2;
		pcsq_score = MIN(SCORE_PCSQ_SCALE, MAX(0, pcsq_score));
		score += pcsq_score;

		if (MASK(to) & sb->eval_block.good_squares[board.side_tm])
		{
			score += 64;
			flags |= MOVE_NOREDUCE;
		}
		/* If we have a piece-square score high enough, make sure we don't
			reduce the move. */
//		if (pcsq_score > zct->lmr_threshold * SCORE_PCSQ_SCALE / 100)
//			flags |= MOVE_NOREDUCE;
		/* passed pawn bonus */
		if (board.piece[from] == PAWN &&
			RANK_OF(SQ_FLIP_COLOR(to, board.side_tm)) == RANK_7 &&
			see_score >= 0)
		{
			score += SCORE_PPAWN;
			flags |= MOVE_NOREDUCE;
		}
	}
	ASSERT(score >= 0 && score < MAX_SCORE);
	*move |= SET_SCORE(score) | flags;

	return see_score;
}

/**
score_moves():
Generates and sets the score for each move on the stack.
Created 082606; last modified 011108
**/
void score_moves(SEARCH_BLOCK *sb)
{
	MOVE *move;
	int score;

/*	if (sb->check)
		sb->last_move = generate_evasions(sb->first_move, sb->check);
	else
		sb->last_move = generate_moves(sb->first_move);
*/
	sb->last_move = generate_legal_moves(sb->first_move);
	for (move = sb->first_move; move < sb->last_move; move++)
	{
		/* Score the move. */
		score = score_move(sb, move);

		/* Check for an easy speedup: SEE determined that this is an
			illegal king capture. */
		if (score == -MATE)
			*move-- = *--sb->last_move;
	}
	sb->next_move = sb->first_move;
}

/**
score_caps():
Generates and sets the score for each move on the stack, also throwing away captures deemed losing by SEE.
Created 082606; last modified 072108
**/
void score_caps(SEARCH_BLOCK *sb)
{
	VALUE score;
	MOVE *move;

	sb->last_move = generate_captures(sb->first_move);
	for (move = sb->first_move; move < sb->last_move; move++)
	{
		/* MVV/LVA */
		score = board.piece[MOVE_TO(*move)] * 8 -
				board.piece[MOVE_FROM(*move)] + 8;
		*move |= SET_SCORE(score);
	}
	sb->next_move = sb->first_move;
}

/**
score_checks():
Generates and sets the score for each check on the stack.
Created 110606; last modified 110607
**/
void score_checks(SEARCH_BLOCK *sb)
{
	VALUE score;
	MOVE *move;

	sb->last_move = generate_checks(sb->first_move);
	for (move = sb->first_move; move < sb->last_move; move++)
	{
		score = board.piece[MOVE_TO(*move)] * 8 -
				board.piece[MOVE_FROM(*move)] + 8;
		*move |= SET_SCORE(score);
	}
	sb->next_move = sb->first_move;
}

/**
select_move():
Selects the highest scored move from the move list.
Created 081505; last modified 122008
**/
MOVE select_move(SEARCH_BLOCK *sb)
{
	int best_score;
	MOVE *m;
	MOVE *best_move;
	MOVE move;

#ifdef SMP
	SPLIT_POINT *sp;

	sp = *board.split_point;
	/* Check if we're in SMP mode and on a split ply. If so, we need to grab
		the move in SMP-safe way from the split point's move list. */
	if (sb->ply == *board.split_ply)
	{
		LOCK(sp->move_lock);
		sb = sp->sb;

		ASSERT(sb->ply == *board.split_ply);
	}
#endif

	move = NO_MOVE;
	/* Select a move according to the move state. Some of these case statements
		are fall-through, because if a move is valid it will be returned, and
		otherwise the next state is used to find one. */
	switch (sb->select_state)
	{
		case SELECT_HASH_MOVE:
			sb->select_state = SELECT_THREAT_MOVE;
			move = sb->hash_move;
			move |= SET_SCORE(SCORE_HASH_MOVE);
			if (move_is_valid(move))
				goto done;

		case SELECT_THREAT_MOVE:
			sb->select_state = SELECT_GEN_MOVES;
			move = board.threat_move[sb->ply];
			move |= SET_SCORE(SCORE_THREAT_MOVE);
			if (move_is_valid(move))
				goto done;

		case SELECT_GEN_MOVES:
			sb->select_state = SELECT_MOVE;
			score_moves(sb);

		case SELECT_MOVE:
select_move:
			if (sb->next_move >= sb->last_move)
			{
				move = NO_MOVE;
				goto done;
			}

			best_score = MOVE_SCORE(*sb->next_move);
			best_move = sb->next_move;
			for (m = sb->next_move + 1; m < sb->last_move; m++)
			{
				if (MOVE_SCORE(*m) > best_score)
				{
					best_score = MOVE_SCORE(*m);
					best_move = m;
				}
			}
			/* Swap the best move with the current move. We do this even in the
				case that they are the same, but it doesn't matter. */
			move = *best_move;
			*best_move = *sb->next_move;
			*sb->next_move = move;

			sb->next_move++;
			/* Check if the move is the hash move or the threat move, and skip
				it if it is. We do this after incrementing next_move, so we
				make sure to skip it. */
			if (move == sb->hash_move || move == board.threat_move[sb->ply])
				goto select_move;

			break;

		default:
			/* Compiler shut-up code */
			break;
	}

done:

#ifdef SMP
	if (sb->ply == *board.split_ply)
	{
		UNLOCK(sp->move_lock);
		SMP_DEBUG(print("cpu %i at %i got move %M, score %i\n", board.id,
			(*board.split_point)->id, move, MOVE_SCORE(move)));
	}
#endif
	return move;
}

/**
select_qsearch_move():
Selects the highest scored move from the move list.
Created 122008; last modified 122008
**/
MOVE select_qsearch_move(SEARCH_BLOCK *sb)
{
	VALUE eval;
	MOVE *m;
	MOVE *best_move;
	MOVE move;
	int score;
	int best_score;

	/* Select a move according to the move state. Some of these case statements
		are fall-through, because if a move is valid it will be returned, and
		otherwise the next state is used to find one. */
	switch (sb->select_state)
	{
		case SELECT_HASH_MOVE:
			sb->select_state = SELECT_THREAT_MOVE;
			move = sb->hash_move;
			move |= SET_SCORE(SCORE_HASH_MOVE);
			if (move_is_valid(move))
				return move;

		case SELECT_THREAT_MOVE:
			sb->select_state = SELECT_GEN_MOVES;
			move = board.threat_move[sb->ply];
			move |= SET_SCORE(SCORE_THREAT_MOVE);
			if (move_is_valid(move))
				return move;

		case SELECT_GEN_MOVES:
			sb->select_state = SELECT_MOVE;
			/* We're in qsearch, and we either search all moves if we're in
				check, or just captures if we're not. */
			if (sb->check)
				score_moves(sb);
			else
				score_caps(sb);
			goto select_move;

		case SELECT_GEN_CHECKS:
			/* All captures were unsuccessful, so we try to generate some
				non-capturing checks. */
			sb->select_state = SELECT_MOVE;
			score_checks(sb);
			goto select_move;

		case SELECT_MOVE:
select_move:
			if (sb->next_move >= sb->last_move)
				return NO_MOVE;
			best_score = MOVE_SCORE(*sb->next_move);
			best_move = sb->next_move;
			for (m = sb->next_move + 1; m < sb->last_move; m++)
			{
				if (MOVE_SCORE(*m) > best_score)
				{
					best_score = MOVE_SCORE(*m);
					best_move = m;
				}
			}
			/* Swap the best move with the current move. We do this even in the
				case that they are the same, but it doesn't matter. */
			move = *best_move;
			*best_move = *sb->next_move;
			*sb->next_move = move;

			sb->next_move++;
			/* Check if the move is the hash move or the threat move, and skip
				it if it is. We do this after incrementing next_move, so we
				make sure to skip it. */
			if (move == sb->hash_move || move == board.threat_move[sb->ply])
				goto select_move;

			/* Delta pruning. We prune a capture if it is losing (SEE < 0) or
				if the material gain cannot bring the score up to alpha. */
	//		if (!sb->check && !sb->threat)
			{
				score = see(MOVE_TO(move), MOVE_FROM(move));
				if (score < 0)
					goto select_move;

				eval = sb->eval_block.full_eval;
			if (!sb->check && !sb->threat)
				if (score + 50 + eval < sb->alpha)
					goto select_move;
			}

			return move;

		default:
			/* Compiler shut-up code */
			break;
	}

	return NO_MOVE;
}

/**
select_root_move():
Selects the highest scored move from the move list.
Created 082606; last modified 110606
**/
ROOT_MOVE *select_root_move(void)
{
	ROOT_MOVE *move;
	ROOT_MOVE *best_move;
	ROOT_MOVE temp;

	if (zct->next_root_move >= zct->root_move_list + zct->root_move_count)
		return NULL;
	best_move = zct->next_root_move;
	for (move = best_move + 1;
		move < zct->root_move_list + zct->root_move_count; move++)
	{
		/* Order the best move first, and order all other moves after it
			according to the node count of its subtree last iteration. */
		if (move->score > best_move->score || (move->nodes > best_move->nodes &&
			best_move->score != zct->root_pv_counter))
			best_move = move;
	}
	if (best_move != zct->next_root_move)
	{
		temp = *best_move;
		*best_move = *zct->next_root_move;
		*zct->next_root_move = temp;
	}
	return zct->next_root_move++;
}
