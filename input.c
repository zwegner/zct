/** ZCT/input.c--Created 070105 **/

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
#include <ctype.h>

#ifdef ZCT_POSIX
#	include <sys/time.h>
#elif defined (ZCT_WINDOWS)
#	include <windows.h>
#endif

/**
read_line():
Get an input line from stdin.
Created 070105; last modified 050208
**/
void read_line(void)
{
	if ((zct->input_buffer = getln(zct->input_stream)) == NULL)
	{
		/* We read in a source file, so return to stdin and read more. */
		if (zct->source)
		{
			fclose(zct->input_stream);
			zct->input_stream = stdin;
			zct->source = FALSE;
			prompt();
			read_line();
			return;
		}
		else
			exit(EXIT_SUCCESS);
	}
	if (!zct->source)
		fprintf(zct->log_stream, "%s\n", zct->input_buffer);
}

/**
getln():
Read in one line from the given file stream into an automatically allocated
string. This is because there is no portable function to do this! Non-C users
are invited to shut up. ;)
Created 050408; last modified 083108
**/
char *getln(FILE *stream)
{
	int c;
	int size;
	static char *buffer = NULL;

	if (buffer != NULL)
	{
		free(buffer);
		buffer = NULL;
	}
	size = 0;
	while ((c = fgetc(stream)) != EOF)
	{
		if (buffer == NULL)
			buffer = malloc(1);
		else
			buffer = realloc(buffer, size + 1);
		buffer[size] = c;
		size++;
		/* Commands are delimited by newlines and semicolons. */
		if (c == '\0' || c == '\n' || c == '\r' || c == ';')
		{
			/* We don't want to keep newlines and semicolons. */
			if (c != '\0')
				buffer[size - 1] = '\0';
			break;
		}
	}
	return buffer;
}

