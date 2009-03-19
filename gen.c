/** ZCT/gen.c--Created 060305 **/

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
#include "bit.h"

SQ_RANK pawn_promote_rank[2] = { RANK_8, RANK_1 };

#define PAWN_PUSH(move, from, to)										\
	do																	\
	{																	\
		if (RANK_OF(to) == pawn_promote_rank[board.side_tm])			\
		{																\
			*move++ = SET_FROM(from) | SET_TO(to) | SET_PROMOTE(KNIGHT);\
			*move++ = SET_FROM(from) | SET_TO(to) | SET_PROMOTE(BISHOP);\
			*move++ = SET_FROM(from) | SET_TO(to) | SET_PROMOTE(ROOK);	\
			*move++ = SET_FROM(from) | SET_TO(to) | SET_PROMOTE(QUEEN);	\
		}		   														\
		else	   														\
			*move++ = SET_FROM(from) | SET_TO(to);						\
	} while (FALSE)

/**
generate_moves():
Generates pseudo-legal moves.
Created 060305; last modified 020608
**/
MOVE *generate_moves(MOVE *next_move)
{
	BITBOARD forward_pawns;
	BITBOARD moves;
	BITBOARD pieces;
	BITBOARD target;
	PIECE piece;
	SQUARE from;
	SQUARE to;

	/* pawns */
	pieces = board.piece_bb[PAWN] & board.color_bb[board.side_tm];
	forward_pawns = SHIFT_FORWARD(pieces, board.side_tm);

	/* pawn caps */
	target = board.color_bb[board.side_ntm];
	if (board.ep_square != OFF_BOARD)
		target |= MASK(board.ep_square);
	/* left */
	moves = SHIFT_LF(forward_pawns) & target;
	FOR_BB(to, moves)
	{
		from = to - pawn_step[board.side_tm] + 1;
		PAWN_PUSH(next_move, from, to);
	}
	/* right */
	moves = SHIFT_RT(forward_pawns) & target;
	FOR_BB(to, moves)
	{
		from = to - pawn_step[board.side_tm] - 1;
		PAWN_PUSH(next_move, from, to);
	}
	/* pawn single step */
	target = ~board.occupied_bb;
	moves = forward_pawns & target;
	/* save pawns on 3rd rank to double push afterwards */
	pieces = moves & MASK_RANK_COLOR(RANK_3, board.side_tm);
	FOR_BB(to, moves)
	{
		from = to - pawn_step[board.side_tm];
		PAWN_PUSH(next_move, from, to);
	}
	/* pawn double step */
	moves = SHIFT_FORWARD(pieces, board.side_tm) & target;
	FOR_BB(to, moves)
	{
		from = to - 2 * pawn_step[board.side_tm];
		*next_move++ = SET_FROM(from) | SET_TO(to);
	}

	/* other moves */
	target = ~board.color_bb[board.side_tm];
	for (piece = KNIGHT; piece <= KING; piece++)
	{
		pieces = board.color_bb[board.side_tm] & board.piece_bb[piece];
		FOR_BB(from, pieces)
		{
			moves = attacks_bb(piece, from) & target;
			FOR_BB(to, moves)
				*next_move++ = SET_FROM(from) | SET_TO(to);
		}
	}
	/* castling */
	from = board.king_square[board.side_tm];
	if (CAN_CASTLE_KS(board.castle_rights, board.side_tm) &&
		!(board.occupied_bb & castle_ks_mask[board.side_tm]) &&
		!is_attacked(from, board.side_ntm) &&
		!is_attacked(from + 1, board.side_ntm) &&
		!is_attacked(from + 2, board.side_ntm))
		*next_move++ = SET_FROM(from) | SET_TO(from + 2);
	if (CAN_CASTLE_QS(board.castle_rights, board.side_tm) &&
		!(board.occupied_bb & castle_qs_mask[board.side_tm]) &&
		!is_attacked(from, board.side_ntm) &&
		!is_attacked(from - 1, board.side_ntm) &&
		!is_attacked(from - 2, board.side_ntm))
		*next_move++ = SET_FROM(from) | SET_TO(from - 2);

	return next_move;
}

