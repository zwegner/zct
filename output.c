/** ZCT/output.c--Created 060205 **/

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
#include "smp.h"

/**
move_string():
Returns the string representation of the given move.
Created 070105; last modified 052508
**/
char *move_string(MOVE move)
{
	static char str[32];
	char *next;
	PIECE piece;
	MOVE move_list[256];
	MOVE *last;
	MOVE *next_move;
	int total;
	int total_r;
	int total_f;

	next = str;
	if (move == NO_MOVE)
		strcpy(str, "<no move>");
	else if (move == NULL_MOVE)
		strcpy(str, "NULL");
	/* Check the notation system. */
	else if (zct->notation == COORDINATE)
	{
		*next++ = SQ_FILE_CHAR(MOVE_FROM(move));
		*next++ = SQ_RANK_CHAR(MOVE_FROM(move));
		*next++ = SQ_FILE_CHAR(MOVE_TO(move));
		*next++ = SQ_RANK_CHAR(MOVE_TO(move));
		if (MOVE_PROMOTE(move))
			*next++ = piece_char[WHITE][MOVE_PROMOTE(move)];
		*next = '\0';
	}
	else if (zct->notation == LSAN)
	{
		piece = board.piece[MOVE_FROM(move)];
		if (piece != PAWN)
			*next++ = piece_char[WHITE][piece];
		*next++ = SQ_FILE_CHAR(MOVE_FROM(move));
		*next++ = SQ_RANK_CHAR(MOVE_FROM(move));
		if (board.color[MOVE_TO(move)] != EMPTY ||
			(piece == PAWN && MOVE_TO(move) == board.ep_square))
			*next++ = 'x';
		else
			*next++ = '-';
		*next++ = SQ_FILE_CHAR(MOVE_TO(move));
		*next++ = SQ_RANK_CHAR(MOVE_TO(move));
		if (MOVE_PROMOTE(move))
			*next++ = piece_char[WHITE][MOVE_PROMOTE(move)];
		*next = '\0';
	}
	else if (zct->notation == SAN)
	{
		piece = board.piece[MOVE_FROM(move)];
		/* Pawn moves are different in SAN. */
		if (piece == PAWN)
		{
			/* Captures need the file to disambiguate from. We need to detect
				en passant captures as well. */
			if (board.color[MOVE_TO(move)] != EMPTY ||
				MOVE_TO(move) == board.ep_square)
			{
				*next++ = SQ_FILE_CHAR(MOVE_FROM(move));
				*next++ = 'x';
			}
			/* TO square */
			*next++ = SQ_FILE_CHAR(MOVE_TO(move));
			*next++ = SQ_RANK_CHAR(MOVE_TO(move));
			/* Promotion letter: N, B, R, Q */
			if (MOVE_PROMOTE(move))
			{
				*next++ = '=';
				*next++ = piece_char[WHITE][MOVE_PROMOTE(move)];
			}
			*next = '\0';
		}
		/* Castling moves get detected indirectly, by the king move. */
		else if (piece == KING && ABS(MOVE_TO(move) - MOVE_FROM(move)) == 2)
		{
			if (MOVE_TO(move) > MOVE_FROM(move))
				return "O-O";
			else
				return "O-O-O";
		}
		else
		{
			/* Start off with a piece specifier. */
			*next++ = piece_char[WHITE][piece];

			/* Count the number of moves that we need to disambiguate from.
				We count the number of moves from the same file and rank, as
				extra specifiers of the rank and/or file will need to be added
				in that case. */
			total = 0;
			total_r = 0;
			total_f = 0;
			last = generate_moves(move_list);
			check_legality(move_list, &last);
			for (next_move = move_list; next_move < last; next_move++)
			{
				if (board.piece[MOVE_FROM(*next_move)] ==
					piece && MOVE_TO(*next_move) == MOVE_TO(move))
				{
					total++;
					if (RANK_OF(MOVE_FROM(*next_move)) ==
						RANK_OF(MOVE_FROM(move)))
						total_r++;
					if (FILE_OF(MOVE_FROM(*next_move)) ==
						FILE_OF(MOVE_FROM(move)))
						total_f++;
				}
			}
			/* If more than one move was found, add extra specifiers to make
				the move unambiguous. */
			if (total > 1)
			{
				/* This looks backwards, but it's not. If there are multiple
					moves coming from the same rank, the file will be different,
					so use that to disambiguate. */
				if (total_r > 1)
					*next++ = SQ_FILE_CHAR(MOVE_FROM(move));
				if (total_f > 1)
					*next++ = SQ_RANK_CHAR(MOVE_FROM(move));
				/* Minimum case: neither rank nor file matches any other move,
					so just use the file. */
				if (total_r == 1 && total_f == 1)
					*next++ = SQ_FILE_CHAR(MOVE_FROM(move));
			}
			/* Capture specifier. */
			if (board.color[MOVE_TO(move)] != EMPTY)
				*next++ = 'x';
			/* TO square. */
			*next++ = SQ_FILE_CHAR(MOVE_TO(move));
			*next++ = SQ_RANK_CHAR(MOVE_TO(move));
		}
		/* This shouldn't ever happen! But it does... */
		if (!make_move(move))
		{
			sprint(str, sizeof(str), "ILL(%S%S,%i)", MOVE_FROM(move),
				MOVE_TO(move), MOVE_SCORE(move));
			return str;
		}
		if (in_check())
		{
			last = generate_evasions(move_list, check_squares());
			if (last - move_list == 0)
				*next++ = '#';
			else
				*next++ = '+';
		}
		unmake_move();
		*next = '\0';
	}
	return str;
}

