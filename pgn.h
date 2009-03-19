/** ZCT/pgn.h--Created 091407 **/

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

#ifndef	PGN_H
#define PGN_H

#define STR_SIZE		(128)

/* PGN definitions */
typedef struct
{
	long offset;
	int move_count;
	struct
	{
		char event[STR_SIZE];
		char site[STR_SIZE];
		char date[STR_SIZE];
		char round[STR_SIZE];
		char white[STR_SIZE];
		char black[STR_SIZE];
		char result[STR_SIZE];
		char fen[STR_SIZE];
	} tag;
} PGN_GAME;

typedef struct
{
	int game_count;
	PGN_GAME *game;
} PGN_DATABASE;

/* A state for the PGN-parsing finite state machine. */
typedef enum { NEUTRAL, HEADERS, MOVES } PGN_STATE;

/* EPD definitions */
typedef enum { EPD_BEST_MOVE, EPD_AVOID_MOVE } EPD_TYPE;

typedef struct
{
	char id[STR_SIZE];
	char fen[STR_SIZE];
	int solution_count;
	MOVE solution[256];
	EPD_TYPE type;
} EPD_POS;

/* Some definitions for generic position processing (PGN or EPD) */
typedef enum { POS_EPD, POS_PGN } POS_TYPE;

typedef struct
{
	POS_TYPE type;
	char flags[8];
	union
	{
		PGN_GAME *pgn;
		EPD_POS *epd;
	};
} POS_DATA;

typedef BOOL (*POS_FUNC)(void *arg, POS_DATA *pos);

/* Prototypes */
void pgn_load(int game_number, POS_FUNC pos_func, void *pos_arg);
int epd_load(char *filename, POS_FUNC pos_func, void *pos_arg);

#endif /* PGN_H */
