/** ZCT/check.c--Created 070705 **/

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

/**
in_check():
Returns TRUE if the side to move is in check.
Created 070705; last modified 091806
**/
BOOL in_check(void)
{
	return is_attacked(board.king_square[board.side_tm], board.side_ntm);
}

/**
check_squares():
Returns the bitboard of all pieces that are checking the king -- max of 2.
Created 083106; last modified 091806
**/
BITBOARD check_squares(void)
{
	return attack_squares(board.king_square[board.side_tm], board.side_ntm);
}

/**
is_attacked():
Returns TRUE if the given square is attacked by the given side.
Created 070705; last modified 070905
**/
BOOL is_attacked(SQUARE square, COLOR color)
{
	BITBOARD attacks;

	/* pawns */
	attacks = pawn_caps_bb[COLOR_FLIP(color)][square];
	if (attacks & board.piece_bb[PAWN] & board.color_bb[color])
		return TRUE;
	/* knights */
	attacks = knight_moves_bb[square];
	if (attacks & board.piece_bb[KNIGHT] & board.color_bb[color])
		return TRUE;
	/* kings */
	attacks = king_moves_bb[square];
	if (attacks & board.piece_bb[KING] & board.color_bb[color])
		return TRUE;
	/* bishop sliders */
	attacks = attacks_bb(BISHOP, square);
	if (attacks & (board.piece_bb[BISHOP] | board.piece_bb[QUEEN]) &
		board.color_bb[color])
		return TRUE;
	/* rook sliders */
	attacks = attacks_bb(ROOK, square);
	if (attacks & (board.piece_bb[ROOK] | board.piece_bb[QUEEN]) &
		board.color_bb[color])
		return TRUE;

	return FALSE;
}

/**
attack_squares():
Returns a bitboard of all pieces of a given side attacking the given square.
Created 091806; last modified 091806
**/
BITBOARD attack_squares(SQUARE square, COLOR color)
{
	return board.color_bb[color] &
		((pawn_caps_bb[COLOR_FLIP(color)][square] & board.piece_bb[PAWN]) |
		(knight_moves_bb[square] & board.piece_bb[KNIGHT]) |
		(attacks_bb(BISHOP, square) &
			(board.piece_bb[BISHOP] | board.piece_bb[QUEEN])) |
		(attacks_bb(ROOK, square) &
			(board.piece_bb[ROOK] | board.piece_bb[QUEEN])) |
		(king_moves_bb[square] & board.piece_bb[KING]));
}

/**
move_gives_check():
Simply enough, looks to see if the given move in the current position is a checking move.
Created 052408; last modified 052408
**/
BOOL move_gives_check(MOVE move)
{
	BOOL check;

	if (make_move(move))
	{
		check = in_check();
		unmake_move();
		return check;
	}
	else
		return FALSE;
}

/**
is_pinned():
For a given side checks if the given piece is pinned to its king.
Created 052107; last modified 090108
**/
BOOL is_pinned(SQUARE sq, COLOR color)
{
	SQUARE king_sq;
	BITBOARD occupied;
	BITBOARD attacks;
	BITBOARD old_attacks;

	king_sq = board.king_square[color];
	occupied = board.occupied_bb & ~MASK(sq);
	/* Bishop sliders */
	old_attacks = dir_attacks(king_sq, board.occupied_bb, DIR_A1H8) |
		dir_attacks(king_sq, board.occupied_bb, DIR_A8H1);
	if (old_attacks & MASK(sq))
	{
		attacks = dir_attacks(king_sq, occupied, DIR_A1H8) |
			dir_attacks(king_sq, occupied, DIR_A8H1);
		return (attacks & ~old_attacks & board.color_bb[COLOR_FLIP(color)] &
			(board.piece_bb[BISHOP] | board.piece_bb[QUEEN])) != (BITBOARD)0;
	}
	/* Rook sliders */
	old_attacks = dir_attacks(king_sq, board.occupied_bb, DIR_HORI) |
		dir_attacks(king_sq, board.occupied_bb, DIR_VERT);
	if (old_attacks & MASK(sq))
	{
		attacks = dir_attacks(king_sq, occupied, DIR_HORI) |
			dir_attacks(king_sq, occupied, DIR_VERT);
		return (attacks & ~old_attacks & board.color_bb[COLOR_FLIP(color)] &
			(board.piece_bb[ROOK] | board.piece_bb[QUEEN])) != (BITBOARD)0;
	}
	return FALSE;
}

