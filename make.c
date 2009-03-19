/** ZCT/make.c--Created 070305 **/

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
make_move():
Makes the given move on the internal board.
Created 070305; last modified 050408
**/
BOOL make_move(MOVE move)
{
	PIECE cap;
	PIECE piece;
	PIECE promote;
	SQUARE ep_square;
	SQUARE from;
	SQUARE to;
	SQUARE castle_from;
	SQUARE castle_to;

	/* Back up game info for undoing this move. */
	board.game_entry->move = move;
	board.game_entry->ep_square = board.ep_square;
	board.game_entry->castle_rights = board.castle_rights;
	board.game_entry->fifty_count = board.fifty_count;
	board.game_entry->hashkey = board.hashkey;
	board.game_entry->path_hashkey = board.path_hashkey;
	board.game_entry->pawn_hashkey = board.pawn_entry.hashkey;

	if (board.side_tm == BLACK)
		board.move_number++;

	/* Null move, quick and cheap. */
	if (move == NULL_MOVE)
	{
		board.hashkey ^= zobrist_ep[board.ep_square];
		board.hashkey ^= zobrist_stm;
		board.fifty_count = 0;
		board.ep_square = OFF_BOARD;
		board.hashkey ^= zobrist_ep[OFF_BOARD];
//		update_path_hashkey(NULL_MOVE);
		board.side_tm = board.side_ntm;
		board.side_ntm = COLOR_FLIP(board.side_ntm);
		board.game_entry->capture = EMPTY;
		board.game_entry++;
		return TRUE;
	}

	from = MOVE_FROM(move);
	to = MOVE_TO(move);
	cap = board.piece[to];
	piece = board.piece[from];
	promote = MOVE_PROMOTE(move);
	/* Update before we increment game_entry. */
//	update_path_hashkey(move);

	board.game_entry->capture = cap;
	board.game_entry++;

	board.fifty_count++;
	board.hashkey ^= zobrist_castle[board.castle_rights];
	board.castle_rights &= castle_rights_mask[from] & castle_rights_mask[to];
	board.hashkey ^= zobrist_castle[board.castle_rights];
	board.hashkey ^= zobrist_ep[board.ep_square];
	board.hashkey ^= zobrist_stm;

	if (cap != EMPTY)
	{
		board.fifty_count = 0;
		CLEAR_BIT(board.color_bb[board.side_ntm], to);
		CLEAR_BIT(board.piece_bb[cap], to); 
		board.material[board.side_ntm] -= piece_value[cap];
		board.hashkey ^= zobrist_piece[board.side_ntm][cap][to];
		if (cap == PAWN)
		{
			board.pawn_entry.hashkey ^=
				zobrist_piece[board.side_ntm][PAWN][to];
			board.pawn_count[board.side_ntm]--;
		}
		else
			board.piece_count[board.side_ntm]--;
	}
	CLEAR_BIT(board.occupied_bb, from);
	CLEAR_BIT(board.color_bb[board.side_tm], from);
	CLEAR_BIT(board.piece_bb[piece], from);
	SET_BIT(board.occupied_bb, to);
	SET_BIT(board.color_bb[board.side_tm], to);
	SET_BIT(board.piece_bb[piece], to);
	board.color[from] = EMPTY;
	board.piece[from] = EMPTY;
	board.color[to] = board.side_tm;
	board.piece[to] = piece;
	board.hashkey ^= zobrist_piece[board.side_tm][piece][from];
	board.hashkey ^= zobrist_piece[board.side_tm][piece][to];

	/* Pawn moves require some extra handling. */
	if (piece == PAWN)
	{
		/* Adjust pawn hashkey and fifty count. */
		board.pawn_entry.hashkey ^=
			zobrist_piece[board.side_tm][PAWN][from];
		board.pawn_entry.hashkey ^=
			zobrist_piece[board.side_tm][PAWN][to];
		board.fifty_count = 0;
		/* Set the en passant square on captures. */
		/* ep_square is the square of the pawn removed on ep captures, and the
			square where en passant is legal after a double pawn step. */
		ep_square = to - pawn_step[board.side_tm];
		if (ABS(to - from) == 16 && (pawn_caps_bb[board.side_tm][ep_square] &
			board.piece_bb[PAWN] & board.color_bb[board.side_ntm]))
			board.ep_square = ep_square;
		else
		{
			/* Remove ep pawn. */
			if (to == board.ep_square)
			{
				CLEAR_BIT(board.occupied_bb, ep_square);
				CLEAR_BIT(board.color_bb[board.side_ntm], ep_square);
				CLEAR_BIT(board.piece_bb[PAWN], ep_square);
				board.color[ep_square] = EMPTY;
				board.piece[ep_square] = EMPTY;
				board.pawn_count[board.side_ntm]--;
				board.material[board.side_ntm] -= piece_value[PAWN];
				board.hashkey ^= zobrist_piece[board.side_ntm][PAWN][ep_square];
				board.pawn_entry.hashkey ^=
					zobrist_piece[board.side_ntm][PAWN][ep_square];
			}
			/* Handle promotions. */
			else if (promote)
			{
				CLEAR_BIT(board.piece_bb[PAWN], to);
				SET_BIT(board.piece_bb[promote], to);
				board.piece[to] = promote;
				board.pawn_count[board.side_tm]--;
				board.piece_count[board.side_tm]++;
				board.material[board.side_tm] +=
					piece_value[promote] - piece_value[PAWN];
				board.hashkey ^= zobrist_piece[board.side_tm][PAWN][to];
				board.hashkey ^= zobrist_piece[board.side_tm][promote][to];
				board.pawn_entry.hashkey ^=
					zobrist_piece[board.side_tm][PAWN][to];
			}
			board.ep_square = OFF_BOARD;
		}
	}
	else
	{
		board.ep_square = OFF_BOARD;
		if (piece == KING)
		{
			board.king_square[board.side_tm] = to;
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
				CLEAR_BIT(board.occupied_bb, castle_from);
				CLEAR_BIT(board.color_bb[board.side_tm], castle_from);
				CLEAR_BIT(board.piece_bb[ROOK], castle_from);
				SET_BIT(board.occupied_bb, castle_to);
				SET_BIT(board.color_bb[board.side_tm], castle_to);
				SET_BIT(board.piece_bb[ROOK], castle_to);
				board.color[castle_from] = EMPTY;
				board.piece[castle_from] = EMPTY;
				board.color[castle_to] = board.side_tm;
				board.piece[castle_to] = ROOK;
				board.hashkey ^= zobrist_piece[board.side_tm][ROOK][castle_from];
				board.hashkey ^= zobrist_piece[board.side_tm][ROOK][castle_to];
			}
		}
	}
	board.hashkey ^= zobrist_ep[board.ep_square];

	/* Check legality. */
	if (in_check())
	{
		board.side_tm = board.side_ntm;
		board.side_ntm = COLOR_FLIP(board.side_ntm);
		unmake_move();
		return FALSE;
	}
	board.side_tm = board.side_ntm;
	board.side_ntm = COLOR_FLIP(board.side_ntm);
	return TRUE;
}

