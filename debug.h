/** ZCT/debug.h--Created 102206 **/

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

#ifndef DEBUG_H
#define DEBUG_H

#ifdef DEBUG_SEARCH

typedef enum { DEBUG_ALWAYS, DEBUG_HASHKEY, DEBUG_PLY, DEBUG_MOVE, DEBUG_SCORE_GT, DEBUG_SCORE_LT } DEBUG_COND_TYPE;
typedef enum { DEBUG_COND_OR, DEBUG_COND_AND } DEBUG_COND_MODE;

typedef struct DEBUG_COND
{
	BOOL active;
	DEBUG_COND_TYPE cond_type;
	BITBOARD data;
} DEBUG_COND;

typedef struct
{
	BOOL set_break;
	BOOL stop;
	int ply;
	int depth;
	SEARCH_BLOCK *sb;
} DEBUG_STATE;

#define COND_STACK_SIZE			(32)

extern DEBUG_COND cond_stack[COND_STACK_SIZE];
extern int cond_count;
extern DEBUG_COND_MODE cond_mode;
extern DEBUG_STATE debug;

/* Functions */
void debug_continue(void);
void debug_next(void);
void debug_break(void);
void debug_step(SEARCH_BLOCK *search_block);
void debug_set(DEBUG_COND_TYPE cond_type, BITBOARD data);
void debug_clear(int cond);
void debug_clear_all(void);
void debug_cond_print(void);
void debug_print(SEARCH_BLOCK *search_block);

#endif /* DEBUG_SEARCH */

#endif /* DEBUG_H */