/**
get_attack_bb_set():
For the current position, calculate various attack data that can be used for
move generation, search, and evaluation, such as pieces that check the king,
and pieces of the side to move that are pinned.
Created 090108; last modified 112308
**/
void get_attack_bb_set(BITBOARD *checks, BITBOARD not_pinned[],
	BITBOARD *all_pinned)
{
	BITBOARD pinned[4];
	BITBOARD attacks;
	BITBOARD old_attacks;
	BITBOARD potential_pinned;
	BITBOARD no_pinned_occ;
	BITBOARD pinners;
	BITBOARD pinned_orth;
	BITBOARD pinned_diag;
	BITBOARD pinner_piece[4] =
	{
		board.piece_bb[ROOK] | board.piece_bb[QUEEN],
		board.piece_bb[BISHOP] | board.piece_bb[QUEEN],
		board.piece_bb[BISHOP] | board.piece_bb[QUEEN],
		board.piece_bb[ROOK] | board.piece_bb[QUEEN],
	};
	COLOR color;
	SQUARE king_sq;
	SQUARE pinner_sq;
	DIRECTION dir;

	color = board.side_tm;
	king_sq = board.king_square[color];

	/* Calculate bishop attacks */
	attacks = attacks_bb(BISHOP, king_sq);
	*checks = attacks & pinner_piece[DIR_A1H8];
	/* Save the attacks */
	old_attacks = attacks;

	/* Calculate rook attacks */
	attacks = attacks_bb(ROOK, king_sq);
	*checks |= attacks & pinner_piece[DIR_HORI];
	/* Save the attacks */
	old_attacks |= attacks;

	/* Add in pawn and knight checks. */
	*checks |= pawn_caps_bb[color][king_sq] & board.piece_bb[PAWN];
	*checks |= knight_moves_bb[king_sq] & board.piece_bb[KNIGHT];
	/* Only the opposite side can check. Just do the AND once to save time. */
	*checks &= board.color_bb[COLOR_FLIP(color)];

	/* Calculate pinned pieces. All pinned pieces will be on a ray from the
		king_sq. */
	potential_pinned = old_attacks & board.occupied_bb;
	/* no_pinned_occ contains all squares with pieces that can't be pinned, so
		that we can detect attacks through those pieces to see if they're
		actually pinned. */
	no_pinned_occ = board.occupied_bb ^ potential_pinned;
	*all_pinned = 0;
	for (dir = DIR_HORI; dir <= DIR_VERT; dir++)
	{
		/* See if any sliders are behind the potentially pinned pieces. */
		pinners = dir_attacks(king_sq, no_pinned_occ, dir) &
			board.color_bb[COLOR_FLIP(color)] & pinner_piece[dir];

		/* Loop through pinners and add in the piece they are pinning. */
		pinned[dir] = 0;
		FOR_BB(pinner_sq, pinners)
			pinned[dir] |= (in_between_bb[king_sq][pinner_sq] &
				board.occupied_bb);

		/* Add in these pinned pieces king_sq the all_pinned set. */
		*all_pinned |= pinned[dir];
	}
	/* Now calculate the pieces that _aren't_ pinned, and can move in a certain
		direction. The piece can be pinned in the certain direction, but if it
		is a slider of the right type it can still move.
		E.g. WKe1,Bd2; BQa5 --> Bd2 can move in DIR_A8H1 */
	pinned_orth = pinned[DIR_VERT] | pinned[DIR_HORI];
	pinned_diag = pinned[DIR_A1H8] | pinned[DIR_A8H1];
	not_pinned[DIR_HORI] = ~(pinned[DIR_VERT] | pinned_diag);
	not_pinned[DIR_VERT] = ~(pinned[DIR_HORI] | pinned_diag);
	not_pinned[DIR_A1H8] = ~(pinned[DIR_A8H1] | pinned_orth);
	not_pinned[DIR_A8H1] = ~(pinned[DIR_A1H8] | pinned_orth);
}

