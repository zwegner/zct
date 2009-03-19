/** ZCT/cmdxb.c--Created 081206 **/

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

COMMAND xb_commands[] =
{
	{ 0, ".", NULL, 0, cmd_stats },
	{ 0, "?", NULL, 1, cmd_null },
	{ 0, "accepted", NULL, 0, cmd_null },
	{ 0, "analyze", NULL, 1, cmd_analyze_x },
	{ 0, "easy", NULL, 2, cmd_easy },
	{ 0, "computer", NULL, 0, cmd_computer },
	{ 0, "force", NULL, 1, cmd_force },
	{ 0, "go", NULL, 2, cmd_go },
	{ 0, "hard", NULL, 0, cmd_hard },
	{ 0, "ics", NULL, 0, cmd_ics },
	{ 0, "level", NULL, 0, cmd_level },
	{ 0, "new", NULL, 1, cmd_new },
	{ 0, "nopost", NULL, 0, cmd_nopost },
	{ 0, "otim", NULL, 0, cmd_otim },
	{ 0, "ping", NULL, 0, cmd_ping },
	{ 0, "post", NULL, 0, cmd_post },
	{ 0, "protover", NULL, 0, cmd_protover },
	{ 0, "quit", NULL, 0, cmd_exit },
	{ 0, "random", NULL, 0, cmd_null },
	{ 0, "rating", NULL, 0, cmd_rating },
	{ 0, "rejected", NULL, 0, cmd_null },
	{ 0, "result", NULL, 1, cmd_result },
	{ 0, "sd", NULL, 0, cmd_sd },
	{ 0, "setboard", NULL, 1, cmd_setboard },
	{ 0, "st", NULL, 0, cmd_st },
	{ 0, "time", NULL, 0, cmd_time },
	{ 0, "undo", NULL, 1, cmd_undo },
	{ 0, NULL, NULL, 0, NULL }
};

/**
cmd_null():
Handles various commands that are treated as no-ops.
Created 102206; last modified 102206
**/
void cmd_null(void)
{
}

/**
cmd_analyze_x():
This is equivalent to the cmd_analyze function in cmddef.c, but analysis in XBoard is slightly different.
Created 102406; last modified 102806
**/
void cmd_analyze_x(void)
{
	zct->protocol = ANALYSIS_X;
	zct->zct_side = EMPTY;
	initialize_cmds();
}

/**
cmd_computer():
Tells ZCT that it is playing against a computer.
Created 090206; last modified 090206
**/
void cmd_computer(void)
{
	zct->computer = TRUE;
}

/**
cmd_easy():
Turn pondering off.
Created 090206; last modified 090206
**/
void cmd_easy(void)
{
	zct->hard = FALSE;
}

/**
cmd_force():
Set ZCT to be in "observing" mode: accept moves, but don't search.
Created 070105; last modified 070105
**/
void cmd_force(void)
{
	zct->zct_side = EMPTY;
}

/**
cmd_go():
Makes ZCT search and make a move in the current position.
Created 082406; last modified 082406
**/
void cmd_go(void)
{
	zct->zct_side = board.side_tm;
}

/**
cmd_hard():
Turn pondering on.
Created 090206; last modified 090206
**/
void cmd_hard(void)
{
	zct->hard = TRUE;
}

/**
cmd_level():
Set the time control being used.
Created 090206; last modified 090206
**/
void cmd_level(void)
{
	int moves;
	int base;
	int inc;

	if (cmd_input.arg_count != 4)
	{
		print("Usage: level moves base increment\n");
		return;
	}
	moves = atoi(cmd_input.arg[1]);
	base = atoi(cmd_input.arg[2]) * 60000;
	if (strchr(cmd_input.arg[2], ':'))
		base += atoi(strchr(cmd_input.arg[2], ':')) * 1000;
	inc = atoi(cmd_input.arg[3]) * 1000;
	set_time_control(moves, base, inc);
}

/**
cmd_ics():
Tells ZCT that we are playing on an Internet Chess Server.
Created 011308; last modified 011308
**/
void cmd_ics(void)
{
	if (cmd_input.arg_count != 2)
	{
		print("Usage: ics ics_host\n");
		return;
	}
	if (strcmp(cmd_input.arg[1], "-") != 0)
	{
		zct->ics_mode = TRUE;
		print("tellall Hello! This is %s%i playing (a computer program).\n",
			ZCT_VERSION_STR, ZCT_VERSION);
	}
}

/**
cmd_nopost():
Turn search display off.
Created 090206; last modified 090206
**/
void cmd_nopost(void)
{
	zct->post = FALSE;
}

