/** ZCT/epd.c--Created 111208 **/

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
#include "pgn.h"

/**
epd_load():
Opens an EPD file and does processing on each position based on the POS_FUNC.
This is usually used for running test suites.
Created 110506; last modified 111208
**/
int epd_load(char *file_name, POS_FUNC pos_func, void *pos_arg)
{
	FILE *epd_file;
	int x;
	int positions;
	char *s;
	char buffer[BUFSIZ];
	MOVE move;
	EPD_POS epd;
	POS_DATA pos_data;

	if ((epd_file = fopen(file_name, "rt")) == NULL)
	{
		perror(file_name);
		return 0;
	}
	positions = 0;
	pos_data.epd = &epd;

	while (fgets(buffer, sizeof(buffer), epd_file) != NULL)
	{
		set_cmd_input(buffer);
		cmd_parse(" \t\n");
		if (cmd_input.arg_count < 6)
		{
			print("Bad epd record at position %i: %s", positions, buffer);
			continue;
		}

		/* Grab the FEN portion of the EPD record and set up the board. */
		strcpy(epd.fen, "");
		for (x = 0; x < cmd_input.arg_count && x < 5 &&
			strcmp(cmd_input.arg[x], "id") != 0 &&
			strcmp(cmd_input.arg[x], "bm") != 0 &&
			strcmp(cmd_input.arg[x], "am") != 0; x++)
		{
			strcat(epd.fen, cmd_input.arg[x]);
			strcat(epd.fen, " ");
		}
		initialize_board(epd.fen);
		generate_root_moves();

		/* Parse the rest of the EPD record to find the other information:
			best move, avoid move, ID */
		epd.solution_count = 0;
		for (x = 0; x < cmd_input.arg_count; x++)
		{
			/* Best move or avoid move. */
			if (!strcmp(cmd_input.arg[x], "bm") ||
				!strcmp(cmd_input.arg[x], "am"))
			{
				epd.type = (cmd_input.arg[x][0] == 'b' ?
					EPD_BEST_MOVE : EPD_AVOID_MOVE); /* Kinda hacky */

				/* We could have multiple solutions for this EPD. Pull off each
					one, parse it, and att it to the list. */
				while (TRUE)
				{
					x++;
					/* Pull off a semicolon from the end. Save the pointer,
						because it marks the end of the loop. */
					if ((s = strchr(cmd_input.arg[x], ';')) != NULL)
						*s = '\0';

					/* Try parsing the move. */
					move = input_move(cmd_input.arg[x], INPUT_GET_MOVE);
					if (move == NO_MOVE)
						print("Illegal solution at position %i: %s\n",
							positions, cmd_input.arg[x]);
					else
						epd.solution[epd.solution_count++] = move;

					/* No more solutions... */
					if (s != NULL)
						break;
				}
			}
			if (!strcmp(cmd_input.arg[x], "id"))
				strcpy(epd.id, cmd_input.arg[++x]);
		}
		/* Sanity check. */
		if (epd.solution_count == 0)
;//			print("No solutions given at position %i!!!\n", positions);

		if (pos_func != NULL)
			if (pos_func(pos_arg, &pos_data))
				break;
		positions++;
	}
	fclose(epd_file);

	return positions;
}
