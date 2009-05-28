/** ZCT/debug.c--Created 102206 **/

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

#ifdef DEBUG_SEARCH

DEBUG_COND cond_stack[COND_STACK_SIZE];
int cond_count = 0;
DEBUG_COND_MODE cond_mode = DEBUG_COND_OR;

DEBUG_STATE debug =
{
	FALSE, FALSE, -1, 0, NULL
};

/**
debug_continue():
When the user enters the "continue" command, we need to reset some variables
so that we stop breaking and continue searching.
Created 083108; last modified 083108
**/
void debug_continue(void)
{
	debug.set_break = FALSE;
	debug.stop = FALSE;
	debug.ply = -1;
}

/**
debug_next():
When the user enters the "next" command, we set the variables to walk to the
next node and break again.
Created 083108; last modified 083108
**/
void debug_next(void)
{
	debug.set_break = TRUE;
	debug.stop = FALSE;
}

/**
debug.set_break():
When the user enters the "break" command, we stop searching and show the current
search state. The user can enter commands to walk around the search tree.
Created 083108; last modified 083108
**/
void debug_break(void)
{
	debug.set_break = TRUE;
}

/**
debug():
During the search, checks to see if any conditions are true. If so, it breaks
into the debug interface, where the user can step through the tree to see the
search in action.
Created 102206; last modified 083108
**/
void debug_step(SEARCH_BLOCK *search_block)
{
	int cond;
	BOOL stop;
	BOOL current;

	stop = FALSE;

	if (search_block->ply < debug.ply)
		debug.ply = -1;
	else if (debug.ply != -1 && search_block->ply > debug.ply)
		stop = TRUE;
	else if (debug.set_break)
		stop = TRUE;
	else
	{
		if (cond_mode == DEBUG_COND_OR)
			stop = FALSE;
		/* COND_AND needs to prove all the conditions aren't true, but we
			need to check that conditions are set. */
		else if (cond_count > 0)
			stop = TRUE;
		else
			stop = FALSE;

		/* Loop through all conditions. */
		for (cond = 0; cond < cond_count; cond++)
		{
			switch (cond_stack[cond].cond_type)
			{
				case DEBUG_ALWAYS:
					current = (cond_mode == DEBUG_COND_OR ? TRUE : FALSE);
					break;
				case DEBUG_HASHKEY:
					current = (((HASHKEY)cond_stack[cond].data) ==
						board.hashkey);
					break;
				case DEBUG_PLY:
					current = (((int)cond_stack[cond].data) ==
						search_block->ply);
					break;
				case DEBUG_MOVE:
					current = (MOVE_COMPARE(cond_stack[cond].data) ==
						MOVE_COMPARE(board.game_stack->move) &&
						cond_stack[cond].data >> 32 == search_block->ply);
					break;
				case DEBUG_SCORE_LT:
					break;
				case DEBUG_SCORE_GT:
					break;
				default:
					break;
			}
			if ((cond_mode == DEBUG_COND_OR && current == TRUE) ||
				(cond_mode == DEBUG_COND_AND && current == FALSE))
			{
				stop = current;
				break;
			}
		}
	}
	if (stop)
	{
		if (debug.ply == -1)
			debug.ply = search_block->ply;
		debug.stop = TRUE;

		while (debug.stop)
		{
			debug_print(search_block);
			prompt();
			if (zct->input_buffer[0] == '\0')
				read_line();
			else
				print("%s\n", zct->input_buffer);
			if (command(zct->input_buffer) != CMD_GOOD)
				debug.stop = FALSE;
		}
	}
}

/**
debug_set():
Sets a condition to break the search on.
Created 102206; last modified 051507
**/
void debug_set(DEBUG_COND_TYPE cond_type, BITBOARD data)
{
	int cond;
	for (cond = 0; cond < COND_STACK_SIZE; cond++)
	{
		if (!cond_stack[cond].active)
		{
			cond_count++;
			cond_stack[cond].cond_type = cond_type;
			cond_stack[cond].data = data;
			cond_stack[cond].active = TRUE;
			break;
		}
	}
	if (cond == COND_STACK_SIZE)
		print("Error: no more room for conditions.\n");
}

/**
debug_clear():
Clears a certain condition to break.
Created 102206; last modified 051507
**/
void debug_clear(int cond)
{
	if (cond_stack[cond].active)
	{
		cond_stack[cond].active = FALSE;
		cond_count--;
	}
}

/**
debug_clear_all():
Clears all conditions to break.
Created 102206; last modified 051407
**/
void debug_clear_all(void)
{
	int cond;

	for (cond = 0; cond < COND_STACK_SIZE; cond++)
		debug_clear(cond);
}

/**
debug_cond_print():
Prints out all set conditions.
Created 102206; last modified 051407
**/
void debug_cond_print(void)
{
	int cond;
	char *cond_str[] = { "Always", "Hashkey = ", "Ply = ", "Move = ",
	   	"Score > ", "Score < " };

	print("Condition mode: %s\n", cond_mode == DEBUG_COND_OR ? "OR" : "AND");
	for (cond = 0; cond < COND_STACK_SIZE; cond++)
	{
		if (cond_stack[cond].active)
		{
			print("cond %02i: %s", cond, cond_str[cond_stack[cond].cond_type]);
			if (cond_stack[cond].cond_type != DEBUG_ALWAYS) 
				print("%L", cond_stack[cond].data);
			print("\n");
		}
	}
}

/**
debug_print():
Prints information about the current position being searched.
Created 051507; last modified 052408
**/
void debug_print(SEARCH_BLOCK *search_block)
{
	/* If there is a move made, go back to the main position of the node
		temporarily. */
	if (search_block->move_made)
		unmake_move();
	print("%Bsearch_state=%R %E\n", &board, search_block->search_state,
		   	search_block);
	//print("search_state=%R %E\n",search_block->search_state, search_block);
	if (search_block->move_made)
	{
		print("move=%M return=%V\n", search_block->move,
			-(search_block + 1)->return_value);
		make_move(search_block->move);
	}
}

#endif /* DEBUG_SEARCH */