/**
cmd_otim():
Set the amount of time on the opponent's clock.
Created 090206; last modified 090206
**/
void cmd_otim(void)
{
	if (cmd_input.arg_count != 2)
	{
		print("Usage: otim time_in_centiseconds\n");
		return;
	}
	set_opponent_clock(atoi(cmd_input.arg[1]) * 10);
}

/**
cmd_ping():
The "ping" command allows the GUI and ZCT to synchronize.
Created 102906; last modified 102906
**/
void cmd_ping(void)
{
	if (cmd_input.arg_count != 2)
	{
		print("Usage: ping ping_number\n");
		return;
	}
	zct->ping = atoi(cmd_input.arg[1]);
	if (zct->engine_state == PONDERING)
	{
		print("pong %i\n", zct->ping);
		zct->ping = 0;
	}
}

/**
cmd_post():
Turn search display on.
Created 090206; last modified 090206
**/
void cmd_post(void)
{
	zct->post = TRUE;
}

/**
cmd_protover():
Tell ZCT the current XBoard protocol version.
Created 090206; last modified 090206
**/
void cmd_protover(void)
{
	if (cmd_input.arg_count != 2)
	{
		print("Usage: protover protocol_version\n");
		return;
	}
	if (atoi(cmd_input.arg[1]) < 2)
		fatal_error("tellusererror Only protover > 2 supported.\n");

	print("feature ping=1\n");
	print("feature setboard=1\n");
	print("feature playother=0\n");
	if (zct->notation == SAN)
		print("feature san=1\n");
	else
		print("feature san=0\n");
	print("feature usermove=0\n");
	print("feature time=1\n");
	print("feature draw=0\n");
	print("feature sigint=0\n");
	print("feature sigterm=0\n");
	print("feature reuse=1\n");
	print("feature analyze=1\n");
	print("feature myname=\"%s%i\"\n", ZCT_VERSION_STR, ZCT_VERSION);
	print("feature colors=0\n");
	print("feature ics=1\n");
	print("feature name=0\n");
	print("feature pause=0\n");
	print("feature done=1\n");
}

/**
cmd_rating():
The "rating" command informs ZCT of its opponent's and its own ELO rating.
Created 022207; last modified 022207
**/
void cmd_rating(void)
{
}

/**
cmd_result():
Tells ZCT that the current game is over and what the result is.
Created 090206; last modified 090206
**/
void cmd_result(void)
{
	zct->game_over = TRUE;
}

/**
cmd_sd():
The "sd" command sets the time control at a fixed depth per move.
Created 103106; last modified 083108
**/
void cmd_sd(void)
{
	int depth;

	if (cmd_input.arg_count != 2)
	{
	 	print("Usage: sd depth\n");
		return;
	}
	depth = atoi(cmd_input.arg[1]);
	if (depth < 1 || depth >= MAX_PLY)
	{
		print("tellusererror Depth must be between 1 and %i.\n", MAX_PLY - 1);
		return;
	}
	zct->max_depth = depth;
}

/**
cmd_setboard():
Set the board position with a FEN.
Created 100306; last modified 100306
**/
void cmd_setboard(void)
{
	int arg;
	char fen[BUFSIZ];

	if (cmd_input.arg_count < 3 || cmd_input.arg_count > 7)
	{
		/* Do we really need to print all this out? */
		print("Usage: setboard board_position side_to_move [castle_status "
			"[ep_square [move_count [fifty_move_count]]]]\n");
		return;
	}

	/* This is ghetto as fuck: copy the delimited strings back into one. */
	strcpy(fen, "");
	for (arg = 1; arg < cmd_input.arg_count; arg++)
	{
		strcat(fen, cmd_input.arg[arg]);
		strcat(fen, " ");
	}
	/* Set up the position. */
	initialize_board(fen);
}

/**
cmd_st():
The "st" command sets the time control at a fixed amount for each move,
in seconds.
Created 103106; last modified 103106
**/
void cmd_st(void)
{
	int time;

	if (cmd_input.arg_count != 2)
	{
	 	print("Usage: st time\n");
		return;
	}
	time = atoi(cmd_input.arg[1]) * 1000;
	set_time_control(0, 0, time);
}

/**
cmd_time():
Set the amount of time on ZCT's clock.
Created 082506; last modified 082506
**/
void cmd_time(void)
{
	if (cmd_input.arg_count != 2)
	{
		print("Usage: time time_in_centiseconds\n");
		return;
	}
	set_zct_clock(atoi(cmd_input.arg[1]) * 10);
}

/**
cmd_undo():
Undoes a move.
Created 090206; last modified 010908
**/
void cmd_undo(void)
{
	unmake_move();
	zct->search_age--;
	if (zct->search_age < 0)
		zct->search_age = MAX_SEARCH_AGE;
	zct->zct_side = EMPTY;
	zct->game_over = FALSE;
	zct->game_result = INCOMPLETE;
}