/**
board_string():
Returns the given board position as a string.
Created 060205; last modified 100306
**/
char *board_string(BOARD *board)
{
	char c;
	char temp[8];
	static char str[256];
	SQ_RANK r;
	SQ_FILE f;
	SQUARE s;

	strcpy(str, "  +-----------------+");
	if (board->side_tm == BLACK)
		strcat(str, " <-");
	strcat(str, "\n");
	for (r = RANK_8; r >= RANK_1; r--)
	{
		sprintf(temp, "%i |", r + 1);
		strcat(str, temp);
		for (f = FILE_A; f <= FILE_H; f++)
		{
			s = SQ_FROM_RF(r, f);
			if (board->color[s] != EMPTY)
				c = piece_char[board->color[s]][board->piece[s]];
			else if (SQ_COLOR(s) == WHITE)
				c = ' ';
			else
				c = '.';
			sprintf(temp, " %c", c);
			strcat(str, temp);
		}
		strcat(str, " |\n");
	}

	strcat(str, "  +-----------------+");
	if (board->side_tm == WHITE)
		strcat(str, " <-");
	strcat(str, "\n    a b c d e f g h\n");
	return str;
}

/**
fen_string():
Return the Forsythe-Edwards notation of a position.
Created 071708; last modified 071708
**/
char *fen_string(BOARD *board)
{
	static char str[256];
	char *s;
	SQUARE square;
	SQ_RANK rank;
	SQ_FILE file;
	int empty_count;

	s = str;
	/* Main pieces on board part */
	for (rank = RANK_8; rank >= RANK_1; rank--)
	{
		empty_count = 0;
		for (file = FILE_A; file <= FILE_H; file++)
		{
			square = SQ_FROM_RF(rank, file);
			if (board->color[square] != EMPTY)
			{
				if (empty_count > 0)
				{
					*s++ = empty_count + '0';
					empty_count = 0;
				}
				*s++ = piece_char[board->color[square]][board->piece[square]];
			}
			else
				empty_count++;
		}
		if (empty_count > 0)
			*s++ = empty_count + '0';
		if (rank != RANK_1)
			*s++ = '/';
	}
	*s++ = ' ';
	/* Side to move */
	*s++ = board->side_tm == WHITE ? 'w' : 'b';
	*s++ = ' ';
	/* Castling rights */
	if (CAN_CASTLE(board->castle_rights, WHITE) ||
		CAN_CASTLE(board->castle_rights, BLACK))
	{
		if (CAN_CASTLE_KS(board->castle_rights, WHITE))
			*s++ = 'K';
		if (CAN_CASTLE_QS(board->castle_rights, WHITE))
			*s++ = 'Q';
		if (CAN_CASTLE_KS(board->castle_rights, BLACK))
			*s++ = 'k';
		if (CAN_CASTLE_QS(board->castle_rights, BLACK))
			*s++ = 'q';
	}
	else
		*s++ = '-';
	*s++ = ' ';
	/* EP square */
	if (board->ep_square != OFF_BOARD)
	{
		sprint(s, 2, "%S", board->ep_square);
		s += 2;
	}
	else
		*s++ = '-';
	/* Now print in the move number and fifty move count. */
	sprint(s, sizeof(str) - (s - str), " %i %i", board->fifty_count,
		board->move_number);

	return str;
}