/**
input_move():
Parse the given string as a move.
Created 081105; last modified 050108
**/
int input_move(char *string, INPUT_MODE mode)
{
	char *c;
	char *promote_s = "  nNbBrRqQ  ";
	char *rank_s = "12345678";
	char *file_s = "abcdefgh";
	char *piece_s = "pPnNbBrRqQkK";
	char move_string[8];
	static MOVE move_list[256];
	static MOVE *last;
	int found;
	MOVE move;
	MOVE *next;
	PIECE promote;
	PIECE piece;
	SQUARE from;
	SQUARE to;
	SQ_FILE from_file;
	SQ_RANK from_rank;
	SQ_FILE to_file;
	SQ_RANK to_rank;
	static HASHKEY last_hashkey = (HASHKEY)0;

	promote = EMPTY;
	piece = EMPTY;
	from = OFF_BOARD;
	to = OFF_BOARD;
	from_file = -1;
	from_rank = -1;
	if ((c = strchr(string, ' ')) != NULL)
		*c = '\0';
	if (zct->notation == COORDINATE)
	{
		/* Some sanity checks on the move. */
		if (strlen(string) < 4 || strlen(string) > 5)
			goto end;
		if (string[0] < 'a' || string[0] > 'h')
			goto end;
		if (string[1] < '1' || string[1] > '8')
			goto end;
		if (string[2] < 'a' || string[2] > 'h')
			goto end;
		if (string[3] < '1' || string[3] > '8')
			goto end;
		/* The move is in a1a1 notation, we can just parse it like this. */
		from = SQ_FROM_RF(string[1] - '1', string[0] - 'a');
		to = SQ_FROM_RF(string[3] - '1', string[2] - 'a');
		/* Check for promotion. */
		if (string[4] != '\0')
			if (strchr("NBRQ", toupper((int)string[4])))
				promote = strchr(piece_char[WHITE],
					toupper((int)string[4])) - piece_char[WHITE];
	}
	else if (zct->notation == LSAN || zct->notation == SAN)
	{
		if (strlen(string) > 7)
			goto end;
		strcpy(move_string, string);
		piece = PAWN; /* Pawn is the default piece, no specifier is needed. */
		/* Pull off any characters that might be added on the move. */
		if ((c = strchr(move_string, '+')) != NULL)
			*c = '\0';
		else if ((c = strchr(move_string, '#')) != NULL)
			*c = '\0';
		/* Detect castling moves. */
		if (!strcmp(move_string, "o-o") ||
			!strcmp(move_string, "O-O") ||
			!strcmp(move_string, "0-0"))
		{
			piece = KING;
			to = board.side_tm == WHITE ? G1 : G8;
			from = board.side_tm == WHITE ? E1 : E8;
		}
		else if (!strcmp(move_string, "o-o-o") ||
			!strcmp(move_string, "O-O-O") ||
			!strcmp(move_string, "0-0-0"))
		{
			piece = KING;
			to = board.side_tm == WHITE ? C1 : C8;
			from = board.side_tm == WHITE ? E1 : E8;
		}
		if (strlen(move_string) < 2)
			goto end;
		/* Pull off the promotion piece, if present. */
		if ((c = strchr(move_string, '=')) != NULL)
			strcpy(c, c + 1);
		if ((c = strchr(promote_s, move_string[strlen(move_string) - 1])) != NULL)
		{
			promote = (c - promote_s) >> 1;
			move_string[strlen(move_string) - 1] = '\0';
		}
		if (strlen(move_string) < 2)
			goto end;
		/* The to square must be specified fully. */
		to_rank = move_string[strlen(move_string) - 1] - '1';
		to_file = move_string[strlen(move_string) - 2] - 'a';
		if (to_file < FILE_A || to_file > FILE_H || to_rank < RANK_1 || to_rank > RANK_8)
			goto end;
		to = SQ_FROM_RF(to_rank, to_file);
		move_string[strlen(move_string) - 2] = '\0';
		if (strlen(move_string) == 0)
			goto end;
		/* Pull off the capture specifier, if present. */
		if (move_string[strlen(move_string) - 1] == 'x')
			move_string[strlen(move_string) - 1] = '\0';
		if (strlen(move_string) == 0)
			goto end;
		/* Pull off any information about the from square present. */
		if ((c = strchr(rank_s, move_string[strlen(move_string) - 1])) != NULL)
		{
			from_rank = c - rank_s;
			move_string[strlen(move_string) - 1] = '\0';
		}
		if (strlen(move_string) == 0)
			goto end;
		if ((c = strchr(file_s, move_string[strlen(move_string) - 1])) != NULL)
		{
			from_file = c - file_s;
			move_string[strlen(move_string) - 1] = '\0';
		}
		if (strlen(move_string) == 0)
			goto end;
		if ((c = strchr(piece_s, move_string[strlen(move_string) - 1])) != NULL)
			piece = (c - piece_s) >> 1;
	}
end:
	/* See if the move is legal. */
	if (board.hashkey != last_hashkey)
	{
		last = generate_moves(move_list);
		check_legality(move_list, &last);
		last_hashkey = board.hashkey;
	}
	found = 0;
	move = NO_MOVE;
	for (next = move_list; next < last; next++)
	{
		if (piece != EMPTY && piece != board.piece[MOVE_FROM(*next)])
			continue;
		if (promote != EMPTY && promote != MOVE_PROMOTE(*next))
			continue;
		if (from != OFF_BOARD && from != MOVE_FROM(*next))
			continue;
		if (to != MOVE_TO(*next)) /* The TO square must be fully specified. */
			continue;
		if (from_rank != -1 && from_rank != RANK_OF(MOVE_FROM(*next)))
			continue;
		if (from_file != -1 && from_file != FILE_OF(MOVE_FROM(*next)))
			continue;
		found++;
		move = *next;
	}
	if (found == 1)
	{
		if (mode == INPUT_GET_MOVE)
			return move;
		else if (mode == INPUT_USER_MOVE)
			make_move(move);
		return TRUE;
	}
	else if (mode == INPUT_USER_MOVE)
	{
		if (zct->protocol != UCI)
		{
			if (found > 1)
				print("Illegal move (ambiguous): %s\n", string);
			else
				print("Illegal move (unrecognizable): %s\n", string);
		}
	}
	return FALSE;
}

/**
input_available():
This function checks during a search if there is input waiting that we need to process.
Created 102206; last modified 102206
**/
BOOL input_available(void)
{
#ifdef ZCT_POSIX
	int fd;
	fd_set f_s;
	struct timeval tv;

	if (zct->source)
		return FALSE;
	fd = fileno(zct->input_stream);
	FD_ZERO(&f_s);
	FD_SET(fd, &f_s);
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	select(1, &f_s, 0, 0, &tv);
	return FD_ISSET(fd, &f_s);
#elif defined(ZCT_WINDOWS)
	static BOOL initialized = FALSE;
	static BOOL is_pipe;
	static HANDLE input_handle;
	DWORD dummy;

	if (zct->source)
		return FALSE;
#	if defined(FILE_CNT)
	if (stdin->_cnt > 0)
		return TRUE;
#	endif

	if (!initialized)
	{
		initialized = TRUE;
		input_handle = GetStdHandle(STD_INPUT_HANDLE);
		/* We rely on this function failing or not to see if we're in console mode...
			Is Windows really that badly designed??? */
		is_pipe = (GetConsoleMode(input_handle, &dummy) == 0);
		if (!is_pipe)
		{
			SetConsoleMode(input_handle, dummy & ~(ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT));
			FlushConsoleInputBuffer(input_handle);
		}
	}
	if (is_pipe)
	{
		if (!PeekNamedPipe(input_handle, NULL, 0, NULL, &dummy, NULL))
			return TRUE;
		return (dummy != 0);
	}
	else
	{
		GetNumberOfConsoleInputEvents(input_handle, &dummy);
		return (dummy > 1);
	}
#endif /* ZCT_WINDOWS */
}