/**
update_path_hashkey():
When updating the path hashkey, we only take into consideration the last few moves.
This function is used to take out an old move from the key and add in the new one.
Created 050408; last modified 050408
**/
void update_path_hashkey(MOVE move)
{
	MOVE old_move;
	int entry;

	entry = (board.game_entry - board.game_stack) % HASH_PATH_MOVE_COUNT;
	/* Take out old move. */
	if (board.game_entry - HASH_PATH_MOVE_COUNT >= board.game_stack)
	{
		old_move = (board.game_entry - HASH_PATH_MOVE_COUNT)->move;
		board.path_hashkey ^= zobrist_path_move[MOVE_KEY(old_move)][entry];
	}
	board.path_hashkey ^= zobrist_path_move[MOVE_KEY(move)][entry];
}

/**
is_legal():
Checks if the given move is legal, i.e. does not put the king in check. This function is
needed to not clobber some internal game history arrays.
Created 091507; last modified 091507
**/
BOOL is_legal(MOVE move)
{
	BOOL checked;
	PIECE cap;
	PIECE piece;
	SQUARE ep_square;
	SQUARE from;
	SQUARE to;
	SQUARE castle_from;
	SQUARE castle_to;

	from = MOVE_FROM(move);
	to = MOVE_TO(move);
	cap = board.piece[to];
	piece = board.piece[from];
	/* Suppress stupid compiler warnings!! */
	ep_square = OFF_BOARD;
	castle_from = OFF_BOARD;
	castle_to = OFF_BOARD;

	if (cap != EMPTY)
	{
		CLEAR_BIT(board.color_bb[board.side_ntm], to);
		CLEAR_BIT(board.piece_bb[cap], to); 
	}
	else
		SET_BIT(board.occupied_bb, to);
	CLEAR_BIT(board.occupied_bb, from);
	/* ep captures */
	if (piece == PAWN && to == board.ep_square)
	{
		ep_square = to - pawn_step[board.side_tm];
		CLEAR_BIT(board.occupied_bb, ep_square);
		CLEAR_BIT(board.color_bb[board.side_ntm], ep_square);
		CLEAR_BIT(board.piece_bb[PAWN], ep_square);
	}
	else if (piece == KING)
	{
		board.king_square[board.side_tm] = to;
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
			CLEAR_BIT(board.occupied_bb, castle_from);
			SET_BIT(board.occupied_bb, castle_to);
		}
	}
	/* We have changed all of the internal bitboards, now see if the king is in check. */
	checked = in_check();

	SET_BIT(board.occupied_bb, from);
	if (cap != EMPTY)
	{
		SET_BIT(board.color_bb[board.side_ntm], to);
		SET_BIT(board.piece_bb[cap], to); 
	}
	else
		CLEAR_BIT(board.occupied_bb, to);
	/* ep captures */
	if (piece == PAWN && to == board.ep_square)
	{
		SET_BIT(board.occupied_bb, ep_square);
		SET_BIT(board.color_bb[board.side_ntm], ep_square);
		SET_BIT(board.piece_bb[PAWN], ep_square);
	}
	else if (piece == KING)
	{
		board.king_square[board.side_tm] = from;
		/* castling */
		if (ABS(to - from) == 2)
		{
			SET_BIT(board.occupied_bb, castle_from);
			CLEAR_BIT(board.occupied_bb, castle_to);
		}
	}
	return !checked;
}
