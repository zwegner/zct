/** ZCT/unmake.c--Created 070705 **/

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
unmake_move():
Unmakes the last move on the internal board.
Created 070905; last modified 050408
**/
void unmake_move()
{
	MOVE move;
	PIECE piece;
	PIECE promote;
	PIECE cap;
	SQUARE ep_square;
	SQUARE from;
	SQUARE to;
	SQUARE castle_from;
	SQUARE castle_to;

	if (board.game_entry <= board.game_stack)
		return;
	board.game_entry--;

	/* Restore game info from game_stack. */
	board.castle_rights = board.game_entry->castle_rights;
	board.ep_square = board.game_entry->ep_square;
	board.fifty_count = board.game_entry->fifty_count;
	board.hashkey = board.game_entry->hashkey;
	board.path_hashkey = board.game_entry->path_hashkey;
	board.pawn_entry.hashkey = board.game_entry->pawn_hashkey;
	move = board.game_entry->move;
	cap = board.game_entry->capture;

	/* Switch the side and such. */
	board.side_tm = board.side_ntm;
	board.side_ntm = COLOR_FLIP(board.side_ntm);

	if (board.side_tm == BLACK)
		board.move_number--;

	if (move == NULL_MOVE)
		return;

	from = MOVE_FROM(move);
	to = MOVE_TO(move);
	piece = board.piece[to];
	promote = MOVE_PROMOTE(move);

	/* promotions */
	if (promote)
	{
		CLEAR_BIT(board.piece_bb[promote], to);
		board.pawn_count[board.side_tm]++;
		board.piece_count[board.side_tm]--;
		board.material[board.side_tm] -=
			piece_value[promote] - piece_value[PAWN];
		piece = PAWN;
	}

	/* Standard piece movement. */
	CLEAR_BIT(board.piece_bb[piece], to);
	CLEAR_BIT(board.color_bb[board.side_tm], to);
	SET_BIT(board.piece_bb[piece], from);
	SET_BIT(board.color_bb[board.side_tm], from);
	SET_BIT(board.occupied_bb, from);
	board.piece[from] = piece;
	board.color[from] = board.side_tm;

	/* Restore the TO square based on whether the move was a capture or not. */
	if (cap == EMPTY)
	{
		CLEAR_BIT(board.occupied_bb, to);
		board.piece[to] = EMPTY;
		board.color[to] = EMPTY;
	}
	else
	{
		SET_BIT(board.piece_bb[cap], to); 
		SET_BIT(board.color_bb[board.side_ntm], to);
		board.piece[to] = cap;
		board.color[to] = board.side_ntm;
		if (cap == PAWN)
			board.pawn_count[board.side_ntm]++;
		else
			board.piece_count[board.side_ntm]++;
		board.material[board.side_ntm] += piece_value[cap];
	}

	/* En passant. */
	if (piece == PAWN && to == board.ep_square)
	{
		ep_square = to - pawn_step[board.side_tm];
		SET_BIT(board.occupied_bb, ep_square);
		SET_BIT(board.color_bb[board.side_ntm], ep_square);
		SET_BIT(board.piece_bb[PAWN], ep_square);
		board.color[ep_square] = board.side_ntm;
		board.piece[ep_square] = PAWN;
		board.pawn_count[board.side_ntm]++;
		board.material[board.side_ntm] += piece_value[PAWN];
	}
	else if (piece == KING)
	{
		board.king_square[board.side_tm] = from;
		/* castling */
		if (ABS(to - from) == 2)
		{
			if (to > from)
			{
				castle_from = from + 3;
				castle_to = from + 1;
			}
			else
			{
				castle_from = from - 4;
				castle_to = from - 1;
			}
			CLEAR_BIT(board.occupied_bb, castle_to);
			CLEAR_BIT(board.color_bb[board.side_tm], castle_to);
			CLEAR_BIT(board.piece_bb[ROOK], castle_to);
			SET_BIT(board.occupied_bb, castle_from);
			SET_BIT(board.color_bb[board.side_tm], castle_from);
			SET_BIT(board.piece_bb[ROOK], castle_from);
			board.color[castle_to] = EMPTY;
			board.color[castle_from] = board.side_tm;
			board.piece[castle_to] = EMPTY;
			board.piece[castle_from] = ROOK;
		}
	}
}
