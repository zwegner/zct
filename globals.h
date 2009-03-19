/** ZCT/globals.h--Created 070105 **/

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

#ifndef GLOBALS_H
#define GLOBALS_H

/* board representation */
extern THREAD_LOCAL BOARD board;
#if !defined(ZCT_WINDOWS) && defined(SMP) 
extern GLOBALS *zct;
#else
extern GLOBALS zct[1];
#endif

/* various constant board data */
extern const int pawn_step[2];
extern const CASTLE_RIGHTS castle_rights_mask[64];
extern BITBOARD mask_bb[64];
extern BITBOARD pawn_caps_bb[2][64];
extern BITBOARD knight_moves_bb[64];
extern BITBOARD king_moves_bb[64];
extern BITBOARD in_between_bb[64][64];
extern BITBOARD castle_ks_mask[2];
extern BITBOARD castle_qs_mask[2];
extern HASHKEY zobrist_piece[2][6][64];
extern HASHKEY zobrist_castle[16];
extern HASHKEY zobrist_ep[65];
extern HASHKEY zobrist_stm;
extern HASHKEY zobrist_path_move[4096][HASH_PATH_MOVE_COUNT];

/* other constants */
extern const char color_str[2][6];
extern const char piece_char[2][7];
extern const char piece_str[6][7];
extern const char result_str[][64];

/* These are process-local on non-Windows, thread-local on Windows... */
extern THREAD_LOCAL HASH_ENTRY *qsearch_hash_table;
extern THREAD_LOCAL PAWN_HASH_ENTRY *pawn_hash_table;
extern THREAD_LOCAL EVAL_HASH_ENTRY *eval_hash_table;

extern unsigned int sb_id;
extern THREAD_LOCAL GAME_ENTRY *root_entry;

#endif /* GLOBALS_H */
