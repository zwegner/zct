/** ZCT/book.h--Created 091507 **/

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
#include "pgn.h"

#ifndef BOOK_H
#define BOOK_H

/* The header structure for a book file. This is written at the beginning
	of every book file. */
typedef struct
{
	char zctb[4]; /* This is a string to make sure we are reading a valid ZCT
					 book. It just contains the letters "ZCTB". */
	unsigned short major; /* Version numbers */
	unsigned short minor;
	HASHKEY checksum;	/* A checksum of the book for corrupted files. Basically
						every hashkey in the book is XORed together. */
} BOOK_HEADER;

/* Book flags. These are used for books with markup such as Nxe5!! */
#define BOOK_FLAG_QQ			(0x01) /* ?? */
#define BOOK_FLAG_Q				(0x02) /* ? */
#define BOOK_FLAG_EQ			(0x04) /* = */
#define BOOK_FLAG_X				(0x08) /* ! */
#define BOOK_FLAG_XX			(0x10) /* !! */
#define BOOK_FLAG_XQ			(0x20) /* !? */
#define BOOK_FLAG_QX			(0x40) /* ?! */
typedef unsigned char BOOK_FLAGS;

/* The data structure for a book position. The hashkey is a mixture of the
	current position and the position after the book move. */
typedef struct
{
	HASHKEY hashkey;
	unsigned short wins;
	unsigned short losses;
	unsigned short draws;
	BOOK_FLAGS flags;
	unsigned char learn;
} BOOK_POSITION;

typedef struct
{
	BOOK_POSITION *book_buffer;
	int buffer_size;
	int depth;
} BOOK_POS_INFO;

#define BOOK_UPPER_MASK			(0xFFFF000000000000ull)
#define BOOK_LOWER_MASK			(0x0000FFFFFFFFFFFFull)

extern FILE *book_file;

BOOL check_header(FILE *file);
BOOK_POSITION *bytes_to_book(unsigned char *bytes);
unsigned char *book_to_bytes(BOOK_POSITION *book_position);
BOOK_HEADER *bytes_to_header(unsigned char *bytes);
unsigned char *header_to_bytes(BOOK_HEADER *header);
BOOL book_pos_func(void *pos_arg, POS_DATA *pos_data);
BOOK_POSITION *hash_book(HASHKEY hashkey, BOOK_POSITION *book_buffer,
	int buffer_size);

#endif /* BOOK_H */
