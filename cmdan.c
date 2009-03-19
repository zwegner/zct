/** ZCT/cmdan.c--Created 102406 **/

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

COMMAND an_commands[] =
{
	{ 0, ".", NULL, 0, cmd_stats },
	{ 0, "bk", NULL, 0, cmd_null },
	{ 0, "display", NULL, 0, cmd_display },
	{ 0, "exit", NULL, 1, cmd_analyze_exit },
	{ 0, "hint", NULL, 0, cmd_null },
	{ 0, "new", NULL, 1, cmd_new },
	{ 0, "setboard", NULL, 1, cmd_setboard },
	{ 0, "undo", NULL, 1, cmd_undo },
	{ 0, NULL, NULL, 0, NULL }
};

/**
cmd_stats():
The "." command causes ZCT to print current search info during analysis.
Created 102406; last modified 061207
**/
void cmd_stats(void)
{
	sum_counters();
	print("stat01: %i %L %i %i %i %M\n", time_used() / 10,
		zct->nodes + zct->q_nodes, zct->current_iteration,
		zct->root_move_count - (zct->next_root_move - zct->root_move_list) + 1,
		zct->root_move_count, (zct->next_root_move - 1)->move);
}

/**
cmd_analyze_exit():
The "exit" command causes ZCT to exit analysis mode.
Created 102406; last modified 102406
**/
void cmd_analyze_exit(void)
{
	zct->engine_state = IDLE;
	if (zct->protocol == ANALYSIS_X)
		zct->protocol = XBOARD;
	else
		zct->protocol = DEFAULT;
	initialize_cmds();
}
