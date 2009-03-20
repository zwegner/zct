/** ZCT/hash.c--Created 081706 **/

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

//#define USE_SPDH
//#define PV_HASH_CUTOFFS

/**
hash_probe():
Probes the hash table for the current position. Returns a move and score, if they are found.
Created 081706; last modified 051208
**/
BOOL hash_probe(SEARCH_BLOCK *sb, BOOL is_qsearch)
{
	int x;
	HASH_BOUND_TYPE type;
	HASH_ENTRY *entry;
	VALUE value;
	HASHKEY hashkey;
	BITBOARD data;

	hashkey = HASH_NON_PATH(board.hashkey);
	if (is_qsearch)
	{
		entry = &qsearch_hash_table[hashkey % zct->qsearch_hash_size];
		zct->qsearch_hash_probes++;
	}
	else
	{
		entry = &zct->hash_table[hashkey % zct->hash_size];
		zct->hash_probes++;
	}
	for (x = 0; x < HASH_SLOT_COUNT; x++)
	{
		hashkey = entry->entry[x].hashkey;
		data = entry->entry[x].data;
#ifdef USE_SPDH
		if (HASH_NON_PATH(hashkey ^ data) == HASH_NON_PATH(board.hashkey))
#else
		if ((hashkey ^ data) == board.hashkey)
#endif
		{
			if (is_qsearch)
				zct->qsearch_hash_hits++;
			else
				zct->hash_hits++;
			sb->hash_move = HASH_MOVE(data);
#ifndef PV_HASH_CUTOFFS
			if (sb->node_type != NODE_PV)
#endif
#ifdef USE_SPDH
			/* Only allow cutoffs in the case of the paths being the same. */
			if (HASH_DEPTH(data) >= sb->depth && HASH_PATH(hashkey ^ data) == HASH_PATH(board.path_hashkey))
#else
			if (HASH_DEPTH(data) >= sb->depth)
#endif
			{
				value = HASH_VALUE(data);
				/* Adjust the score for mates and bitbase hits. */
				if (value >= MATE - MAX_PLY)
					value -= sb->ply;
				else if (value <= -MATE + MAX_PLY)
					value += sb->ply;
				/* Check for cutoffs. */
				type = HASH_TYPE(data);
				if (type == HASH_EXACT_BOUND)
				{
					if (!is_qsearch)
						zct->hash_cutoffs++;
					sb->alpha = value;
					return TRUE;
				}
				if (value < sb->beta && type == HASH_UPPER_BOUND)
					sb->beta = value + 1;
				if (value > sb->alpha && type == HASH_LOWER_BOUND)
					sb->alpha = value;
				if (sb->alpha >= sb->beta)
				{
					if (!is_qsearch)
						zct->hash_cutoffs++;
					return TRUE;
				}
			}
			break;
		}
	}
	return FALSE;
}

/**
hash_store():
Stores the given information about the position in the hash table.
Created 081706; last modified 060808
**/
void hash_store(SEARCH_BLOCK *sb, MOVE move, VALUE value, HASH_BOUND_TYPE type, BOOL is_qsearch)
{
	int x;
	int best_slot;
	int best_depth;
	BITBOARD data;
	BITBOARD hashkey;
	HASH_ENTRY *entry;

	if (value >= MATE - MAX_PLY)
		value += sb->ply;
	else if (value <= -MATE + MAX_PLY)
		value -= sb->ply;

	hashkey = HASH_NON_PATH(board.hashkey);
	if (is_qsearch)
		entry = &qsearch_hash_table[hashkey % zct->qsearch_hash_size];
	else
		entry = &zct->hash_table[hashkey % zct->hash_size];
	/* Now look through the slots to find the best slot to store in. */
	best_slot = 0;
	best_depth = MAX_PLY * PLY + 1024;
	for (x = 0; x < HASH_SLOT_COUNT; x++)
	{
		hashkey = entry->entry[x].hashkey;
		data = entry->entry[x].data;
#ifdef USE_SPDH
		if (HASH_NON_PATH(hashkey ^ data) == HASH_NON_PATH(board.hashkey))
#else
		if ((hashkey ^ data) == board.hashkey)
#endif
		{
			if (HASH_DEPTH(data) > sb->depth)
				return;
			else
			{
				best_slot = x;
				break;
			}
		}
		else if (HASH_DEPTH(data) - age_difference(HASH_AGE(data)) * 2 * PLY < best_depth)
		{
			best_slot = x;
			best_depth = HASH_DEPTH(data) - age_difference(HASH_AGE(data)) * 2 * PLY;
		}
	}
	/* Check to see if we should overwrite the entry. */
	if (HASH_TYPE(entry->entry[best_slot].data) != HASH_EXACT_BOUND || (type == HASH_EXACT_BOUND/* &&
		sb->depth >= HASH_DEPTH(entry->entry[best_slot].data)*/) ||
		age_difference(HASH_AGE(entry->entry[best_slot].data)) > 1)
	{
		/* Update the hash fill statistic. */
		if (!is_qsearch && entry->entry[best_slot].data == 0 && entry->entry[best_slot].hashkey == 0)
			zct->hash_entries_full++;
		/* Store the data in the hash table. */
	//	if (move == NO_MOVE)
	//		move = HASH_MOVE(entry->entry[best_slot].data);
		data = SET_HASH_DEPTH(sb->depth) | SET_HASH_AGE(zct->search_age) | SET_HASH_TYPE(type) |
			SET_HASH_THREAT(sb->threat) | SET_HASH_VALUE(value) | SET_HASH_MOVE(move);
#ifdef USE_SPDH
		hashkey = (HASH_NON_PATH(board.hashkey) | HASH_PATH(board.path_hashkey)) ^ data;
#else
		hashkey = board.hashkey ^ data;
#endif
		entry->entry[best_slot].data = data;
		entry->entry[best_slot].hashkey = hashkey;
	}
}