/**
generate_captures():
Generates pseudo-legal captures and queen promotions to be used in quiescent
search.
Created 101806; last modified 020608
**/
MOVE *generate_captures(MOVE *next_move)
{
	BITBOARD forward_pawns;
	BITBOARD moves;
	BITBOARD pieces;
	BITBOARD target;
	PIECE piece;
	SQUARE from;
	SQUARE to;

	/* pawn caps */
	pieces = board.piece_bb[PAWN] & board.color_bb[board.side_tm];
	forward_pawns = SHIFT_FORWARD(pieces, board.side_tm);
	target = board.color_bb[board.side_ntm];
	if (board.ep_square != OFF_BOARD)
		target |= MASK(board.ep_square);
	/* left */
	moves = SHIFT_LF(forward_pawns) & target;
	FOR_BB(to, moves)
	{
		from = to - pawn_step[board.side_tm] + 1;
		if (RANK_OF(to) == pawn_promote_rank[board.side_tm])
			*next_move++ = SET_FROM(from) | SET_TO(to) | SET_PROMOTE(QUEEN);
		else
			*next_move++ = SET_FROM(from) | SET_TO(to);
	}
	/* right */
	moves = SHIFT_RT(forward_pawns) & target;
	FOR_BB(to, moves)
	{
		from = to - pawn_step[board.side_tm] - 1;
		if (RANK_OF(to) == pawn_promote_rank[board.side_tm])
			*next_move++ = SET_FROM(from) | SET_TO(to) | SET_PROMOTE(QUEEN);
		else
			*next_move++ = SET_FROM(from) | SET_TO(to);
	}
	/* non-capture promotions */
	moves = forward_pawns & ~board.occupied_bb & MASK_RANK(pawn_promote_rank[board.side_tm]);
	while (moves)
	{
		to = first_square(moves);
		CLEAR_FIRST_BIT(moves);
		from = to - pawn_step[board.side_tm];
		*next_move++ = SET_FROM(from) | SET_TO(to) | SET_PROMOTE(QUEEN);
	}

	/* other moves */
	target = board.color_bb[board.side_ntm];
	for (piece = KNIGHT; piece <= KING; piece++)
	{
		pieces = board.color_bb[board.side_tm] & board.piece_bb[piece];
		while (pieces)
		{
			from = first_square(pieces);
			CLEAR_FIRST_BIT(pieces);
	
			moves = attacks_bb(piece, from) & target;
			while (moves)
			{
				to = first_square(moves);
				CLEAR_FIRST_BIT(moves);
				*next_move++ = SET_FROM(from) | SET_TO(to);
			}
		}
	}

	return next_move;
}

/**
generate_checks():
Generates pseudo-legal checking moves. Note that this does not generate any
captures, as it is intended to be used in quiescence after the captures have
already been generated and tried.
Created 103006; last modified 052107
**/
MOVE *generate_checks(MOVE *next_move)
{
	BITBOARD forward_pawns;
	BITBOARD moves;
	BITBOARD pieces;
	BITBOARD target;
	PIECE piece;
	SQUARE from;
	SQUARE to;
	SQUARE king_square;

	king_square = board.king_square[board.side_ntm];
	/* pawns */
	forward_pawns = SHIFT_FORWARD(board.piece_bb[PAWN] &
		board.color_bb[board.side_tm], board.side_tm);

	/* pawn single step */
	target = ~board.occupied_bb;
	moves = forward_pawns & target;
	/* save pawns on 3rd rank to double push afterwards */
	pieces = moves & MASK_RANK_COLOR(RANK_3, board.side_tm);
	target &= pawn_caps_bb[board.side_ntm][king_square];
	moves &= target & ~MASK_RANK(pawn_promote_rank[board.side_tm]);
	while (moves)
	{
		to = first_square(moves);
		CLEAR_FIRST_BIT(moves);
		*next_move++ = SET_FROM(to - pawn_step[board.side_tm]) | SET_TO(to);
	}
	/* pawn double step */
	moves = SHIFT_FORWARD(pieces, board.side_tm) & target;
	while (moves)
	{
		to = first_square(moves);
		CLEAR_FIRST_BIT(moves);
		*next_move++ = SET_FROM(to - 2 * pawn_step[board.side_tm]) | SET_TO(to);
	}

	/* other moves */
	for (piece = KNIGHT; piece <= QUEEN; piece++)
	{
		target = ~board.occupied_bb & attacks_bb(piece, king_square);
		pieces = board.color_bb[board.side_tm] & board.piece_bb[piece];
		while (pieces)
		{
			from = first_square(pieces);
			CLEAR_FIRST_BIT(pieces);
	
			moves = attacks_bb(piece, from) & target;
			while (moves)
			{
				to = first_square(moves);
				CLEAR_FIRST_BIT(moves);
				*next_move++ = SET_FROM(from) | SET_TO(to);
			}
		}
	}
	/* castling */
	target = MASK(king_square);
	from = board.king_square[board.side_tm];
	/* King side */
	if (CAN_CASTLE_KS(board.castle_rights, board.side_tm) &&
		!(board.occupied_bb & castle_ks_mask[board.side_tm]) &&
		!is_attacked(from, board.side_ntm) &&
		!is_attacked(from + 1, board.side_ntm) &&
		!is_attacked(from + 2, board.side_ntm))
	{
		/* Get the theoretical occupied state after the castling. */
		pieces = board.occupied_bb ^
			(MASK(from) | MASK(from + 1) | MASK(from + 2) | MASK(from + 3));
		if ((dir_attacks(from + 1, pieces, DIR_HORI) |
			dir_attacks(from + 1, pieces, DIR_VERT)) & target)
			*next_move++ = SET_FROM(from) | SET_TO(from + 2);
	}
	/* Queen Side */
	if (CAN_CASTLE_QS(board.castle_rights, board.side_tm) &&
		!(board.occupied_bb & castle_qs_mask[board.side_tm]) &&
		!is_attacked(from, board.side_ntm) &&
		!is_attacked(from - 1, board.side_ntm) &&
		!is_attacked(from - 2, board.side_ntm))
	{
		/* Get the theoretical occupied state after the castling. */
		pieces = board.occupied_bb ^
			(MASK(from) | MASK(from - 1) | MASK(from - 2) | MASK(from - 4));
		if ((dir_attacks(from - 1, pieces, DIR_HORI) |
			dir_attacks(from - 1, pieces, DIR_VERT)) & target)
			*next_move++ = SET_FROM(from) | SET_TO(from - 2);
	}
	return next_move;
}

