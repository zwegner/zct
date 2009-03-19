/** ZCT/cmduci.c--Created 101607 **/

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

COMMAND uci_commands[] =
{
	{ 0, "debug", NULL, 0, cmd_null },
	{ 0, "go", NULL, 0, cmd_uci_go },
	{ 0, "isready", NULL, 0, cmd_isready },
	{ 0, "ponderhit", NULL, 0, cmd_ponderhit },
	{ 0, "position", NULL, 1, cmd_position },
	{ 0, "quit", NULL, 1, cmd_exit },
	{ 0, "setoption", NULL, 0, cmd_setoption },
	{ 0, "stop", NULL, 1, cmd_uci_stop },
	{ 0, "ucinewgame", NULL, 1, cmd_new },
	{ 0, NULL, NULL, 0, NULL }
};

/**
cmd_isready():
The "isready" command implements synchronization between engine and GUI.
Created 101607; last modified 101607
**/
void cmd_isready(void)
{
	zct->ping = 1;
}


/**
cmd_ponderhit():
The "ponderhit" command tells ZCT that it is OK to move now, when in ponder search.
Created 101607; last modified 010908
**/
void cmd_ponderhit(void)
{
	zct->engine_state = NORMAL;
}

/**
cmd_position():
The "position" command sets ZCT to a certain position, possibly after some moves.
Created 101607; last modified 051708
**/
void cmd_position(void)
{
	char fen[256];
	int c;

	for (c = 1; c < cmd_input.arg_count; c++)
	{
		if (!strcmp(cmd_input.arg[c], "startpos"))
			initialize_board(NULL);
		else if (!strcmp(cmd_input.arg[c], "fen"))
		{
			strcpy(fen, "");
			c++;
			while (c < cmd_input.arg_count && strcmp(cmd_input.arg[c], "moves") != 0)
			{
				strcat(fen, cmd_input.arg[c]);
				strcat(fen, " ");
				c++;
			}
			/* We haven't reached the end of the string, so we go back in order
				to re-parse the next token, which is "moves". */
			if (c < cmd_input.arg_count)
				c--;
			initialize_board(fen);
		}
		else if (!strcmp(cmd_input.arg[c], "moves"))
		{
			c++;
			while (c < cmd_input.arg_count)
			{
				if (input_move(cmd_input.arg[c], INPUT_CHECK_MOVE))
					input_move(cmd_input.arg[c], INPUT_USER_MOVE);
				else
					break;
				c++;
			}
		}
	}
	zct->zct_side = EMPTY;
}

/**
cmd_setoption():
The "setoption" command sets various parameters that ZCT may or may not have.
Created 101607; last modified 101607
**/
void cmd_setoption(void)
{
}

/**
cmd_stop():
The "stop" command makes ZCT stop as soon as possible from searching. The actual command does nothing,
but the command parameters 
Created 101607; last modified 101607
**/
void cmd_uci_stop(void)
{
	zct->engine_state = IDLE;
}

/**
cmd_go():
The "go" command makes ZCT start searching. All parameters of searching are within this one command.
Created 101607; last modified 050108
**/
void cmd_uci_go(void)
{
	int c;

	zct->zct_side = board.side_tm;
	set_time_control(0, 0, 0);
	for (c = 1; c < cmd_input.arg_count; c++)
	{
		/* "moves" sets ZCT to only search certain root moves. */
		if (!strcmp(cmd_input.arg[c], "moves"))
		{
			/* Should this be implemented? */
		}
		else if (!strcmp(cmd_input.arg[c], "wtime"))
		{
			c++;
			if (board.side_tm == WHITE)
				set_zct_clock(atoi(cmd_input.arg[c]));
			else
				set_opponent_clock(atoi(cmd_input.arg[c]));
		}
		else if (!strcmp(cmd_input.arg[c], "btime"))
		{
			c++;
			if (board.side_tm == BLACK)
				set_zct_clock(atoi(cmd_input.arg[c]));
			else
				set_opponent_clock(atoi(cmd_input.arg[c]));
		}
		else if (!strcmp(cmd_input.arg[c], "depth"))
		{
			c++;
			zct->max_depth = atoi(cmd_input.arg[c]);
		}
		else if (!strcmp(cmd_input.arg[c], "nodes"))
		{
			c++;
			zct->max_nodes = atoi(cmd_input.arg[c]);
		}
		else if (!strcmp(cmd_input.arg[c], "infinite"))
			zct->engine_state = INFINITE;
		else if (!strcmp(cmd_input.arg[c], "ponder"))
			zct->engine_state = INFINITE;
	}
}
