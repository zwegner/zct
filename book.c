/** ZCT/book.c--Created 091507 **/

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
#include "book.h"
#include "pgn.h"

FILE *book_file = NULL;

/**
book_update():
This is used to update an opening book from a given PGN file.
Created 091507; last modified 071708
**/
void book_update(char *pgn_file_name, char *book_file_name, int width,
	int depth, int win_percent)
{
	int buffer_size;
	int d;
	int game;
	int game_count;
	unsigned char bytes[sizeof(BOOK_POSITION)];
	BOOK_POS_INFO bpi;
	BOOK_POSITION *book_buffer;
	BOOK_POSITION *bp;
	BOOK_POSITION *new_bp;
	BOOK_HEADER header;

	/* Open the pgn file. We use the game count to allocate a book buffer. */
	game_count = pgn_open(pgn_file_name);
	if (game_count <= 0)
	{
		if (game_count == 0)
			print("%s: no games found.\n", pgn_file_name);
		return;
	}

	/* Close the book we were using. */
	if (book_file != NULL)
		fclose(book_file);
	/* See if the book exists already. If so, read the data in to update it. */
	book_file = fopen(book_file_name, "r");
	if (book_file != NULL)
	{
		/* Count the number of book positions there are. */
		fseek(book_file, 0, SEEK_END);
		buffer_size = (ftell(book_file) / sizeof(BOOK_POSITION)) +
			(game_count * depth);
	}
	/* Otherwise, we use the game count of the PGN and the depth to find the
		maximum number of entries needed for the book buffer. */
	else
		buffer_size = game_count * depth;

	/* Allocate the buffer. */
	print("Allocating book buffer of %i positions...\n", buffer_size);
	book_buffer = (BOOK_POSITION *)calloc(buffer_size, sizeof(BOOK_POSITION));
	if (book_buffer == NULL)
	{
		print("Failed to allocate book_buffer.\n");
		if (book_file != NULL)
			fclose(book_file);
		return;
	}
	/* Read in any old positions to the buffer. */
	if (book_file != NULL)
	{
		fseek(book_file, 0, SEEK_SET);
		while (!feof(book_file))
		{
			if (fread(bytes, sizeof(bytes), 1, book_file))
			{
				bp = bytes_to_book(bytes);
				new_bp = hash_book(bp->hashkey, book_buffer, buffer_size);
				if (new_bp != NULL)
					*new_bp = *bp;
			}
			else
				break;
		}
		fclose(book_file);
	}

	/* Open the book file. We use the "truncate" method here so the old data
		is erased. This is safe because we already have all of the old data
		in memory. */
	book_file = fopen(book_file_name, "wb");
	if (book_file == NULL)
	{
		print("%s: could not open.\n", book_file_name);
		free(book_buffer);
		return;
	}

	/* Set up a struct to pass to the book making function with all of the
		book making parameters. */
	bpi.book_buffer = book_buffer;
	bpi.buffer_size = buffer_size;
	bpi.depth = depth;
	/* Run through the PGN and parse each game. Aren't POS_FUNCs great? */
	for (game = 1; game <= game_count; game++)
		pgn_load(game, book_pos_func, &bpi);

	/* Write a placeholder for the book header. Once we checksum the book
		while writing it, we'll come back and overwrite it. */
	header.zctb[0] = 'Z';
	header.zctb[1] = 'C';
	header.zctb[2] = 'T';
	header.zctb[3] = 'B';
	header.major = 3;
	header.minor = ZCT_VERSION;
	header.checksum = 0;
	fwrite(header_to_bytes(&header), sizeof(BOOK_HEADER), 1, book_file);
	
	/* Dump the book buffer into the book file. */
	print("Dumping positions to book... ");
	d = 0;
	/* Loop through the buffer and take the games that have a sufficient
		number of games and win percentage. */
	for (bp = book_buffer; bp < book_buffer + buffer_size; bp++)
	{
		/* Only write the position if there are enough games, and the score is
			high enough. */
		if (bp->hashkey != 0 && (bp->wins + bp->losses + bp->draws) >= width &&
			100 * (bp->wins + bp->draws / 2) / (bp->wins + bp->losses +
			bp->draws) >= win_percent)
		{
			fwrite(book_to_bytes(bp), sizeof(BOOK_POSITION), 1, book_file);
			header.checksum ^= bp->hashkey;
			d++;
		}
	}
	/* Overwrite the header now that we have a correct checksum. */
	fseek(book_file, 0, SEEK_SET);
	fwrite(header_to_bytes(&header), sizeof(BOOK_HEADER), 1, book_file);
	/* Clean up... */
	fclose(book_file);
	book_file = NULL;
	free(book_buffer);
	print("%i positions written.\n", d);
	/* Verify the book. */
	book_file = fopen(book_file_name, "rb");
	if (book_file != NULL)
	{
		if (!check_header(book_file))
		{
			print("WARNING! Checksum test failed. Book is corrupted and "
				"will not load!\n");
		}
		fclose(book_file);
		book_file = NULL;
	}
}

