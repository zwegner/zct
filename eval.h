/** ZCT/eval.h--Created 080806 **/

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

#ifndef EVAL_H
#define EVAL_H

//#define EVAL_DEBUG

#ifdef EVAL_DEBUG
#define DEBUG_EVAL(x)		(x)
#else
#define DEBUG_EVAL(x)
#endif

typedef struct
{
	char *name;
	VALUE *value; /* pointer to real eval parameters */
	int dimensions; /* 0, 1, or 2 */
	int dimension[2];
} EVAL_PARAMETER;

/* basic evaluation terms */
extern VALUE piece_value[7];
extern VALUE piece_endgame_value[7];
extern VALUE piece_square_value[2][6][64];
extern VALUE development_value[5];
extern VALUE early_queen_value;
/* pawn terms */
extern VALUE passed_pawn_value[8];
extern VALUE connected_pp_value;
extern VALUE weak_pawn_value;
extern VALUE doubled_pawn_value;
/* bishop terms */
extern VALUE bishop_pair_value;
extern VALUE bishop_trapped_value;
/* rook terms */
extern VALUE rook_on_seventh_value[2];
extern VALUE rook_open_file_value[4];
/* mobility terms */
extern VALUE mobility_value[6][32];
extern VALUE safe_mobility_value[6][32];
/* king safety terms */
extern VALUE king_shelter_value[31];
extern VALUE king_safety_att_weight[6];
extern VALUE king_safety_att_value[41];
extern VALUE king_safety_block_weight[6];
extern VALUE king_safety_block_value[21];
extern VALUE king_safety_def_weight[6];
extern VALUE king_safety_def_percentage[41];
extern VALUE lost_castling_value[2];
extern VALUE trapped_rook_value;
/* endgame terms */
extern VALUE king_bishop_square_value[64];
extern VALUE king_endgame_square_value[64];
/* miscellaneous terms */
extern VALUE side_tm_value;
/* masks, data, etc. */
extern BITBOARD development_mask[2];
extern BITBOARD trapped_rook_mask[2][8];

/* This is basically the set of all evaluation terms. It is used for settings and autotuning,
	both of which are not yet implemented. */
extern EVAL_PARAMETER eval_parameter[];

/* Some evaluation data, to be used between the various evaluation files. */
extern BITBOARD attack_set[2][6];
extern BITBOARD good_squares[2];
extern PHASE phase;

/* Color-independent fill functions */
extern BITBOARD (*fill_forward[2])(BITBOARD g, BITBOARD p);
extern BITBOARD (*smear_forward[2])(BITBOARD g);

/* The least and the greatest files that relevant pawns are on for king-pawn
	shelters. */
static const SQ_FILE shelter_file[2][2] =
{
	{ FILE_F, FILE_H },
	{ FILE_A, FILE_C }
};

/* Prototypes. */

/* eval.c */
VALUE interpolate(VALUE opening, PHASE phase, PHASE target);
VALUE evaluate_knights(COLOR color);
VALUE evaluate_bishops(COLOR color);
VALUE evaluate_rooks(COLOR color);
VALUE evaluate_queens(COLOR color);
VALUE evaluate_mobility(BITBOARD attacks, PIECE piece, COLOR color,
	SQUARE square);
VALUE evaluate_endgame(COLOR color);
VALUE evaluate_king_safety(EVAL_BLOCK *eval_block, COLOR color);

/* evalinit.c */
void initialize_params(EVAL_PARAMETER *param);
void copy_params(EVAL_PARAMETER *from, EVAL_PARAMETER *to);
void free_params(EVAL_PARAMETER *param);
void print_params(EVAL_PARAMETER *param);

/* evalpawns.c */
VALUE evaluate_pawns(void);
VALUE evaluate_passed_pawns(EVAL_BLOCK *eval_block, COLOR color);

#endif /* EVAL_H */
