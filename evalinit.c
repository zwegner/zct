/** ZCT/evalinit.c--Created 080806 **/

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
#include "eval.h"

VALUE piece_value[7] = { 100, 350, 350, 500, 1000, 10000, 0 };
VALUE piece_endgame_value[7] = { 100, 250, 275, 525, 950, 10000, 0 };

VALUE piece_square_value[2][6][64] =
{
	{
		{ /* white pawn */
			 0,  0,  0,  0,  0,  0,  0,  0,
			 10, 8,  6, -6, -6,  6,  8,  10,
			-16,-4, -8, -2, -2, -8, -4, -16,
			-10, 0,  0,  8,  8,  0,  0, -10,
			-8,  0,  0,  0,  0,  0,  0, -8,
			-6,  0,  0,  0,  0,  0,  0, -6,
			-5,  0,  0,  0,  0,  0,  0, -5,
			 0,  0,  0,  0,  0,  0,  0,  0
		},
		{ /* white knight */
			-6,-4,-2,-1,-1,-2,-4,-6,
			-4,-3, 0, 0, 0, 0,-3,-4,
			-2, 0, 4, 6, 6, 4, 0,-2,
			-1, 0, 6, 8, 8, 6, 0,-1,
			-1, 0, 6, 8, 8, 6, 0,-1,
			-2, 0, 4, 6, 6, 4, 0,-2,
			-4,-3, 0, 0, 0, 0,-3,-4,
			-6,-4,-2,-1,-1,-2,-4,-6
		},
		{ /* white bishop */
			 0,-2,-4,-7,-7,-4,-2, 0,
			-2, 2, 1,-2,-2, 1, 2,-2,
			-4, 1, 4, 0, 0, 4, 1,-3,
			-7,-2, 0, 8, 8, 0,-2,-7,
			-7,-2, 0, 8, 8, 0,-2,-7,
			-4, 1, 4, 0, 0, 4, 1,-3,
			-2, 2, 1,-2,-2, 1, 2,-2,
			 0,-2,-4,-7,-7,-4,-2, 0
		},
		{ /* white rook */
			 0, 0, 0, 0, 0, 0, 0, 0,
			 0, 0, 0, 0, 0, 0, 0, 0,
			 0, 0, 0, 0, 0, 0, 0, 0,
			 0, 0, 0, 0, 0, 0, 0, 0,
			 0, 0, 0, 0, 0, 0, 0, 0,
			 0, 0, 0, 0, 0, 0, 0, 0,
			 0, 0, 0, 0, 0, 0, 0, 0,
			 0, 0, 0, 0, 0, 0, 0, 0
		},
		{ /* white queen */
			 0, 0, 0, 0, 0, 0, 0, 0,
			 0, 0, 0, 0, 0, 0, 0, 0,
			 0, 0, 0, 0, 0, 0, 0, 0,
			 0, 0, 0, 0, 0, 0, 0, 0,
			 0, 0, 0, 0, 0, 0, 0, 0,
			 0, 0, 0, 0, 0, 0, 0, 0,
			 0, 0, 0, 0, 0, 0, 0, 0,
			 0, 0, 0, 0, 0, 0, 0, 0
		},
		{ /* white king */
			 2, 2, 0,-2,-2, 0, 2, 2,
			-4,-8,-8,-8,-8,-8,-8,-4,
			-8,-8,-8,-8,-8,-8,-8,-8,
			-8,-8,-8,-8,-8,-8,-8,-8,
			-8,-8,-8,-8,-8,-8,-8,-8,
			-8,-8,-8,-8,-8,-8,-8,-8,
			-8,-8,-8,-8,-8,-8,-8,-8,
			-8,-8,-8,-8,-8,-8,-8,-8
		}
	}
};
VALUE development_value[5] = { 15, 5, 0, -5, -15 };
VALUE early_queen_value = -30;

VALUE passed_pawn_value[8] = { 0, 200, 120, 100, 60, 40, 20, 0 };
VALUE connected_pp_value = 30;
VALUE weak_pawn_value = -20;
VALUE doubled_pawn_value = -25;

VALUE bishop_pair_value = 50;
VALUE bishop_trapped_value = -100;

VALUE rook_on_seventh_value[2] = { -10, 10 };
VALUE rook_open_file_value[4] = { -20, 0, -10, 10 };

