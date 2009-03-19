/** ZCT/perft.c--Created 082606 **/

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
#include "smp.h"

//#define USE_PERFT_HASHING

/**
perft():
Do a perft tree.
Created 070705; last modified 082806
**/
BITBOARD perft(int depth, MOVE *first_move)
{
	BITBOARD r;
	BITBOARD checks;
	MOVE *move;
	MOVE *last_move;

#ifdef USE_PERFT_HASHING
	if (r = perft_hash_lookup(depth))
		return r;
#endif

/*	checks = check_squares();
	if (checks)
		last_move = generate_evasions(first_move, checks);
	else
*/		last_move = generate_legal_moves(first_move);
	if (depth == 1)
		return last_move - first_move;
//		last_move = generate_moves(first_move);
	r = 0;
	for (move = first_move; move < last_move; move++)
	{
	/*	if (make_move(*move))
		{
			if (depth <= 1)
				r++;
			else
				r += perft(depth - 1, last_move);
			unmake_move();
		}*/
		make_move(*move);
		r += perft(depth - 1, last_move);
		unmake_move();
	}
#ifdef USE_PERFT_HASHING
	perft_hash_store(depth, r);
#endif
	return r;
}

/**
perft_hash_lookup():
Probe the hash table for a subtree value with the same depth that might have been computed before.
Created 100406; last modified 081607
**/
BITBOARD perft_hash_lookup(int depth)
{
	int x;
	HASH_ENTRY *entry;

	entry = &zct->hash_table[board.hashkey % zct->hash_size];
	for (x = 0; x < HASH_SLOT_COUNT; x++)
	{
		if (entry->entry[x].hashkey == board.hashkey && (entry->entry[x].data & 63) == depth)
			return entry->entry[x].data >> 6;
	}
	return 0;
}

/**
perft_hash_store():
Store the results of a perft search into the hash table. This stores a hashkey, depth counter and
64 bit perft subtree count.
Created 100406; last modified 081607
**/
void perft_hash_store(int depth, BITBOARD nodes)
{
	int x;
	HASH_ENTRY *entry;
	int best_entry;
	int best_depth;

	best_entry = 0;
	best_depth = 64;
	entry = &zct->hash_table[board.hashkey % zct->hash_size];
	for (x = 0; x < HASH_SLOT_COUNT; x++)
	{
		if ((entry->entry[x].data & 63) < best_depth)
		{
			best_entry = x;
			best_depth = entry->entry[x].data & 63;
		}
	}
	entry->entry[best_entry].data = depth | (nodes << 6);
	entry->entry[best_entry].hashkey = board.hashkey;
}
