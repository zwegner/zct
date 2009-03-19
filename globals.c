/** ZCT/globals.c--Created 070105 **/

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

/* board representation */
THREAD_LOCAL BOARD board;
#if !defined(ZCT_WINDOWS) && defined(SMP) 
GLOBALS *zct;
#else
GLOBALS zct[1];
#endif

/* various constant board data */
const int pawn_step[2] = { 8, -8 };
const CASTLE_RIGHTS castle_rights_mask[64] = /* which castling flags each square affects */
{
	~CASTLE_WQ, ~CASTLE_NO, ~CASTLE_NO, ~CASTLE_NO, ~CASTLE_WB, ~CASTLE_NO, ~CASTLE_NO, ~CASTLE_WK,
	~CASTLE_NO, ~CASTLE_NO, ~CASTLE_NO, ~CASTLE_NO, ~CASTLE_NO, ~CASTLE_NO, ~CASTLE_NO, ~CASTLE_NO,
	~CASTLE_NO, ~CASTLE_NO, ~CASTLE_NO, ~CASTLE_NO, ~CASTLE_NO, ~CASTLE_NO, ~CASTLE_NO, ~CASTLE_NO,
	~CASTLE_NO, ~CASTLE_NO, ~CASTLE_NO, ~CASTLE_NO, ~CASTLE_NO, ~CASTLE_NO, ~CASTLE_NO, ~CASTLE_NO,
	~CASTLE_NO, ~CASTLE_NO, ~CASTLE_NO, ~CASTLE_NO, ~CASTLE_NO, ~CASTLE_NO, ~CASTLE_NO, ~CASTLE_NO,
	~CASTLE_NO, ~CASTLE_NO, ~CASTLE_NO, ~CASTLE_NO, ~CASTLE_NO, ~CASTLE_NO, ~CASTLE_NO, ~CASTLE_NO,
	~CASTLE_NO, ~CASTLE_NO, ~CASTLE_NO, ~CASTLE_NO, ~CASTLE_NO, ~CASTLE_NO, ~CASTLE_NO, ~CASTLE_NO,
	~CASTLE_BQ, ~CASTLE_NO, ~CASTLE_NO, ~CASTLE_NO, ~CASTLE_BB, ~CASTLE_NO, ~CASTLE_NO, ~CASTLE_BK
};
BITBOARD mask_bb[64];
BITBOARD pawn_caps_bb[2][64];
BITBOARD knight_moves_bb[64];
BITBOARD king_moves_bb[64];
BITBOARD in_between_bb[64][64];
BITBOARD castle_ks_mask[2];
BITBOARD castle_qs_mask[2];
HASHKEY zobrist_piece[2][6][64];
HASHKEY zobrist_castle[16];
HASHKEY zobrist_ep[65];
HASHKEY zobrist_stm;
HASHKEY zobrist_path_move[4096][HASH_PATH_MOVE_COUNT];

/* other constants */
const char color_str[2][6] = { "white", "black" };
const char piece_char[2][7] = { "PNBRQK", "pnbrqk" };
const char piece_str[6][7] = { "pawn", "knight", "bishop", "rook", "queen", "king" };
const char result_str[][64] =
{
	"*", "1-0 {White mates}", "0-1 {Black mates}", "0-1 {White resigns}", "1-0 {Black resigns}",
	"1/2-1/2 {Stalemate}", "1/2-1/2 {Fifty move rule}", "1/2-1/2 {Three repetitions}",
	"1/2-1/2 {Draw by insufficient material", "1/2-1/2 {Draw by mutual agreement}"
};

/* These are process-local. */
THREAD_LOCAL HASH_ENTRY *qsearch_hash_table = NULL;
THREAD_LOCAL PAWN_HASH_ENTRY *pawn_hash_table = NULL;
THREAD_LOCAL EVAL_HASH_ENTRY *eval_hash_table = NULL;

unsigned int sb_id = 0;
THREAD_LOCAL GAME_ENTRY *root_entry;
