/** ZCT/cmddbg.c--Created 102206 **/

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
#include "debug.h"

#ifdef DEBUG_SEARCH

COMMAND dbg_commands[] =
{
	{ 0, "break", "break into the debugger", 0, cmd_break },
	{ 0, "clear", "clear a break condition", 0, cmd_clear },
	{ 0, "continue", "continue searching until the next breakpoint", 0,
		cmd_continue },
	{ 0, "display", "display the current board position", 0, cmd_display },
	{ 0, "eval", "display the static evaluation for the current position", 0,
		cmd_eval },
	{ 0, "exit", "exit debug mode", 1, cmd_debug_exit },
	{ 0, "go", "set ZCT to move", 0, cmd_debug_go },
	{ 0, "hard", "turn pondering on", 0, cmd_hard },
	{ 0, "help", "display a list of commands", 0, cmd_help },
	{ 0, "moves", "display the available moves", 0, cmd_moves },
	{ 0, "new", "start a new game", 1, cmd_new },
	{ 0, "next", "step to the next break point", 0, cmd_next },
	{ 0, "sd", "set ZCT to think for a certain depth each move", 0, cmd_sd },
	{ 0, "setboard", "sets up a new FEN position", 1, cmd_setboard },
	{ 0, "setbreak", "set a new break condition", 0, cmd_setbreak },
	{ 0, "stop", "breaks the search", 1, cmd_stop },
	{ 0, "undo", "undoes a move", 1, cmd_undo },
	{ 0, "verify", "verify the internal board", 0, cmd_verify },
	{ 0, NULL, NULL, 0, NULL }
};

/**
cmd_break():
Steps into the debugger.
Created 051507; last modified 051507
**/
void cmd_break(void)
{
	debug_break();
}

/**
cmd_clear():
Clears a given break condition. Takes one integer argument.
Created 051507; last modified 051507
**/
void cmd_clear(void)
{
	int c;

	if (cmd_input.arg_count != 2)
	{
		print("Usage: clear cond_id\n");
		return;
	}
	c = atoi(cmd_input.arg[1]);
	if (c >= 1 && c <= COND_STACK_SIZE)
		debug_clear(c);
	else
		print("Condition id must be 1-%i\n", COND_STACK_SIZE);
}

/**
cmd_continue():
Makes the search continue until the next breakpoint.
Created 051507; last modified 051507
**/
void cmd_continue(void)
{
	debug_continue();
}

/**
cmd_debug_exit():
Exits into the default interface.
Created 102206; last modified 051407
**/
void cmd_debug_exit(void)
{
	zct->protocol = DEFAULT;
	initialize_cmds();
}

/**
cmd_debug_go():
Makes zct start searching in debug mode.
Created 051507; last modified 051507
**/
void cmd_debug_go(void)
{
	zct->engine_state = DEBUGGING;
}

/**
cmd_next():
Makes zct start searching in debug mode.
Created 051507; last modified 051507
**/
void cmd_next(void)
{
	debug_next();
}

/**
cmd_setbreak():
Sets a new break condition for debugging. This takes two arguments, one a break type (0-5),
and the other a value.
Created 051507; last modified 052508
**/
void cmd_setbreak(void)
{
	int c;
	BOOL good;
	GAME_ENTRY *start;
	BITBOARD data;

	if (cmd_input.arg_count >= 2)
	{
		if (!strcmp(cmd_input.arg[1], "always"))
			debug_set(DEBUG_ALWAYS, 0);
		if (!strcmp(cmd_input.arg[1], "ply"))
		{
			if (cmd_input.arg_count != 3)
				print("Usage: setbreak ply plynumber\n");
			data = atoi(cmd_input.arg[2]);
			if (data == 0 || data > MAX_PLY)
				print("Invalid ply number.\n");
			debug_set(DEBUG_PLY, data);
		}
		else if (!strcmp(cmd_input.arg[1], "moves"))
		{
			start = board.game_entry;
			good = TRUE;
			for (c = 2; c < cmd_input.arg_count; c++)
				if (!input_move(cmd_input.arg[c], INPUT_USER_MOVE))
				{
					good = FALSE;
					break;
				}
			if (good)
				debug_set(DEBUG_HASHKEY, board.hashkey);
			while (board.game_entry > start)
				unmake_move();
		}
		else if (!strcmp(cmd_input.arg[1], "move"))
		{
			if (cmd_input.arg_count != 3 && cmd_input.arg_count != 4)
				print("Usage: setbreak move move_to_break_on [ply]\n");
			if (input_move(cmd_input.arg[2], INPUT_CHECK_MOVE))
			{
				data = input_move(cmd_input.arg[2], INPUT_GET_MOVE);
				debug_set(DEBUG_MOVE, data);
			}
			else
				print("Invalid ply number.\n");
		}
	}
	/* Default condition: break on the current position */
	else
		debug_set(DEBUG_HASHKEY, board.hashkey);
	debug_cond_print();
}

/**
cmd_stop():
Makes zct stop for debugging at the current position.
Created 051507; last modified 051507
**/
void cmd_stop(void)
{
	zct->engine_state = IDLE;
}

#else

COMMAND dbg_commands[] =
{
	{ 0, NULL, NULL, 0, NULL }
};

#endif /* DEBUG_SEARCH */