/**
bitboard_string():
Returns the string representaion of the given bitboard.
Created 070105; last modified 090106
**/
char *bitboard_string(BITBOARD bitboard)
{
	char c;
	char temp[8];
	static char str[256];
	SQ_RANK r;
	SQ_FILE f;
	SQUARE s;

	strcpy(str, "");
	for (r = RANK_8; r >= RANK_1; r--)
	{
		sprintf(temp, " %i  ", r + 1);
		strcat(str, temp);
		for (f = FILE_A; f <= FILE_H; f++)
		{
			s = SQ_FROM_RF(r, f);
			if (bitboard & MASK(s))
				c = '1';
			else
				c = '0';
			sprintf(temp, "%c ", c);
			strcat(str, temp);
		}
		strcat(str, "\n");
	}
	strcat(str, "    a b c d e f g h\n");
	return str;
}

/**
two_bitboards_string():
Returns the string representation of two bitboards side-by-side.
Created 071505; last modified 100306
**/
char *two_bitboards_string(BITBOARD bitboard_1, BITBOARD bitboard_2)
{
	char c;
	char temp[8];
	static char str[512];
	SQ_RANK r;
	SQ_FILE f;
	SQUARE s;

	strcpy(str, "");
	for (r = RANK_8; r >= RANK_1; r--)
	{
		sprintf(temp, " %i  ", r + 1);
		strcat(str, temp);
		for (f = FILE_A; f <= FILE_H; f++)
		{
			s = SQ_FROM_RF(r, f);
			if (bitboard_1 & MASK(s))
				c = '1';
			else
				c = '0';
			sprintf(temp, "%c ", c);
			strcat(str, temp);
		}
		strcat(str, "\t");
		for (f = FILE_A; f <= FILE_H; f++)
		{
			s = SQ_FROM_RF(r, f);
			if (bitboard_2 & MASK(s))
				c = '1';
			else
				c = '0';
			sprintf(temp, "%c ", c);
			strcat(str, temp);
		}
		strcat(str, "\n");
	}
	strcat(str, "    A B C D E F G H \tA B C D E F G H\n");
	return str;
}

/**
bitboard_hex_string():
Returns the string representation of the given integer in hexadecimal.
Created 100207; last modified 100207
**/
char *bitboard_hex_string(BITBOARD bitboard)
{
	static char str[32];
	sprintf(str, "%#010x%08x", (unsigned int)(bitboard >> 32),
		(unsigned int)bitboard);

	return str;
}

