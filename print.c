/** ZCT/print.c--Created 100306 **/

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
#include "smp.h"

/**
sprint():
Prints the arguments to the given string, sprintf style. These additional formats are supported:
B: BOARD *, prints board position
C: COLOR, prints color string
E: SEARCH_BLOCK *, prints iterative search stack element
F: BOARD *, prints FEN string
I: BITBOARD, prints a bitboard or two with modifier 'l'
L: BITBOARD, prints a 64-bit integer
M: MOVE, prints a move, or a zero-terminated move list with modifier 'l'
O: VALUE, prints a value converted into a win probability
P: PIECE, prints the name of a piece
R: SEARCH_STATE, prints an iterative search return point
S: SQUARE, prints a square
T: int, prints a time interval
V: VALUE, prints an evaluation
X: BITBOARD, prints a long long integer in hexadecimal.
Created 100306; last modified 011208
**/
void sprint(char *string, int max_length, char *text, ...)
{
	va_list args;
	char *temp;

	temp = malloc(max_length + 1);
	va_start(args, text);
	sprint_(temp, max_length, text, args);
	va_end(args);

	strcpy(string, temp);
	free(temp);
}

/**
print():
Prints the arguments to output_stream and log_stream, printf style.
Created 122205; last modified 031709
**/
void print(char *text, ...)
{
	char string[8192];
	va_list args;
	
	va_start(args, text);
	sprint_(string, sizeof(string), text, args);
	va_end(args);
	
	LOCK(smp_data->io_lock);
	fprintf(zct->output_stream, "%s", string);
	fprintf(zct->log_stream, "%s", string);
	UNLOCK(smp_data->io_lock);
}

/**
sprint_():
This is the internal function for sprint, that takes a va_list instead of
variable args.
Created 100306; last modified 031709
**/
void sprint_(char *string, int max_length, char *text, va_list args)
{
	int length;
	char *temp;
	char fmt[16];
	char *next;
	char *fmt_next;
	/* Argument temps */
	char f_char;
	char *f_str;
	int f_int;
	double f_double;
	void *f_voidp;
	/* ZCT arg temps */
	int f_time;
	BOARD *f_board;
	BITBOARD f_bitboard;
	BITBOARD f_bitboard_2;
	COLOR f_color;
	MOVE f_move;
	MOVE *f_movelist;
	PIECE f_piece;
	SEARCH_BLOCK *f_sb;
	SEARCH_STATE f_ss;
	SQUARE f_square;
	VALUE f_value;

	temp = malloc(max_length + 1);
	strcpy(string, "");
	length = 0;
	for (next = text; *next != '\0'; next++)
	{
		if (*next == '%')
		{
			fmt_next = fmt;
			*fmt_next++ = '%';
			while (*++next != '\0')
			{
				*fmt_next++ = *next;
				/* Format specifier. Capital letters are ZCT-specific. */
				if (strchr("BcCEFfiILMOpPRsSTuVxX%", *next))
					break;
			}
			*fmt_next = '\0';
			
			if (*next == '\0')
				fatal_error("fatal error: invalid format specifier, "
					"fmt=%s\n", fmt);

			switch (*(fmt_next - 1))
			{
				/* Standard specifiers */
				case 'c':
					f_char = va_arg(args, int);
					sprintf(temp, fmt, f_char);
					break;
				case 's':
					if (strchr(fmt, '*'))
					{
						f_int = va_arg(args, int);
						f_str = va_arg(args, char *);
						sprintf(temp, fmt, f_int, f_str);
					}
					else
					{
						f_str = va_arg(args, char *);
						sprintf(temp, fmt, f_str);
					}
					break;
				case 'i': case 'u': case 'x':
					f_int = va_arg(args, int);
					sprintf(temp, fmt, f_int);
					break;
				case 'f':
					f_double = va_arg(args, double);
					sprintf(temp, fmt, f_double);
					break;
				case 'p':
					f_voidp = va_arg(args, void *);
					sprintf(temp, fmt, f_voidp);
					break;
				case '%':
					strcpy(temp, "%");
					break;
				/* ZCT-specific specifiers */
				case 'B':
					f_board = va_arg(args, BOARD *);
					strcpy(temp, board_string(f_board));
					break;
				case 'C':
					f_color = va_arg(args, int);
					strcpy(temp, color_str[f_color]);
					break;
				case 'E':
					f_sb = va_arg(args, SEARCH_BLOCK *);
					strcpy(temp, search_block_string(f_sb));
					break;
				case 'F':
					f_board = va_arg(args, BOARD *);
					strcpy(temp, fen_string(f_board));
					break;
				case 'I':
					if (strchr(fmt, 'l'))
					{
						f_bitboard = va_arg(args, BITBOARD);
						f_bitboard_2 = va_arg(args, BITBOARD);
						strcpy(temp, two_bitboards_string(f_bitboard,
							f_bitboard_2));
					}
					else
					{
						f_bitboard = va_arg(args, BITBOARD);
						strcpy(temp, bitboard_string(f_bitboard));
					}
					break;
				case 'L':
					f_bitboard = va_arg(args, BITBOARD);
					/* Replace the format specifier with the standard. */
					*(fmt_next - 1) = '\0';
					strcat(fmt, I64);
					sprintf(temp, fmt, f_bitboard);
					break;
				case 'M':
					if (strchr(fmt, 'l'))
					{
						*(fmt_next - 1) = '\0';
						*strchr(fmt, 'l') = 's';
						f_movelist = va_arg(args, MOVE *);
						sprintf(temp, fmt, pv_string(f_movelist));
					}
					else
					{
						*(fmt_next - 1) = 's';
						f_move = va_arg(args, MOVE);
						sprintf(temp, fmt, move_string(f_move));
					}
					break;
				case 'O':
					/* VALUE is promoted to int through a va_list */
					f_value = (VALUE)va_arg(args, int);
					*(fmt_next - 1) = 's';
					sprintf(temp, fmt, value_prob_string(f_value));
					break;
				case 'P':
					f_piece = va_arg(args, int);
					strcpy(temp, piece_str[f_piece]);
					break;
				case 'R':
					f_ss = va_arg(args, SEARCH_STATE);
					strcpy(temp, search_state_string(f_ss));
					break;
				case 'S':
					f_square = va_arg(args, SQUARE);
					strcpy(temp, square_string(f_square));
					break;
				case 'T':
					f_time = va_arg(args, int);
					*(fmt_next - 1) = 's';
					sprintf(temp, fmt, time_string(f_time));
					break;
				case 'V':
					/* VALUE is promoted to int through a va_list */
					f_value = (VALUE)va_arg(args, int);
					*(fmt_next - 1) = 's';
					sprintf(temp, fmt, value_string(f_value));
					break;
				case 'X':
					f_bitboard = va_arg(args, BITBOARD);
					strcpy(temp, bitboard_hex_string(f_bitboard));
					break;
				default:
					fatal_error("fatal error: print: bad format specifier, "
						"spec='%c'\n", *(fmt_next - 1));
					break;
			}
		}
		else
		{
			temp[0] = *next;
			temp[1] = '\0';
		}
		if (length + strlen(temp) < max_length)
		{
			strcat(string, temp);
			length = strlen(string);
		}
		else
		{
			fatal_error("fatal error: print consumes too many characters, "
				"fmt=%s, length=%i, max_length=%i\n", text,
				length + strlen(temp), max_length);
		}
	}
	free(temp);
}