/**
age_difference():
Computes the relative age of a hash entry compared to the current search. This is necessary because of wrap-arounds.
Created 060507; last modified 060807
**/
int age_difference(int age)
{
	if (age > zct->search_age)
		age -= MAX_SEARCH_AGE;
	return zct->search_age - age;
}

/**
hash_clear():
Clears the hash table of all entries, as well as move ordering data.
Created 053107; last modified 011808
**/
void hash_clear(void)
{
	BITBOARD entry;
	int x;
	int y;
	BITBOARD hashkey;
	BITBOARD data;
	COLOR c;

	zct->hash_entries_full = 0;
	hashkey = (HASHKEY)0;
	data = SET_HASH_DEPTH(0) | SET_HASH_AGE(0) | SET_HASH_TYPE(HASH_NO_BOUND) |
		SET_HASH_THREAT(0) | SET_HASH_VALUE(0) | SET_HASH_MOVE(NO_MOVE);
	/* main hash table */
	for (entry = 0; entry < zct->hash_size; entry++)
	{
		for (x = 0; x < HASH_SLOT_COUNT; x++)
		{
			zct->hash_table[entry].entry[x].hashkey = hashkey;
			zct->hash_table[entry].entry[x].data = data;
		}
	}
	/* qsearch hash table */
	for (entry = 0; entry < zct->qsearch_hash_size; entry++)
	{
		for (x = 0; x < HASH_SLOT_COUNT; x++)
		{
			qsearch_hash_table[entry].entry[x].hashkey = hashkey;
			qsearch_hash_table[entry].entry[x].data = data;
		}
	}
	/* history tables */
	zct->history_counter = 1;
	for (c = WHITE; c <= BLACK; c++)
		for (x = 0; x < 4096; x++)
			zct->history_table[c][x] = 0;
	/* killer tables */
	for (x = 0; x < MAX_PLY; x++)
		for (y = 0; y < 2; y++)
			zct->killer_move[x][y] = NO_MOVE;
	/* counter-move tables */
	for (c = WHITE; c <= BLACK; c++)
		for (x = 0; x < 4096; x++)
			for (y = 0; y < 2; y++)
				zct->counter_move[c][x][y] = NO_MOVE;
}

/**
hash_print():
Probes the hash table for the current position, and displays any information found in it.
Created 051507; last modified 051208
**/
void hash_print(void)
{
	int x;
	int depth;
	int age;
	BOOL threat;
	HASH_BOUND_TYPE type;
	HASH_ENTRY *entry;
	MOVE move;
	VALUE value;
	HASHKEY hashkey;
	BITBOARD data;

	entry = &zct->hash_table[board.hashkey % zct->hash_size];
	zct->hash_probes++;
	for (x = 0; x < HASH_SLOT_COUNT; x++)
	{
		hashkey = entry->entry[x].hashkey;
		data = entry->entry[x].data;
#ifdef USE_SPDH
		if (HASH_NON_PATH(hashkey ^ data) == HASH_NON_PATH(board.hashkey))
#else
		if ((hashkey ^ data) == board.hashkey)
#endif
		{
			depth = HASH_DEPTH(entry->entry[x].data);
			age = HASH_AGE(entry->entry[x].data);
			type = HASH_TYPE(entry->entry[x].data);
			threat = HASH_THREAT(entry->entry[x].data);
			value = HASH_VALUE(entry->entry[x].data);
			move = HASH_MOVE(entry->entry[x].data);
			print("hash hit: move=%M depth=%i age=%i type=%i threat=%i value=%V\n",
				move, depth, age, type, threat, value);
			return;
		}
	}
	print("hash miss.\n");
}