/**
is_mate():
Checks if the current board position is checkmate.
Created 071308; last modified 071308
**/
BOOL is_mate(void)
{
	BITBOARD checkers;
	MOVE move_list[256];
	MOVE *last;

	checkers = check_squares();
	if (checkers)
	{
		last = generate_evasions(move_list, checkers);
		if (last - move_list == 0)
			return TRUE;
	}
	return FALSE;
}

/**
check_legality():
Goes through the move list and removes moves that are illegal.
Created 081906; last modified 091507
**/
void check_legality(MOVE *first, MOVE **last)
{
	while (first < *last)
	{
		if (!is_legal(*first))
			*first-- = *--*last;
		first++;
	}
}

/**
is_repetition():
Counts the number of times the current position has been played before in the
current game, to check for a 3-rep draw, or a 2-rep in the search. 
Created 090506; last modified 012808
**/
BOOL is_repetition(int limit)
{
	int reps;
	GAME_ENTRY *g;

	reps = 1;
	/* Repetitions can only be at least 4 plies back in the game tree, and
		they will only happen on plies that are an even distance away from
		the current position. Additionally, they can only go as far back as
		when the fifty move counter was last reset by an irreversible move. */
	for (g = board.game_entry - 4;
		g >= board.game_entry - board.fifty_count - 1 && g >= board.game_stack;
		g -= 2)
	{
		if (g->hashkey == board.hashkey)
		{
			reps++;
			if (reps >= limit)
				return TRUE;
		}
	}
	return FALSE;
}

/**
is_quiet():
This function determines whether the current board position is quiet.
It performs two quiescent searches, one for each side. The idea is that
if the position is quiet, the evaluations will match as neither side can
make a good move. This idea is from Tord Romstad.
Created 082008; last modified 082008
**/
BOOL is_quiet(void)
{
	VALUE value_1;
	VALUE value_2;

	search_call(&board.search_stack[0], TRUE, 0, 1, -MATE, MATE,
		board.move_stack, NODE_PV, SEARCH_RETURN);
	value_1 = -search(&board.search_stack[1]);

	make_move(NULL_MOVE);
	search_call(&board.search_stack[0], TRUE, 0, 1, -MATE, MATE,
		board.move_stack, NODE_PV, SEARCH_RETURN);
	value_2 = -search(&board.search_stack[1]);
	unmake_move();

	if (value_1 == value_2)
		return TRUE;
	return FALSE;
}

/**
generate_root_moves():
Sets up the root move list for either game play or searching.
Created 082606; last modified 082908
**/
void generate_root_moves(void)
{
	int next;
	static MOVE move_list[MAX_ROOT_MOVES];
	MOVE *last;

	last = generate_moves(move_list);
	check_legality(move_list, &last);
	for (next = 0; next < last - move_list; next++)
	{
		zct->root_move_list[next].move = move_list[next];
		zct->root_move_list[next].nodes = 0;
		zct->root_move_list[next].last_nodes = 0;
		zct->root_move_list[next].score = 0;
		zct->root_move_list[next].last_score = 0;
		zct->root_move_list[next].pv[0] = NO_MOVE;
	}
	zct->root_move_count = next;
}

/**
check_result():
Generates legal moves, checks if the game is over, and prints the result if
the game is over.
Created 081906; last modified 050108
**/
BOOL check_result(BOOL print_result)
{
	MOVE move_list[256];
	MOVE *last;
	RESULT new_result;
   
	new_result = INCOMPLETE;
	last = generate_moves(move_list);
	check_legality(move_list, &last);
	if (last - move_list == 0)
	{
		if (in_check())
			new_result = (board.side_tm == WHITE ? BLACK_MATES : WHITE_MATES);
		else
			new_result = STALEMATE;
	}
	else if (board.fifty_count >= 100)
		new_result = FIFTY_DRAW;
	else if (is_repetition(3))
		new_result = THREE_REP_DRAW;
	else if (!can_mate(WHITE) && !can_mate(BLACK))
		new_result = MATERIAL_DRAW;
	if (new_result != INCOMPLETE && print_result)
	{
		zct->game_result = new_result;
		print("%s\n", result_str[zct->game_result]);
		zct->game_over = TRUE;
	}
	return (new_result != INCOMPLETE);
}