VALUE mobility_value[6][32] =
{
	{ 0 },
	{ /* knight */
		-20, -15, -10, -6, -3, 0, 0, 4, 10
	},
	{ /* bishop */
		-15, -10, -7, -4, -2, -1, 0,
		2, 4, 6, 10, 12, 15, 15
	},
	{ /* rook */
		-18, -16, -13, -10, -8, -5, -2, 1,
		3, 7, 10, 14, 17, 20, 22
	},
	{ /* queen */
		-20, -20, -20, -19, -19, -18, -17, -15,
		-13, -11, -8, -5, -2, 0, 2, 4, 6, 8,
		10, 10, 10, 10, 10, 10, 10, 10, 10, 10
	},
	{ 0 }
};
VALUE safe_mobility_value[6][32] =
{
	{ 0 },
	{ /* knight */
		-40, -25, -15, -10, -5, 0, 3, 7, 10
	},
	{ /* bishop */
		-70, -40, -15, -12, -8, -4, 0,
		3, 6, 9, 12, 15, 18, 20
	},
	{ /* rook */
		-20, -16, -14, -10, -9, -6, -3, 0,
		3, 6, 9, 12, 16, 20, 22
	},
	{ /* queen */
		-30, -29, -27, -25, -23, -20, -16, -12,
		-9, -7, -5, -4, -3, -2, -1, 0, 2, 4,
		6, 8, 10, 12, 14, 15, 15, 15, 15, 15
	},
	{ 0 }
};

VALUE king_shelter_value[31] =
{
	30, 21, 13, 6, 1, -4, -8, -11, -13, -15,
	-17, -18, -19, -19, -20, -20, -20, -21, -21, -22,
	-23, -25, -27, -29, -32, -36, -41, -46, -53, -61, -70
};
VALUE king_safety_att_value[41] =
{
	100, 96, 92, 88, 82, 76, 70, 63, 56, 48,
	40, 30, 20, 12, 8, 6, 5, 4, 3, 2,
	1, 0, -2, -4, -7, -10, -14, -18, -24, -30,
	-38, -48, -60, -70, -82, -95, -110, -130, -160, -200, -250
};
VALUE king_safety_block_value[21] =
{
	40, 32, 25, 19, 14, 10, 7, 5, 3, 1, 0,
	-1, -3, -5, -7, -10, -14, -19, -25, -32, -40
};
VALUE king_safety_def_percentage[41] =
{ 
	150, 144, 138, 133, 129, 124, 121, 117,
	114, 112, 109, 107, 106, 104, 103, 102,
	101, 101, 100, 100, 100, 100, 100, 99,
	99, 98, 97, 96, 94, 93, 91, 88,
	86, 83, 79, 76, 71, 67, 62, 56, 50
};
int king_safety_att_weight[6] = { 1, 2, 2, 4, 8, 0 };
int king_safety_block_weight[2] = { 2, 5 };
int king_safety_def_weight[6] = { 4, 2, 2, 4, 7, 0 };
VALUE lost_castling_value[2] = { -20, -50 };
VALUE trapped_rook_value = -30;

VALUE king_bishop_square_value[64] =
{
	 40, 30, 20, 10,-10,-20,-30,-40,
	 30, 25, 15,  5, -5,-15,-25,-30,
	 20, 15,  5,  0,  0, -5,-15,-20,
	 10,  5,  0,  0,  0,  0, -5,-10,
	-10, -5,  0,  0,  0,  0,  5, 10,
	-20,-15, -5,  0,  0,  5, 15, 20,
	-30,-25,-15, -5,  5, 15, 25, 30,
	-40,-30,-20,-10, 10, 20, 30, 40,
};

VALUE king_endgame_square_value[64] =
{
	-20,-18,-16,-14,-14,-16,-18,-20,
	-18,-15,-10,-10,-10,-10,-15,-18,
	-16,-10,  0,  2,  2,  0,-10,-16,
	-14,-10,  2,  8,  8,  2,-10,-14,
	-14,-10,  2,  8,  8,  2,-10,-14,
	-16,-10,  0,  2,  2,  0,-10,-16,
	-18,-15,-10,-10,-10,-10,-15,-18,
	-20,-18,-16,-14,-14,-16,-18,-20
};

VALUE side_tm_value = 10; /* XXX test */

BITBOARD development_mask[2];
BITBOARD trapped_rook_mask[2][8];

EVAL_PARAMETER eval_parameter[] =
{
	{ (VALUE *)piece_value, 				1, { 7, 0 } },
	{ (VALUE *)piece_square_value[WHITE],	2, { 6, 64 } },
	{ (VALUE *)development_value,			1, { 5, 0 } },
	{ (VALUE *)&early_queen_value,			0, { 0, 0 } },
	{ (VALUE *)passed_pawn_value,			1, { 8, 0 } },
	{ (VALUE *)&connected_pp_value,			0, { 0, 0 } },
	{ (VALUE *)&weak_pawn_value,			0, { 0, 0 } },
	{ (VALUE *)&doubled_pawn_value,			0, { 0, 0 } },
	{ (VALUE *)&bishop_pair_value,			0, { 0, 0 } },
	{ (VALUE *)rook_on_seventh_value,		1, { 2, 0 } },
	{ (VALUE *)rook_open_file_value,		1, { 4, 0 } },
	{ (VALUE *)mobility_value,				2, { 6, 32 } },
	{ (VALUE *)safe_mobility_value,			2, { 6, 32 } },
	{ (VALUE *)king_shelter_value,			2, { 2, 41 } },
	{ (VALUE *)king_safety_att_value,		1, { 41, 0 } },
	{ (VALUE *)king_safety_block_value,		1, { 21, 0 } },
	{ (VALUE *)king_safety_def_percentage,	1, { 41, 0 } },
	{ (VALUE *)king_safety_att_weight,		1, { 6, 0 } },
	{ (VALUE *)king_safety_block_weight,	1, { 2, 0 } },
	{ (VALUE *)king_safety_def_weight,		1, { 6, 0 } },
	{ (VALUE *)lost_castling_value,			1, { 2, 0 } },
	{ (VALUE *)&trapped_rook_value,			0, { 0, 0 } },
	{ (VALUE *)king_bishop_square_value,	1, { 64, 0 } },
	{ (VALUE *)king_endgame_square_value,	1, { 64, 0 } },
	{ (VALUE *)&side_tm_value,				0, { 0, 0 } },
	{ NULL,									0, { 0, 0 } }
};

