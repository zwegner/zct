/** ZCT/time.c--Created 070305 **/

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

#ifdef ZCT_POSIX
#	include <sys/time.h>
#elif defined(ZCT_WINDOWS)
#	include <time.h>
#endif

struct
{
	int moves;
	int moves_left;
	int base;
	int increment;
	int max_time;
} time_control;

struct
{
	int time_left;
	int time_used;
} player_clock[2];

int last_side;
int last_time;
int time_limit_hard;
int time_limit_soft;
int search_start_time;

/**
get_time():
Returns the time for CPU or wall clock.
Created 070305; last modified 100907
**/
unsigned int get_time(void)
{
#ifdef ZCT_POSIX
	struct timeval timeval;

	gettimeofday(&timeval, NULL);
	return timeval.tv_sec * 1000 + (timeval.tv_usec / 1000);
#elif defined(ZCT_WINDOWS)
	return clock();
#endif
}

/**
start_time():
At the beginning of a search, this is called to start the timer. We need to have
a separate timer than the actual game clock, as things like pondering and
analyzing get much more complicated without it.
Created 102706; last modified 030709
**/
void start_time(void)
{
	search_start_time = get_time();

	zct->stop_search = FALSE;
}

/**
time_used():
Returns the amount of time used so far during the search.
Created 090206; last modified 102706
**/
int time_used(void)
{
	int side;
	int time;

	/* TODO: add support for pausing and add in accumulated search time */
	side = (board.side_tm == zct->zct_side);
	time = get_time();
	return time - search_start_time;
}

/**
time_is_up():
Called by the search to see if it is time to stop searching yet.
Created 090206; last modified 011608
**/
BOOL time_is_up(void)
{
	int time;
	int pv_changes;
	int risk;
	VALUE score_diff;

	time = time_used();
	
	/* Test if we are in one of the three "max" modes, and check for a timeout
		in each them. */
	if (zct->max_depth || zct->max_nodes || time_control.max_time)
	{
		/* Make sure the node count is updated from SMP search. */
		sum_counters();
		if ((zct->max_depth && zct->current_iteration > zct->max_depth) ||
			(zct->max_nodes && zct->nodes + zct->q_nodes > zct->max_nodes) ||
			(time_control.max_time && time > time_control.max_time))
			return TRUE;
		return FALSE;
	}

	/* If we are not out of our soft time limit, or we do not have a time limit,
		just return. */
	if (time < time_limit_soft || zct->current_iteration <= 1 ||
		(zct->engine_state != NORMAL && zct->engine_state != TESTING))
		return FALSE;

	/* We are past the hard limit. Stop searching immediately. */
	if (time > time_limit_hard)
		return TRUE;

	/* Check that we have completed three plies, mainly so that the following
		risk code doesn't access invalid memory on very short searches, but also
		so that there is a (very) minimal acceptable search. */
	if (zct->current_iteration <= 3)
		return FALSE;

	/* If we are past the soft time limit, but not the hard, look for any
		search-related reasons that we might want to continue searching for now,
		namely pv changes or big score changes. */
	if (zct->best_score_by_depth[zct->current_iteration] == -MATE)
	{
		/* Use the data from the last iteration if we haven't got a score for
			this one yet. */
		score_diff = zct->best_score_by_depth[zct->current_iteration - 1] -
			zct->last_root_score;
		pv_changes = zct->pv_changes_by_depth[zct->current_iteration - 1];
	}
	else
	{
		score_diff = zct->best_score_by_depth[zct->current_iteration] -
			zct->last_root_score;
		pv_changes = zct->pv_changes_by_depth[zct->current_iteration];
	}

	/* Calculate the risk of stopping now based on the current root score and
		the number of times the PV changed, and extend time accordingly. Each
		risk point is worth 25% of the soft time limit. */

	/* Bad scores--extend a lot */
	if (score_diff < -300)
		risk = 16;
	else if (score_diff < -160)
		risk = 10;
	else if (score_diff < -100)
		risk = 6;
	else if (score_diff < -60)
		risk = 3;
	else if (score_diff < -30)
		risk = 1;

	/* Good scores--don't extend that much, mainly just make sure we're not
		just hitting some search instability or something similar. */
	else if (score_diff > 160)
		risk = 5;
	else if (score_diff > 70)
		risk = 3;
	else if (score_diff > 30)
		risk = 1;
	else
		risk = 0;

	/* PV changes. */
	risk += MAX(0, pv_changes - 1);

	risk = MIN(risk, 16);

	/* Now bring down the risk factor when the score is already very good or
		bad, because we don't want to waste a lot of extra time when the game
		is already decided and big score jumps are mostly meaningless. */
	if (ABS(zct->last_root_score) > 2000)
		risk /= 8;
	else if (ABS(zct->last_root_score) > 1400)
		risk /= 4;
	else if (ABS(zct->last_root_score) > 700)
		risk /= 2;
	else if (ABS(zct->last_root_score) > 300)
		risk--;

	/* Now see if we should stop with the calculated risk. */
	if (time < time_limit_soft + (time_limit_soft * risk / 4))
		return FALSE;
	return TRUE;
}

