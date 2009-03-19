/** ZCT/pgn.c--Created 091407 **/

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
#include <ctype.h>

FILE *pgn_file = NULL;
PGN_GAME pgn_game;
PGN_DATABASE pgn_database;

/**
pgn_open():
pgn_open opens a .pgn file and reads all games into the internal database.
The number of games found is returned.
Created 091407; last modified 091607
**/
int pgn_open(char *file_name)
{
	char buffer[BUFSIZ];
	char *c;
	char *b;
	int games;
	long last_offset;
	PGN_GAME *game;
	PGN_STATE state;

	/* Try opening the file. */
	if (pgn_file != NULL)
		fclose(pgn_file);
	pgn_file = fopen(file_name, "rt");
	if (pgn_file == NULL)
	{
		print("%s: file not found.\n", file_name);
		return -1;
	}
	/* First, quickly find the number of games. */
	state = NEUTRAL;
	games = 0;
	while (fgets(buffer, BUFSIZ, pgn_file) != NULL)
	{
		switch (state)
		{
			case NEUTRAL:
				/* Stupid macros need an int cast... sigh */
				if (isspace((int)buffer[0]))
					continue;
				state = HEADERS;
			case HEADERS:
				if (buffer[0] == '[')
					continue;
				state = MOVES;
			case MOVES:
				if (strstr(buffer, "1-0") || strstr(buffer, "0-1") ||
					strstr(buffer, "1/2-1/2") || strstr(buffer, "*"))
				{
					games++;
					state = NEUTRAL;
				}
		}
	}
	/* Initialize the database. */
	if (pgn_database.game != NULL)
		free(pgn_database.game);
	pgn_database.game_count = games;
	pgn_database.game = calloc(games, sizeof(PGN_GAME));
	print("%i games loaded.\n", games);
	/* Read in all of the games. */
	state = NEUTRAL;
	rewind(pgn_file);
	last_offset = 0;
	game = &pgn_database.game[0];
	while (fgets(buffer, BUFSIZ, pgn_file))
	{
		switch (state)
		{
			case NEUTRAL:
				/* Stupid macros need an int cast... sigh */
				if (isspace((int)buffer[0]))
					continue;
				game->offset = last_offset;
				state = HEADERS;
			case HEADERS:
				if (buffer[0] == '[')
				{
					c = strtok(buffer, "\"");
					if (c == NULL)
						continue;
					c = strtok(buffer + strlen(c) + 1, "\"");
					if (c == NULL)
						continue;
					b = strtok(buffer + 1, " =\"");
					if (!strcmp(b, "Event"))
						strcpy(game->tag.event, c);
					if (!strcmp(b, "Site"))
						strcpy(game->tag.site, c);
					if (!strcmp(b, "Date"))
						strcpy(game->tag.date, c);
					if (!strcmp(b, "Round"))
						strcpy(game->tag.round, c);
					if (!strcmp(b, "White"))
						strcpy(game->tag.white, c);
					if (!strcmp(b, "Black"))
						strcpy(game->tag.black, c);
					if (!strcmp(b, "Result"))
						strcpy(game->tag.result, c);
					if (!strcmp(b, "FEN"))
					{
						strcpy(game->tag.fen, c);
						initialize_board(c);
					}
					continue;
				}
				state = MOVES;
			case MOVES:
				if (strstr(buffer, "1-0") || strstr(buffer, "0-1") ||
					strstr(buffer, "1/2-1/2") || strstr(buffer, "*"))
				{
					game++;
					state = NEUTRAL;
				}
		}
		last_offset = ftell(pgn_file);
	}
	return games;
}