/**
generate_evasions():
Generates evasions from a position with the side to move in check. The moves generated are guaranteed to be legal.
Created 052107; last modified 020608
**/
MOVE *generate_evasions(MOVE *next_move, BITBOARD checkers)
{
	BITBOARD forward_pawns;
	BITBOARD moves;
	BITBOARD pieces;
	BITBOARD target;
	BITBOARD old_occupied_bb;
	PIECE piece;
	SQUARE from;
	SQUARE to;

	ASSERT(pop_count(checkers) > 0 && pop_count(checkers) <= 2);
	/* If there is only one checking piece, then we can try captures and interpositions. */
	if (pop_count(checkers) == 1)
	{
		/* pawns */
		pieces = board.piece_bb[PAWN] & board.color_bb[board.side_tm];
		forward_pawns = SHIFT_FORWARD(pieces, board.side_tm);

		/* pawn caps */
		target = checkers;
		if (board.ep_square != OFF_BOARD && !(target & MASK_RANK_COLOR(RANK_2, board.side_tm)))
			SET_BIT(target, board.ep_square);
		moves = SHIFT_LF(forward_pawns) & target;
		while (moves)
		{
			to = first_square(moves);
			CLEAR_FIRST_BIT(moves);
			from = to - pawn_step[board.side_tm] + 1;
			if (is_pinned(from, board.side_tm))
				continue;

			PAWN_PUSH(next_move, from, to);
		}
		moves = SHIFT_RT(forward_pawns) & target;
		while (moves)
		{
			to = first_square(moves);
			CLEAR_FIRST_BIT(moves);
			from = to - pawn_step[board.side_tm] - 1;
			if (is_pinned(from, board.side_tm))
				continue;

			PAWN_PUSH(next_move, from, to);
		}
		target = in_between_bb[board.king_square[board.side_tm]]
			[first_square(checkers)];
		/* pawn single step interpositions */
		moves = forward_pawns & ~board.occupied_bb;
		/* save pawns on 3rd rank to double push afterwards */
		pieces = moves & MASK_RANK_COLOR(RANK_3, board.side_tm);
		moves &= target;
		while (moves)
		{
			to = first_square(moves);
			CLEAR_FIRST_BIT(moves);
			from = to - pawn_step[board.side_tm];
			if (is_pinned(from, board.side_tm))
				continue;

			PAWN_PUSH(next_move, from, to);
		}
		/* pawn double step interpositions */
		moves = SHIFT_FORWARD(pieces, board.side_tm) & target & ~board.occupied_bb;
		while (moves)
		{
			to = first_square(moves);
			CLEAR_FIRST_BIT(moves);
			from = to - 2 * pawn_step[board.side_tm];
			if (is_pinned(from, board.side_tm))
				continue;
			*next_move++ = SET_FROM(from) | SET_TO(to);
		}
		/* piece interpositions and captures */
		target |= checkers;
		for (piece = KNIGHT; piece <= QUEEN; piece++)
		{
			pieces = board.color_bb[board.side_tm] & board.piece_bb[piece];
			while (pieces)
			{
				from = first_square(pieces);
				CLEAR_FIRST_BIT(pieces);

				moves = attacks_bb(piece, from) & target;
				while (moves)
				{
					to = first_square(moves);
					CLEAR_FIRST_BIT(moves);
					*next_move++ = SET_FROM(from) | SET_TO(to);
				}
			}
		}
	}

	/* King moves */
	from = board.king_square[board.side_tm];
	moves = attacks_bb(KING, from) & ~board.color_bb[board.side_tm];
	old_occupied_bb = board.occupied_bb;
	while (moves)
	{
		to = first_square(moves);
		CLEAR_FIRST_BIT(moves);
		/* Change the occupancy maps to reflect the king's new position. */
		CLEAR_BIT(board.occupied_bb, from);
		SET_BIT(board.occupied_bb, to);
		if (!is_attacked(to, board.side_ntm))
			*next_move++ = SET_FROM(from) | SET_TO(to);
		board.occupied_bb = old_occupied_bb;
	}
	
	return next_move;
}