/**
book_load():
Loads an opening book given the file name.
Created 091507; last modified 071708
**/
void book_load(char *file_name)
{
	print("Trying opening book %s... ", file_name);
	if (book_file != NULL)
		fclose(book_file);
	book_file = fopen(file_name, "rb");

	if (book_file == NULL)
		print("file not found.\n");
	else
	{
		if (check_header(book_file))
			print("successful.\n");
		else
		{
			print("book failed checksum.\n");
			fclose(book_file);
			book_file = NULL;
		}
	}
}

/**
check_header():
Read the header structure at the beginning of a book, and verify that this is
a valid book, and compatible with the current ZCT.
Created 071708; last modified 071708
**/
BOOL check_header(FILE *file)
{
	BOOK_HEADER *header;
	HASHKEY checksum;
	BOOK_POSITION *bp;
	unsigned char hbytes[sizeof(BOOK_HEADER)];
	unsigned char bytes[sizeof(BOOK_POSITION)];

	/* Each book has to at least have a header... */
	fseek(file, 0, SEEK_SET);
	if (!fread(hbytes, sizeof(hbytes), 1, file))
		return FALSE;
	header = bytes_to_header(hbytes);

	if (strncmp(header->zctb, "ZCTB", 4))
		return FALSE;
	/* This book file is compatible with versions >= 0.3.2453. */
	if (header->major < 3)
		return FALSE;
	if (header->minor < 2453)
		return FALSE;
	/* Now the long part: checksum the whole book. */
	checksum = 0;
	while (!feof(file))
	{
		if (fread(bytes, sizeof(bytes), 1, file))
		{
			bp = bytes_to_book(bytes);
			checksum ^= bp->hashkey;
		}
		else
			break;
	}
	if (checksum != header->checksum)
		return FALSE;
	return TRUE;
}

