/** ZCT/verify.c--Created 071505 **/

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

/**
verify():
Verifies the integrity of the various board information, based on the piece and color arrays.
Created 071505; last modified 011208
**/
void verify(void)
{
	BOOL fail;
	BOARD old_board;
	COLOR c;
	PIECE p;

	old_board = board;
	initialize_bitboards();
	initialize_hashkey();

	fail = FALSE;
	/* Check the occupied bitboards. */
	if (old_board.occupied_bb != board.occupied_bb)
	{
		fail = TRUE;
		print("occupied_bb:\n%lI", old_board.occupied_bb, board.occupied_bb);
	}
	/* Check the piece bitboards. */
	for (p = PAWN; p <= KING; p++)
	{
		if (old_board.piece_bb[p] != board.piece_bb[p])
		{
			fail = TRUE;
			print("piece_bb[%P]:\n%lI", p, old_board.piece_bb[p],
				board.piece_bb[p]);
		}
	}
	for (c = WHITE; c <= BLACK; c++)
	{
		/* Check the color bitboards. */
		if (old_board.color_bb[c] != board.color_bb[c])
		{
			fail = TRUE;
			print("color_bb[%C]:\n%lI", c, old_board.color_bb[c],
				board.color_bb[c]);
		}
		/* Check the pawn counts. */
		if (old_board.pawn_count[c] != board.pawn_count[c])
		{
			fail = TRUE;
			print("pawn_count[%C]: %i %i\n", c, old_board.pawn_count[c],
				board.pawn_count[c]);
		}
		/* Check the piece counts. */
		if (old_board.piece_count[c] != board.piece_count[c])
		{
			fail = TRUE;
			print("piece_count[%C]: %i %i\n", c, old_board.piece_count[c],
				board.piece_count[c]);
		}
		/* Check the material. */
		if (old_board.material[c] != board.material[c])
		{
			fail = TRUE;
			print("material[%C]: %i %i\n", c, old_board.material[c],
				board.material[c]);
		}
	}
	/* Check the hashkey. */
	if (board.hashkey != old_board.hashkey)
	{
		fail = TRUE;
		print("hashkey:\n%lI", old_board.hashkey, board.hashkey);
	}
	/* Check the path hashkey. */
/*	if (board.path_hashkey != old_board.path_hashkey)
	{
		fail = TRUE;
		print("path_hashkey:\n%lI", old_board.path_hashkey, board.path_hashkey);
	}*/
	/* Check the pawn hashkey. */
	if (board.pawn_entry.hashkey != old_board.pawn_entry.hashkey)
	{
		fail = TRUE;
		print("pawn_hashkey:\n%lI", old_board.pawn_entry.hashkey,
			board.pawn_entry.hashkey);
	}
	/* Check if there was a failure. If so, print the board and exit. */
	if (fail)
	{
		print("%B%i\n", &board, board.game_entry - board.game_stack);
		while(--board.game_entry >= board.game_stack)
			print("%S%S\n",	MOVE_FROM((board.game_entry)->move),
				MOVE_TO((board.game_entry)->move));
		fatal_error("fatal error: board verify failed.\n");
	}
}

/**
move_is_valid():
This functions tests the validity of a move in the current position. This is used for hash moves
and threat moves. This does not test their legality, just their pseudo-legality.
Created 011108; last modified 011208
**/
BOOL move_is_valid(MOVE move)
{
	PIECE piece;
	PIECE promote;
	SQUARE from;
	SQUARE to;

	
	/* Check some basic validity measures. */
	if (move == NO_MOVE)
		return FALSE;
	from = MOVE_FROM(move);
	to = MOVE_TO(move);
	if (from == to)
		return FALSE;
	/* Check the colors of the from and to squares. */
	if (board.color[from] != board.side_tm)
		return FALSE;
	if (board.color[to] == board.side_tm)
		return FALSE;
	/* Check the moving piece, see if it can move to the to square. */
	piece = board.piece[from];
	promote = MOVE_PROMOTE(move);
	/* Obviously only a pawn can promote! */
	if (promote && piece != PAWN)
		return FALSE;
	if (piece == PAWN)
	{
		/* Check pawn captures. */
		if (MASK(to) & pawn_caps_bb[board.side_tm][from])
		{
			if (board.color[to] != board.side_ntm && to != board.ep_square)
				return FALSE;
		}
		/* Check single and double step pawn moves. */
		else if (FILE_OF(from) == FILE_OF(to))
		{
			if (board.color[to] != EMPTY)
				return FALSE;
			if (SQ_FLIP_COLOR(to, board.side_tm) <
				SQ_FLIP_COLOR(from, board.side_tm))
				return FALSE;
			if (RANK_DISTANCE(from, to) > 2)
				return FALSE;
			if (RANK_DISTANCE(from, to) == 2)
			{
				/* Look at the square in the middle for double pawn steps. */
				if (board.color[(from + to) / 2] != EMPTY)
					return FALSE;
				/* Make sure the pawn started from the second rank. */
				if (RANK_OF(SQ_FLIP_COLOR(from, board.side_tm)) != RANK_2)
					return FALSE;
			}
		}
		else
			return FALSE;
		/* Check promotion on single step pawn moves. */
		if (RANK_DISTANCE(from, to) == 1)
		{
			if (promote && RANK_OF(SQ_FLIP_COLOR(to, board.side_tm)) != RANK_8)
				return FALSE;
			if (!promote && RANK_OF(SQ_FLIP_COLOR(to, board.side_tm)) == RANK_8)
				return FALSE;
		}
	}
	/* Check castling moves. */
	else if (piece == KING && FILE_DISTANCE(from, to) == 2)
	{
		if (abs(from - to) != 2)
			return FALSE;
		if (to > from)
		{
			if (!CAN_CASTLE_KS(board.castle_rights, board.side_tm) ||
				(board.occupied_bb & castle_ks_mask[board.side_tm]) ||
				is_attacked(from, board.side_ntm) ||
				is_attacked(from + 1, board.side_ntm) ||
				is_attacked(from + 2, board.side_ntm))
			return FALSE;
		}
		else
		{
			if (!CAN_CASTLE_QS(board.castle_rights, board.side_tm) ||
				(board.occupied_bb & castle_qs_mask[board.side_tm]) ||
				is_attacked(from, board.side_ntm) ||
				is_attacked(from - 1, board.side_ntm) ||
				is_attacked(from - 2, board.side_ntm))
			return FALSE;
		}
	}
	else
	{
		if (!(attacks_bb(piece, from) & MASK(to)))
			return FALSE;
	}
	return TRUE;
}