/**
pgn_load():
pgn_load loads a specific game from the opened pgn database. The board
position is set to the last position in the game.
Created 091407; last modified 111208
**/
void pgn_load(int game_number, POS_FUNC pos_func, void *pos_arg)
{
	char buffer[BUFSIZ];
	char move_buf[8];
	char flag_buf[8];
	int consumed;
	int braces;
	int x;
	POS_DATA pos_data;
	PGN_GAME *game;
	PGN_STATE state;
	NOTATION old_notation;

	if (pgn_database.game_count == 0)
	{
		print("No database loaded.\n");
		return;
	}
	if (game_number < 1 || game_number > pgn_database.game_count)
	{
		print("Invalid game number: %i. Valid numbers are 1-%i\n",
			game_number, pgn_database.game_count);
		return;
	}
	/* Copy the PGN tags. */
	game = &pgn_database.game[game_number - 1];
	strcpy(pgn_game.tag.event, game->tag.event);
	strcpy(pgn_game.tag.site, game->tag.site);
	strcpy(pgn_game.tag.date, game->tag.date);
	strcpy(pgn_game.tag.round, game->tag.round);
	strcpy(pgn_game.tag.white, game->tag.white);
	strcpy(pgn_game.tag.black, game->tag.black);
	strcpy(pgn_game.tag.result, game->tag.result);
	strcpy(pgn_game.tag.fen, game->tag.fen);
	
	/* Initialize. */
	if (strcmp(pgn_game.tag.fen, "") != 0)
		initialize_board(pgn_game.tag.fen);
	else
		initialize_board(NULL);

	/* Set the engine to force mode. */
	zct->zct_side = EMPTY;
	old_notation = zct->notation;
	/* PGN uses SAN, of course... */
	zct->notation = SAN;

	braces = 0;
	state = NEUTRAL;
	pos_data.type = POS_PGN;
	pos_data.pgn = game;

	/* Find the game in the PGN file and start reading. */
	fseek(pgn_file, game->offset, SEEK_SET);
	while (fgets(buffer, sizeof(buffer), pgn_file))
	{
		switch (state)
		{
			case NEUTRAL:
				/* Stupid macros need an int cast... sigh */
				if (isspace((int)buffer[0]))
					continue;
				state = HEADERS;
			case HEADERS:
				if (buffer[0] == '[')
					continue;
				state = MOVES;
			case MOVES:
				/* Read the next line of input. Strip out all of the
					unneeded characters. */
				set_cmd_input(buffer);
				cmd_parse(" \t.+#\r\n");
				for (x = 0; x < cmd_input.arg_count; x++)
				{
					if (strchr(cmd_input.arg[x], '{'))
						braces++;
					 if (strchr(cmd_input.arg[x], '}'))
						braces--;
					else if (braces == 0)
					{
						/* Pull out any flags in the move string and separate
							them. */
						consumed = sscanf(cmd_input.arg[x], "%8[^?!=]%8[?!=]",
							move_buf, flag_buf);
						if (consumed == 2)
							strcpy(pos_data.flags, flag_buf);
						else
							strcpy(pos_data.flags, "");

						/* Check if we have a valid move. If so, make it
							and call pos_func. */
						if (input_move(move_buf, INPUT_CHECK_MOVE))
						{
							input_move(move_buf, INPUT_USER_MOVE);
							/* Now execute the pos_func, which does any
								processing for this move, for example the book
								making routine which collects statistics. */
							if (pos_func != NULL)
								if (pos_func(pos_arg, &pos_data))
									goto end;
						}
						/* Check for end-of-game. */
						else if (!strcmp(cmd_input.arg[x], "1-0") ||
							!strcmp(cmd_input.arg[x], "0-1") ||
							!strcmp(cmd_input.arg[x], "1/2-1/2") ||
							!strcmp(cmd_input.arg[x], "*"))
							goto end;
					}
				}
		}
	}
	/* Look at the spaghetti code! How unreadable! ;) */
end:
	zct->notation = old_notation;
}

/**
pgn_save():
pgn_save saves the current game into a pgn file. If the file already exists, it appends the game to the end.
Created 091407; last modified 091407
**/
void pgn_save(char *file_name)
{
	FILE *pgn_file;
	char text[80];
	GAME_ENTRY *current_entry;

	pgn_file = fopen(file_name, "at");
	if (pgn_file == NULL)
	{
		print("%s: file not found.\n");
		return;
	}
	/* Make sure we have the game result, then print the PGN tags. */
	check_result(FALSE);
	strcpy(pgn_game.tag.result, result_str[zct->game_result]);
	fprintf(pgn_file, "[Event \"%s\"]\n", pgn_game.tag.event);
	fprintf(pgn_file, "[Site \"%s\"]\n", pgn_game.tag.site);
	fprintf(pgn_file, "[Date \"%s\"]\n", pgn_game.tag.date);
	fprintf(pgn_file, "[Round \"%s\"]\n", pgn_game.tag.round);
	fprintf(pgn_file, "[White \"%s\"]\n", pgn_game.tag.white);
	fprintf(pgn_file, "[Black \"%s\"]\n", pgn_game.tag.black);
	fprintf(pgn_file, "[Result \"%s\"]\n", pgn_game.tag.result);
	if (strcmp(pgn_game.tag.fen, "") != 0)
		fprintf(pgn_file, "[FEN \"%s\"]\n", pgn_game.tag.fen);
	fprintf(pgn_file, "\n");
	/* Return to the beginning of the game. */
	current_entry = board.game_entry;
	while (board.game_entry > board.game_stack)
		unmake_move();

	/* Start with the proper move number indicator, depending on the side.
		We have to check that at least one move has been made before this. */
	if (board.game_entry < current_entry)
	{
		sprint(text, 80, "%i.%s%M ", board.move_number,
			board.side_tm == WHITE ? "" : "..", board.game_entry->move);
		make_move(board.game_entry->move);
	}
	/* Wade through the rest of the game. */
	while (board.game_entry < current_entry)
	{
		if (board.side_tm == WHITE)
			sprint(text, 80, "%s%i.", text, board.move_number);
		sprint(text, 80, "%s%M ", text, board.game_entry->move);
		make_move(board.game_entry->move);
		if (strlen(text) >= 70)
		{
			fprintf(pgn_file, "%s\n", text);
			strcpy(text, "");
		}
	}
	fprintf(pgn_file, "%s%s\n\n", text, result_str[zct->game_result]);
	fclose(pgn_file);
}