/**
pv_string():
Returns the string representation of the given principle variation.
Created 030306; last modified 042808
**/
char *pv_string(MOVE *pv_start)
{
	static char str[1024];
	char next[16];
	BOOL make_pv_moves;
	GAME_ENTRY *start = board.game_entry;
	MOVE *move;
	SEARCH_BLOCK sb;
 
	ASSERT(pv_start != NULL);

	/* Decide whether we want to actually make the PV moves on our internal
		board. */
	if (zct->notation == COORDINATE)
		make_pv_moves = FALSE;
	else
		make_pv_moves = TRUE;

	/* Black-first PVs get a special move number indicator. */
	if (make_pv_moves && board.side_tm == BLACK)
		sprintf(str, "%i... ", board.move_number);
	else
		strcpy(str, "");

	/* Now loop through the moves one by one and cat the strings together. */
	move = pv_start;
	while (*move)
	{
		/* Move number indicator */
		if (make_pv_moves && board.side_tm == WHITE)
		{
			sprint(next, sizeof(next), "%i. ", board.move_number);
			strcat(str, next);
		}
		/* Cat the move. */
		sprint(next, sizeof(next), "%M", *move);
		strcat(str, next);
		if (make_pv_moves)
		{
			if (!make_move(*move))
				break;
		}

		move++;
		if (*move == NO_MOVE)
			break;
		strcat(str, " ");
	}
	/* Fill in the PV with hash table information, if we have been making
		the PV moves on the internal board up to here. */
	if (make_pv_moves)
	{
		/* Set up a dummy search block to use in the probe. */
		sb.hash_move = NO_MOVE;
		sb.depth = 0;
		hash_probe(&sb, FALSE);
		do
		{
			/* Make sure this is a good move, because hash collisions can cause
				bad moves. */
			if (sb.hash_move == NO_MOVE || !move_is_valid(sb.hash_move) ||
				!is_legal(sb.hash_move))
				break;
			strcat(str, " ");
			/* Move number indicator */
			if (board.side_tm == WHITE)
			{
				sprint(next, sizeof(next), "%i. ", board.move_number);
				strcat(str, next);
			}
			/* Cat the move string, adding a 'H' to indicate a hash move. */
			sprint(next, sizeof(next), "%MH", sb.hash_move);
			strcat(str, next);
			make_move(sb.hash_move);
			/* Probe the hash for the next loop. */
			sb.hash_move = NO_MOVE;
			hash_probe(&sb, FALSE);
			/* Make sure the game isn't over. A repetition could cause an
				infinite loop, at least till it segfaults. */
		} while (!is_repetition(2) && !check_result(FALSE) &&
			sb.hash_move != NO_MOVE);
	}
	/* Go back to the starting position. */
	if (make_pv_moves)
		while (board.game_entry > start)
			unmake_move();

	return str;
}

/**
square_string():
Returns the string representation of the given square.
Created 100306; last modified 100306
**/
char *square_string(SQUARE square)
{
	static char str[4];

	str[0] = FILE_OF(square) + 'A';
	str[1] = RANK_OF(square) + '1';
	str[2] = '\0';
	return str;
}

/**
time_string():
Returns the string representation of the given time interval.
Created 090206; last modified 102306
**/
char *time_string(int time)
{
	static char str[16];

	if (time < 60000) /* seconds */
		sprintf(str, "%.3f", (float)time / 1000);
	else if (time < 3600000) /* minutes */
		sprintf(str, "%i:%02i", time / 60000, (time % 60000) / 1000);
	else /* hours */
		sprintf(str, "%i:%02i:%02i", time / 3600000, (time % 3600000) / 60000,
			(time % 60000) / 1000);
	return str;
}

/**
value_string():
Returns the string representation of the given search value.
Created 081406; last modified 102706
**/
char *value_string(VALUE value)
{
	static char str[16];

	if (zct->protocol == UCI)
	{
		if (value >= MATE - MAX_PLY)
			sprintf(str, "mate %i", (MATE - value + 1) >> 1);
		else if (value <= -MATE + MAX_PLY)
			sprintf(str, "mate -%i", (value + MATE) >> 1);
		else
			sprintf(str, "cp %i", value);
	}
	else
	{
		if (value >= MATE - MAX_PLY)
			sprintf(str, "+#%i", (MATE - value + 1) >> 1);
		else if (value <= -MATE + MAX_PLY)
			sprintf(str, "-#%i", (value + MATE) >> 1);
		else
			sprintf(str, "%+.2f", (float)value / 100);
	}
	return str;
}

/**
value_prob_string():
Returns the string representation of the given search value, after being
converted into an expected win probability.
Created 082408; last modified 082408
**/
char *value_prob_string(VALUE value)
{
	static char str[16];

	sprintf(str, "%.3f", (float)value_to_prob(value));
	return str;
}

/**
search_block_string():
Returns the string representation of the SEARCH_BLOCK type that is used during
iterative searching. This is primarily useful during debugging.
Created 081907; last modified 092307
**/
char *search_block_string(SEARCH_BLOCK *sb)
{
	static char str[256];

	sprint(str, sizeof(str), "sb(id=%i ply=%i depth=%.2f moves=%i/%i "
		"alpha=%V beta=%V rp=%R)", sb->id, sb->ply, (float)sb->depth / PLY,
		sb->moves, sb->last_move - sb->next_move, sb->alpha, sb->beta,
		sb->search_state);

	return str;
}