/**
book_probe():
Probes the book for the current position. Suitable book moves are found, and a
random move is made from these.
Created 091507; last modified 050408
**/
BOOL book_probe(void)
{
	int book_position_count;
	int total_games;
	int random;
	BOOK_POSITION *bp;
	BOOK_POSITION book_positions[256];
	HASHKEY hashkey;
	ROOT_MOVE *root_move;
	unsigned char bytes[sizeof(BOOK_POSITION)];

	if (book_file == NULL)
		return FALSE;
	/* Probe the book to find all position with the same upper bits, i.e. from
		the same starting position. */
	/* TODO: Order the book file, as this is pretty inefficient. Not that it
		matters, as it will complete in like 1 ms. */
	book_position_count = 0;
	rewind(book_file);
	hashkey = board.hashkey & BOOK_UPPER_MASK;
	while (!feof(book_file))
	{
		if (fread(bytes, sizeof(bytes), 1, book_file))
		{
			bp = bytes_to_book(bytes);
			if ((bp->hashkey & BOOK_UPPER_MASK) == hashkey)
				book_positions[book_position_count++] = *bp;
		}
		else
			break;
	}

	/* Now find the moves which are suitable. */
	generate_root_moves();
	total_games = 0;
	for (root_move = zct->root_move_list; root_move < zct->root_move_list +
		zct->root_move_count; root_move++)
	{
		hashkey = board.hashkey & BOOK_UPPER_MASK;
		make_move(root_move->move);
		hashkey |= board.hashkey & BOOK_LOWER_MASK;
		unmake_move();
		/* Print out the book moves we found, and add up the game total. */
		for (bp = book_positions; bp < book_positions +
			book_position_count; bp++)
		{
			if (hashkey == bp->hashkey)
			{
				/* If we're in XBoard mode, print out a list of potential
					book moves. This is pretty ugly! */
				if (zct->protocol == XBOARD)
				{
					if (total_games == 0)
						print("0 0 0 0 (");
					else
						print(" ");
					print("%M=%i", root_move->move, bp->wins + bp->losses +
						bp->draws);
				}
				else if (zct->protocol != UCI)
					print("%M: games=%i winp=%.1f%%\n", root_move->move,
						bp->wins + bp->losses + bp->draws, (float)100 *
						(2 * bp->wins + bp->draws) / (bp->wins + bp->losses +
						bp->draws) / 2);
				/* Add up the total number of won games for the book selection
					algorithm. */
				total_games += bp->wins + bp->draws / 2;
			}
		}
	}
	if (total_games <= 0)
		return FALSE;
	if (zct->protocol == XBOARD)
	{
		print(")\n");
	}

	/* Now select the book move to be played. */
	random = (unsigned int)random_hashkey() % total_games;
	for (root_move = zct->root_move_list; root_move < zct->root_move_list +
		zct->root_move_count; root_move++)
	{
		hashkey = board.hashkey & BOOK_UPPER_MASK;
		make_move(root_move->move);
		/* Don't play repetitions in the book. See WCRCC '08. */
		if (is_repetition(2))
		{
			unmake_move();
			continue;
		}
		hashkey |= board.hashkey & BOOK_LOWER_MASK;
		unmake_move();
		/* Subtract from the random we generated the number of won games for
			this move. This gives a distribution over the moves proportional to
			their frequency and win rate. */
		for (bp = book_positions; bp < book_positions +
			book_position_count; bp++)
		{
			if (hashkey == bp->hashkey)
			{
				random -= bp->wins + bp->draws / 2;
				/* Got one! */
				if (random <= 0)
				{
					board.pv_stack[0][0] = root_move->move;
					/* In ICS mode, kibitz some statistics about the move.
						There really should be a better place for this. */
					if (zct->ics_mode)
						print("tellall bookmove %M: %.1f%% winning, "
							"%.1f%% draws\n", root_move->move,
							(float)100.0 * (bp->wins + bp->draws / 2) /
								(bp->wins + bp->draws + bp->losses),
							(float)100.0 * bp->draws / (bp->wins + bp->draws +
								bp->losses));
					return TRUE;
				}
			}
		}
	}
	return FALSE;
}

/**
bytes_to_book():
Converts an array of bytes into the internal BOOK_POSITION data structure. This
is to allow books created on any platform to be read on any platform.
Created 091507; last modified 101807
**/
BOOK_POSITION *bytes_to_book(unsigned char *bytes)
{
	static BOOK_POSITION bp;
	int x;

	bp.hashkey = (HASHKEY)0;
	for (x = 0; x < 8; x++)
		bp.hashkey |= (HASHKEY)*bytes++ << (x * 8);
	bp.wins = 0;
	for (x = 0; x < 2; x++)
		bp.wins |= *bytes++ << (x * 8);
	bp.losses = 0;
	for (x = 0; x < 2; x++)
		bp.losses |= *bytes++ << (x * 8);
	bp.draws = 0;
	for (x = 0; x < 2; x++)
		bp.draws |= *bytes++ << (x * 8);
	bp.flags = *bytes++;
	bp.learn = *bytes++;
	return &bp;
}

/**
book_to_bytes():
Converts the internal BOOK_POSITION data structure to an array of bytes. This i
used to write the book file portably on all platforms.
Created 091507; last modified 052008
**/
unsigned char *book_to_bytes(BOOK_POSITION *position)
{
	static unsigned char bytes[sizeof(BOOK_POSITION)];
	unsigned char *b;
	int x;

	b = bytes;
	for (x = 0; x < 8; x++)
		*b++ = (unsigned char)(position->hashkey >> (x * 8));
	for (x = 0; x < 2; x++)
		*b++ = (unsigned char)(position->wins >> (x * 8));
	for (x = 0; x < 2; x++)
		*b++ = (unsigned char)(position->losses >> (x * 8));
	for (x = 0; x < 2; x++)
		*b++ = (unsigned char)(position->draws >> (x * 8));
	*b++ = (unsigned char)position->flags;
	*b++ = (unsigned char)position->learn;
	return bytes;
}