/**
initialize_eval():
Initializes all evaluation terms.
Created 080806; last modified 090707
**/
void initialize_eval(void)
{
	COLOR color;
	PIECE piece;
	SQUARE square;
	SQ_FILE file;
	SQ_RANK rank;

	for (piece = PAWN; piece < EMPTY; piece++)
	{
		for (square = A1; square < OFF_BOARD; square++)
			piece_square_value[BLACK][piece][square] = piece_square_value[WHITE][piece][SQ_FLIP(square)];
	}
	development_mask[WHITE] = MASK(B1) | MASK(C1) | MASK(F1) | MASK(G1);
	development_mask[BLACK] = MASK(B8) | MASK(C8) | MASK(F8) | MASK(G8);
	for (color = WHITE; color <= BLACK; color++)
	{
		rank = (color == WHITE ? RANK_1 : RANK_8);
		for (file = FILE_A; file <= FILE_C; file++)
			trapped_rook_mask[color][file] = fill_left(MASK(SQ_FROM_RF(rank, file)), ~(BITBOARD)0) |
				MASK(SQ_FLIP_COLOR(A2, color));
		for (; file <= FILE_E; file++)
			trapped_rook_mask[color][file] = (BITBOARD)0;
		for (; file <= FILE_H; file++)
			trapped_rook_mask[color][file] = fill_right(MASK(SQ_FROM_RF(rank, file)), ~(BITBOARD)0) |
				MASK(SQ_FLIP_COLOR(H2, color));
	}
}

/**
initialize_params():
Given a set of eval parameters different than the defaults, allocate memory so as to not corrupt the real
evaluation data. Each parameter set must be free'd by free_params in order to not cause a memory leak.
Created 101907; last modified 101907
**/
void initialize_params(EVAL_PARAMETER *param)
{
	int p;
	int x;

	for (p = 0; eval_parameter[p].value != NULL; p++)
	{
		if (eval_parameter[p].dimensions == 0)
			x = 1;
		else if (eval_parameter[p].dimensions == 1)
			x = eval_parameter[p].dimension[0];
		else
			x = eval_parameter[p].dimension[0] * eval_parameter[p].dimension[1];
		param[p].dimensions = eval_parameter[p].dimensions;
		param[p].dimension[0] = eval_parameter[p].dimension[0];
		param[p].dimension[1] = eval_parameter[p].dimension[1];
		param[p].value = calloc(sizeof(VALUE), x);
	}
}

/**
free_params():
Given a set of eval parameters different than the defaults, deallocate all memory.
Created 101907; last modified 101907
**/
void free_params(EVAL_PARAMETER *param)
{
	int p;

	for (p = 0; param[p].value != NULL; p++)
		free(param[p].value);
}

/**
copy_params():
Given pointers to two sets of eval parameters, copies from one to the other.
Created 101907; last modified 101907
**/
void copy_params(EVAL_PARAMETER *from, EVAL_PARAMETER *to)
{
	int x;

	while (from->value != NULL)
	{
		if (from->dimensions == 0)
			*to->value = *from->value;
		else if (from->dimensions == 1)
		{
			for (x = 0; x < from->dimension[0]; x++)
				to->value[x] = from->value[x];
		}
		else
		{
			for (x = 0; x < from->dimension[0] * from->dimension[1]; x++)
				to->value[x] = from->value[x];
		}
		to++;
		from++;
	}
}

/**
print_params():
Given a set of eval parameters, print out each value. This is useful after they have been modified by the program,
perhaps by autotuning.
Created 101907; last modified 101907
**/
void print_params(EVAL_PARAMETER *param)
{
	int p;
	int x;

	for (p = 0; param[p].value != NULL; p++)
	{
		print("evalparam %i ", p);
		if (param[p].dimensions == 0)
			print("%i\n", *param[p].value);
		else if (eval_parameter[p].dimensions == 1)
		{
			for (x = 0; x < eval_parameter[p].dimension[0]; x++)
				print(" %i", param[p].value[x]);
			print("\n");
		}
		else
		{
			for (x = 0; x < eval_parameter[p].dimension[0] * eval_parameter[p].dimension[1]; x++)
				print(" %i", param[p].value[x]);
			print("\n");
		}
	}
}
