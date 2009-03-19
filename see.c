/** ZCT/see.c--Created 091706 **/

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

/**
see():
Performs a static exchange evaluation (SEE) for the given capture to see if it
is winning or not.
Created 091706; last modified 090708
**/
VALUE see(SQUARE to, SQUARE from)
{
	int count;
	BITBOARD attacks[2];
	BITBOARD all_attacks[2];
	BITBOARD new_attacks;
	BITBOARD occupied;
	BITBOARD piece_bb;
	COLOR color;
	PIECE piece;
	VALUE value[32];
	VALUE attacked_piece;

	count = 0;
	value[0] = piece_value[board.piece[to]];
	/* If we are promoting a pawn, we need to change the value of the piece. */
	if (board.piece[from] == PAWN &&
		(RANK_OF(to) == RANK_1 || RANK_OF(to) == RANK_8))
		value[0] += attacked_piece = piece_value[QUEEN] - piece_value[PAWN];
	else
		attacked_piece = piece_value[board.piece[from]];
	/* Initialize the occupied set by clearing the piece that just moved.  */
	occupied = board.occupied_bb ^ MASK(from);
	/* The new_attacks bitboard finds attacks uncovered by a piece moving. */
	new_attacks =
		((dir_attacks(to, occupied, DIR_A1H8) |
		  dir_attacks(to, occupied, DIR_A8H1)) &
		(board.piece_bb[BISHOP] | board.piece_bb[QUEEN])) |
		((dir_attacks(to, occupied, DIR_HORI) |
		  dir_attacks(to, occupied, DIR_VERT)) &
		(board.piece_bb[ROOK] | board.piece_bb[QUEEN]));
	/* Initialize the attack sets. */
	attacks[WHITE] = attack_squares(to, WHITE) |
		(new_attacks & board.color_bb[WHITE]);
	attacks[BLACK] = attack_squares(to, BLACK) |
		(new_attacks & board.color_bb[BLACK]);
	/* The all_attacks bitboards keep track of what attacks we have already used
		previously and new attacks that are uncovered. */
	all_attacks[WHITE] = attacks[WHITE];
	all_attacks[BLACK] = attacks[BLACK];

	color = board.side_tm;
	attacks[color] &= ~MASK(from);
	color = COLOR_FLIP(color);
	/* Loop through the attacks. The lowest-valued attacker for each side
		is pulled off in succession. */
	while (attacks[color])
	{
		for (piece = PAWN; piece <= KING; piece++)
		{
			piece_bb = attacks[color] & board.piece_bb[piece];
			if (piece_bb)
			{
				piece_bb &= -piece_bb;
				attacks[color] ^= piece_bb;
				/* Now add in any sliders behind the piece just captured. */
				occupied ^= piece_bb;
				new_attacks =
					((dir_attacks(to, occupied, DIR_A1H8) |
					  dir_attacks(to, occupied, DIR_A8H1)) &
					(board.piece_bb[BISHOP] | board.piece_bb[QUEEN])) |
					((dir_attacks(to, occupied, DIR_HORI) |
					  dir_attacks(to, occupied, DIR_VERT)) &
					(board.piece_bb[ROOK] | board.piece_bb[QUEEN]));
				/* We add the new attacks into the attack set, and make sure
					older attacks are excluded. */
				attacks[WHITE] |= new_attacks &
					~all_attacks[WHITE] & board.color_bb[WHITE];
				attacks[BLACK] |= new_attacks &
					~all_attacks[BLACK] & board.color_bb[BLACK];

				all_attacks[WHITE] |= new_attacks & board.color_bb[WHITE];
				all_attacks[BLACK] |= new_attacks & board.color_bb[BLACK];
				break;
			}
		}
		count++;
		/* Add in the value of this capture into the "search stack".
			We keep track of king captures specially so that illegal moves
			are flagged as such. */
		if (attacked_piece == piece_value[KING])
			value[count] = MATE;
		else
		{
			if (value[count - 1] == MATE)
				value[count] = -MATE;
			else
				value[count] = -value[count - 1] + attacked_piece;
		}
		attacked_piece = piece_value[piece];
		color = COLOR_FLIP(color);
	}
	/* Now, backup through the stack and minimax the best value. */
	while (count > 0)
	{
		if (value[count] > -value[count - 1])
			value[count - 1] = -value[count];
		count--;
	}
	return value[0];	
}