/**
search_state_string():
Returns the string representation of a search state type. This is used in
debugging the iterative search.
Created 092307; last modified 011808
**/
char *search_state_string(SEARCH_STATE ss)
{
	static char search_state_str[][32] =
	{
		"SEARCH_START", "SEARCH_NULL_1", "SEARCH_IID", "SEARCH_JOIN",
		"SEARCH_1", "SEARCH_2", "SEARCH_3", "SEARCH_4", "QSEARCH_START",
		"QSEARCH_1", "SEARCH_WAIT", "SEARCH_CHILD_RETURN", "SEARCH_RETURN"
	};

	return search_state_str[ss];
}

/**
display_search_line():
Prints all search related information for a certain ply in iterative deepening, including PV changes etc.
Created 081206; last modified 071608
**/
void display_search_line(BOOL final, MOVE *pv, VALUE value)
{
	static char s[2048];

	/* Avoid displaying a "mate in 0" score when there is no score for this
		iteration yet. */
	if (value == -MATE && zct->current_iteration > 1)
		value = zct->best_score_by_depth[zct->current_iteration - 1];
	/* Only output if we've searched sufficient nodes. */
	sum_counters();
	if (zct->post && zct->nodes + zct->q_nodes > zct->output_limit)
	{
		if (zct->protocol == XBOARD || zct->protocol == ANALYSIS_X)
			print("%i %+i %i %L %lM\n", zct->current_iteration, value,
				time_used() / 10, zct->nodes + zct->q_nodes, pv);
		else if (zct->protocol == UCI)
		{
			print("info depth %i seldepth %i score %V time %i "
				"nodes %L pv %lM\n", zct->current_iteration,
				zct->max_depth_reached, value, time_used(),
				zct->nodes + zct->q_nodes, pv);
		}
		else
		{
			if (final)
				sprint(s, sizeof(s), "[%2i/%2i]", zct->current_iteration,
					zct->max_depth_reached);
			else
				sprint(s, sizeof(s), " %2i/%2i ", zct->current_iteration,
					zct->max_depth_reached);

			sprint(s, sizeof(s), "%s %9T %6V   %lM (%L)", s,
				time_used(), value, pv,
				zct->nodes + zct->q_nodes);

			/* Modify the output to wrap prettily for 80 columns. */
			line_wrap(s, sizeof(s), 27);

			print("%s\n", s);
		}
	}
}

/**
display_search_header():
At the beginning of a search, if we're in console mode, print out a little
header to indicate what each column is.
Created 111608; last modified 111608
**/
void display_search_header(void)
{
	if (zct->post && zct->protocol != XBOARD && zct->protocol != ANALYSIS_X &&
		zct->protocol != UCI)
		print("Depth        Time  Score   PV (nodes)\n");
}

/**
line_wrap():
Given a string of output in string, with a total buffer size of size, wrap the
string so that it looks nice in the UI. Column is the starting column that the
string will start on after wrapping.
Created 111408; last modified 031709
**/
void line_wrap(char *string, int size, int column)
{
	char *temp;
	char *sub;
	char *c;

	/* Create a temporary string of the appropriate size. */
	temp = malloc(size);

	/* Run through the string until the last part of the string is less than
		the width. */
	for (sub = string; strlen(sub) > 80; )
	{
		/* Start a little ways back, and find the last space in the
			lline. */
		for (c = sub + 78; c > sub && *c != ' '; c--)
			;

		/* This won't happen: 78 characters in a row without spaces.
			But just to be super safe... */
		if (c <= sub)
			break;

		/* Sub should now hold the space we are breaking the line at. */
		sub = c;

		/* Copy the part of the old line we are moving to the next line,
			so we can throw some spaces before it. */
		strcpy(temp, sub);
		sprint(sub, size - (sub - string), "\n%*s %s", column, "", temp);

		/* Sub now contains a newline character (the line break), so advance
			one character to get to the first character on the next line. */
		sub++;
	}

	/* Clean up the temporary string. */
	free(temp);
}