/**
set_time_limit():
This is called before searching to determine how long to search.
Created 090206; last modified 010908
**/
void set_time_limit(void)
{
	int time;

	if (time_control.max_time == 0 && zct->max_depth == 0 && zct->max_nodes == 0)
	{
		/* Fischer-style time controls with increment. We take a good chunk of
			the remaining time and add most of the increment. The idea is to
			build up time when the remaining time is low, and use a lot of it
			when we have more. This formula tries to work well for a wide range
			of time controls, where the increment is relatively big or small,
			e.g. 1'1" vs. 25'4". It's not perfect, but it's pretty close given
			the simple formula. */
		if (time_control.increment)
			time = player_clock[0].time_left / 36 +
				time_control.increment * 3 / 4;
		else if (time_control.moves)
		{
			/* We start with half of time/moves. Remember that
				time_control.moves_left is counted in half moves, so we don't
				need to do anything special except add 1 to prevent division
				by zero. */
			time = player_clock[0].time_left / (time_control.moves_left + 1);
			if (player_clock[0].time_left < time_control.base / 4)
				time = time / 2;
		}
		/* Sudden death. Just divide the remaining time by a constant, because
			we want to make sure we always have enough time for the next move.
			In addition, you can't make a formula that tries to distribute time
			more equally over each move without estimating the number of moves
			left in the game. */
		else
			time = player_clock[0].time_left / 32;

		/* Set both a "soft" and "hard" time limit. Most moves will use the soft
			limit, but ZCT will use more time in critical searches. */
		time_limit_soft = MIN(time, player_clock[0].time_left / 16);
		time_limit_hard = MIN(time * 4, player_clock[0].time_left / 4);

		/* Print out the time usage information. */
		if (zct->protocol != XBOARD && zct->protocol != ANALYSIS &&
			zct->protocol != ANALYSIS_X && zct->protocol != UCI)
			print("Time: (us=%T them=%T) soft=%T hard=%T\n",
				player_clock[0].time_left, player_clock[1].time_left,
				time_limit_soft, time_limit_hard);
	}
}

/**
update_clocks():
Updates the timers, stops the side that moved's clock and starts the side to move's clock.
Created 082406; last modified 090807
**/
void update_clocks(void)
{
	int side;
	int time;

	side = (zct->zct_side == board.side_tm);
	time = get_time();
	/* Check if the side to move hasn't changed. */
	if (last_side != EMPTY && board.side_tm != last_side)
	{
		/* Update the side not to move's clock, because they just moved. */
		if (last_time)
			player_clock[side].time_left -= time - last_time;
		/* Adjust for Fischer-style clocks. */
		if (time_control.increment)
			player_clock[side].time_left += time_control.increment;
		/* Adjust for X moves/Y time-type time controls. */
		if (time_control.moves && time_control.moves_left-- <= 0)
		{
			player_clock[0].time_left += time_control.base;
			player_clock[1].time_left += time_control.base;
			time_control.moves_left = time_control.moves * 2;
		}
		last_time = time;
		player_clock[COLOR_FLIP(side)].time_used = 0;
	}
	last_side = board.side_tm;
}

/**
set_time_control():
Sets the time control to the given values. If the "st" command is used, only increment will be non-zero.
Created 082406; last modified 010908
**/
void set_time_control(int moves, int base, int increment)
{
	time_control.moves = moves;
	time_control.base = base;
	if (moves == 0 && base == 0)
	{
		time_control.increment = 0;
		time_control.max_time = increment;
	}
	else
	{
		time_control.increment = increment;
		time_control.max_time = 0;
	}
	if (moves)
		time_control.moves_left = 2 * moves;
	reset_clocks();
}

/**
reset_clocks():
Sets the clocks back to the starting time.
Created 082406; last modified 082406
**/
void reset_clocks(void)
{
	COLOR color;

	for (color = WHITE; color <= BLACK; color++)
		player_clock[color].time_left = time_control.base;
	last_time = 0;
	last_side = EMPTY;
}

/**
stop_clocks():
Stops both clocks.
Created 082506; last modified 082506
**/
void stop_clocks(void)
{
}

/**
set_zct_clock():
Sets ZCT's clock to a certain time. This is used by the "time" command.
Created 082506; last modified 082506
**/
void set_zct_clock(int time)
{
	player_clock[0].time_left = time;
	last_time = get_time();
}

/**
set_opponent_clock():
Sets the opponent's clock to a certain time. This is used by the "otim" command.
Created 090206; last modified 090206
**/
void set_opponent_clock(int time)
{
	player_clock[1].time_left = time;
	last_time = get_time();
}