/**
bytes_to_header():
Converts an array of bytes into the internal BOOK_HEADER data structure.
Created 071708; last modified 071708
**/
BOOK_HEADER *bytes_to_header(unsigned char *bytes)
{
	static BOOK_HEADER bh;
	int x;

	/* Header string */
	for (x = 0; x < 4; x++)
		bh.zctb[x] = *bytes++;
	/* Major version number */
	bh.major = 0;
	for (x = 0; x < 2; x++)
		bh.major |= *bytes++ << (x * 8);
	/* Minor version number. */
	bh.minor = 0;
	for (x = 0; x < 2; x++)
		bh.minor |= *bytes++ << (x * 8);
	/* Checksum */
	bh.checksum = (HASHKEY)0;
	for (x = 0; x < 8; x++)
		bh.checksum |= (HASHKEY)*bytes++ << (x * 8);

	return &bh;
}

/**
header_to_bytes():
Converts the internal BOOK_HEADER data structure to an array of bytes.
Created 071708; last modified 071708
**/
unsigned char *header_to_bytes(BOOK_HEADER *header)
{
	static unsigned char bytes[sizeof(BOOK_HEADER)];
	unsigned char *b;
	int x;

	b = bytes;
	for (x = 0; x < 4; x++)
		*b++ = (unsigned char)header->zctb[x];
	for (x = 0; x < 2; x++)
		*b++ = (unsigned char)(header->major >> (x * 8));
	for (x = 0; x < 2; x++)
		*b++ = (unsigned char)(header->minor >> (x * 8));
	for (x = 0; x < 8; x++)
		*b++ = (unsigned char)(header->checksum >> (x * 8));
	return bytes;
}

/**
book_pgn_func():
Called from within pgn_load (or epd_load), this function does all processing
needed for a position when creating a book. It returns TRUE if we should
stop parsing the PGN at this point (assuming we're parsing a PGN).
Created 013008; last modified 111208
**/
BOOL book_pos_func(void *pos_arg, POS_DATA *pos_data)
{
	BOOK_POS_INFO *bpi;
	BOOK_POSITION *bp;
	PGN_GAME *pgn_game;
	EPD_POS *epd_pos;
	BOOL drawn;
	COLOR winner;
	HASHKEY hashkey;
	
	bpi = (BOOK_POS_INFO *)pos_arg;

	if (pos_data->type == POS_PGN)
	{
		pgn_game = pos_data->pgn;

		/* See whether we should stop parsing this game based on the depth. */
		if (board.game_entry - board.game_stack > bpi->depth)
			return TRUE;

		/* Determine the game result. */
		winner = EMPTY;
		drawn = FALSE;
		/* Only parse the first characters of the result, as they might have a
		   comment on how the game was decided. */
		if (!strncmp(pgn_game->tag.result, "1-0", 3))
			winner = WHITE;
		else if (!strncmp(pgn_game->tag.result, "0-1", 3))
			winner = BLACK;
		else if (!strncmp(pgn_game->tag.result, "1/2-1/2", 7))
			drawn = TRUE;
		/* No result, skip this game. */
		if (winner == EMPTY && drawn == FALSE)
			return TRUE;

		/* Update the book buffer for the move just made. */
		hashkey = (board.game_entry - 1)->hashkey & BOOK_UPPER_MASK;
		hashkey |= board.hashkey & BOOK_LOWER_MASK;
		/* Find a place to store the position in the book buffer. */
		bp = hash_book(hashkey, bpi->book_buffer, bpi->buffer_size);
		/* Out of space... */
		if (bp == NULL)
			return FALSE;
		
		/* Update the book entry. */
		bp->hashkey = hashkey;
		if (winner == board.side_ntm)
			bp->wins++;
		else if (winner == board.side_tm)
			bp->losses++;
		else
			bp->draws++;
	}
	return FALSE;
}

/**
hash_book():
Look up the current position in the book buffer. If it is already there, we can
just update it. Otherwise, we need to find an empty slot to create a new book
position.
Created 052008; last modified 052008
**/
BOOK_POSITION *hash_book(HASHKEY hashkey, BOOK_POSITION *book_buffer,
	int buffer_size)
{
	BOOK_POSITION *bp;
	BOOK_POSITION *bp_start;

	/* Find an empty slot in the book buffer. */
	bp = bp_start = &book_buffer[hashkey % buffer_size];
	while (bp < book_buffer + buffer_size && bp->hashkey != 0 &&
		bp->hashkey != hashkey)
		bp++;
	/* No slot found, go to beginning. */
	if (bp >= book_buffer + buffer_size)
	{
		bp = book_buffer;
		while (bp < bp_start && bp->hashkey != 0 && bp->hashkey != hashkey)
			bp++;
		/* Still no slot, buffer full. */
		if (bp >= book_buffer + buffer_size)
			return NULL;
	}
	return bp;
}