/**
generate_legal_moves():
Generates legal moves.
Created 090308; last modified 112308
**/
MOVE *generate_legal_moves(MOVE *next_move)
{
	BITBOARD not_pinned[4];
	BITBOARD all_pinned;
	BITBOARD checks;
	BITBOARD old_occupied_bb;
	BITBOARD old_pawns_bb;
	BITBOARD pieces;
	BITBOARD valid;
	BITBOARD forward_pawns;
	BITBOARD moves;
	BITBOARD target;
	BITBOARD evasion_target;
	PIECE piece;
	SQUARE from;
	SQUARE to;
	const DIRECTION pawn_dir[2] = { DIR_A8H1, DIR_A1H8 };
	DIRECTION dir;
	int check_count;

	get_attack_bb_set(&checks, not_pinned, &all_pinned);

	/* Calculate some targets for escaping check, if we happen to be in it. */
	check_count = pop_count(checks);
	/* No checks -- all pieces are free to move. */
	if (check_count == 0)
		evasion_target = ~(BITBOARD)0;
	/* One check -- pieces can block the check or capture the checker. */
	else if (check_count == 1)
	{
		to = board.king_square[board.side_tm];
		from = first_square(checks);
		evasion_target = checks | in_between_bb[from][to];
	}
	/* Two checks -- only king can move. */
	else
		evasion_target = 0;

	if (check_count < 2)
	{
	/* pawns */
	pieces = board.piece_bb[PAWN] & board.color_bb[board.side_tm];
	target = board.color_bb[board.side_ntm] & evasion_target;
	/* pawn caps left */
	dir = pawn_dir[board.side_tm];
	valid = pieces & not_pinned[dir];
	moves = SHIFT_LF(SHIFT_FORWARD(valid, board.side_tm)) & target;
	FOR_BB(to, moves)
	{
		from = to - pawn_step[board.side_tm] + 1;
		PAWN_PUSH(next_move, from, to);
	}
	/* pawn caps left */
	dir = pawn_dir[board.side_tm ^ 1];
	valid = pieces & not_pinned[dir];
	moves = SHIFT_RT(SHIFT_FORWARD(valid, board.side_tm)) & target;
	FOR_BB(to, moves)
	{
		from = to - pawn_step[board.side_tm] - 1;
		PAWN_PUSH(next_move, from, to);
	}
	/* en passant caps */
	if (board.ep_square != OFF_BOARD)
	{
		to = board.ep_square;
		valid = pawn_caps_bb[board.side_ntm][to] &
			board.piece_bb[PAWN] & board.color_bb[board.side_tm];
		old_occupied_bb = board.occupied_bb;
		old_pawns_bb = board.piece_bb[PAWN];
		FOR_BB(from, valid)
		{
			/* Flip the three squares where the occupancy changes to check if
				it leaves the king in check. */
			board.occupied_bb ^= MASK(from) | MASK(to) |
				MASK(to - pawn_step[board.side_tm]);
			/* Take out the pawn, in case it is putting us in check. */
			board.piece_bb[PAWN] ^= MASK(to - pawn_step[board.side_tm]);

			if (!is_attacked(board.king_square[board.side_tm], board.side_ntm))
				*next_move++ = SET_FROM(from) | SET_TO(to);

			/* Revert the occupied state back to the original. */
			board.occupied_bb = old_occupied_bb;
			board.piece_bb[PAWN] = old_pawns_bb;
		}
	}
	/* pawn pushes */
	target = ~board.occupied_bb;
	valid = pieces & not_pinned[DIR_VERT];
	moves = SHIFT_FORWARD(valid, board.side_tm) & target;
	/* save pawns on 3rd rank to double push afterwards */
	pieces = moves & MASK_RANK_COLOR(RANK_3, board.side_tm);
	moves &= evasion_target;
	FOR_BB(to, moves)
	{
		from = to - pawn_step[board.side_tm];
		PAWN_PUSH(next_move, from, to);
	}
	/* pawn double steps */
	moves = SHIFT_FORWARD(pieces, board.side_tm) & target & evasion_target;
	FOR_BB(to, moves)
	{
		from = to - 2 * pawn_step[board.side_tm];
		*next_move++ = SET_FROM(from) | SET_TO(to);
	}

	target = ~board.color_bb[board.side_tm] & evasion_target;
	/* Calculate moves for knights. */
	pieces = board.piece_bb[KNIGHT] & board.color_bb[board.side_tm];
	valid = pieces & ~all_pinned;
	FOR_BB(from, valid)
	{
		moves = knight_moves_bb[from] & target;
		FOR_BB(to, moves)
			*next_move++ = SET_FROM(from) | SET_TO(to);
	}

	/* Calculate moves for orthognal sliders by direction. */
	pieces = (board.piece_bb[BISHOP] | board.piece_bb[QUEEN]) &
		board.color_bb[board.side_tm];
	/* a1h8 diagonal */
	valid = pieces & not_pinned[DIR_A1H8];
	FOR_BB(from, valid)
	{
		moves = dir_attacks(from, board.occupied_bb, DIR_A1H8) & target;
		FOR_BB(to, moves)
			*next_move++ = SET_FROM(from) | SET_TO(to);
	}
	/* a8h1 diagonal */
	valid = pieces & not_pinned[DIR_A8H1];
	FOR_BB(from, valid)
	{
		moves = dir_attacks(from, board.occupied_bb, DIR_A8H1) & target;
		FOR_BB(to, moves)
			*next_move++ = SET_FROM(from) | SET_TO(to);
	}

	/* Calculate moves for diagonal sliders by direction. */
	pieces = (board.piece_bb[ROOK] | board.piece_bb[QUEEN]) &
		board.color_bb[board.side_tm];
	/* horizontal */
	valid = pieces & not_pinned[DIR_HORI];
	FOR_BB(from, valid)
	{
		moves = dir_attacks(from, board.occupied_bb, DIR_HORI) & target;
		FOR_BB(to, moves)
			*next_move++ = SET_FROM(from) | SET_TO(to);
	}
	/* vertical */
	valid = pieces & not_pinned[DIR_VERT];
	FOR_BB(from, valid)
	{
		moves = dir_attacks(from, board.occupied_bb, DIR_VERT) & target;
		FOR_BB(to, moves)
			*next_move++ = SET_FROM(from) | SET_TO(to);
	}
	}

	/* king */
	target = ~board.color_bb[board.side_tm];
	from = board.king_square[board.side_tm];
	moves = king_moves_bb[from] & target;
	old_occupied_bb = board.occupied_bb;
	CLEAR_BIT(board.occupied_bb, from);
	FOR_BB(to, moves)
	{
		if (!is_attacked(to, board.side_ntm))
			*next_move++ = SET_FROM(from) | SET_TO(to);
	}
	board.occupied_bb = old_occupied_bb;

	/* castling */
	if (CAN_CASTLE_KS(board.castle_rights, board.side_tm) &&
		!(board.occupied_bb & castle_ks_mask[board.side_tm]) &&
		!checks &&
		!is_attacked(from + 1, board.side_ntm) &&
		!is_attacked(from + 2, board.side_ntm))
		*next_move++ = SET_FROM(from) | SET_TO(from + 2);
	if (CAN_CASTLE_QS(board.castle_rights, board.side_tm) &&
		!(board.occupied_bb & castle_qs_mask[board.side_tm]) &&
		!checks &&
		!is_attacked(from - 1, board.side_ntm) &&
		!is_attacked(from - 2, board.side_ntm))
		*next_move++ = SET_FROM(from) | SET_TO(from - 2);

	return next_move;
}
