/** ZCT/init.c--Created 060205 **/

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
#include "bit.h"
#include "smp.h"

/**
initialize_settings():
Sets up all of the standard engine state variables.
Created 051708; last modified 051708
**/
void initialize_settings(void)
{
#if !defined(ZCT_WINDOWS) && defined(SMP)
	zct = (GLOBALS *)shared_alloc(sizeof(GLOBALS));
#endif

	/* Initialize the standard global stuff. */
	zct->source = FALSE;
	zct->input_stream = stdin;
	zct->output_stream = stdout;
	zct->log_stream = fopen("logfile.txt", "at+");
	if (zct->log_stream == NULL)
		fatal_error("fatal error: could not open logfile.txt.\n");

	setbuf(zct->input_stream, NULL);
	setbuf(zct->output_stream, NULL);
	setbuf(zct->log_stream, NULL);
	/* Dann Corbit advises that some compilers give incorrect behavior
		without the following lines. Buggy compilers=ugly code. */
	setvbuf(zct->output_stream, NULL, _IONBF, 0);
	setvbuf(zct->input_stream, NULL, _IONBF, 0);
	fflush(NULL);

	/* Initialize name string and print our little header. */
	strcpy(zct->name_string, "");
	fprintf(zct->output_stream, "%s\n\n", zct_version_string());
	fprintf(zct->log_stream, "%s\n\n", zct_version_string());

	/* Engine state */
	zct->protocol = DEFAULT;
	zct->engine_state = IDLE;
	zct->use_book = TRUE;
	zct->post = TRUE;
	zct->hard = TRUE;
	zct->notation = SAN;
	zct->ics_mode = FALSE;

	/* Search stuff */
	set_time_control(0, 5 * 60000, 0);
	zct->nodes_per_check = 10000;
	zct->output_limit = 20000;
	zct->max_depth = 0;
	zct->max_nodes = 0;
	zct->check_extension = 3;
	zct->one_rep_extension = 4;
	zct->threat_extension = 4;
	zct->passed_pawn_extension = 3;
	zct->lmr_threshold = 70;
	zct->history_counter = 1;
	zct->hash_size = 32 * HASH_MB;
	zct->qsearch_hash_size = 256 * HASH_KB;
	zct->pawn_hash_size = 1 * PAWN_HASH_MB;
	zct->eval_hash_size = 512 * EVAL_HASH_KB;
}

/**
initialize_data():
Sets up all of the basic chess stuff needed for the program.
Created 070305; last modified 011808
**/
void initialize_data(void)
{
	int x;
	int y;
	int z;
	BITBOARD mask;
	COLOR c;
	PIECE p;
	SQUARE s;
	BITBOARD (*fill_dir[8])(BITBOARD pieces, BITBOARD empty) =
	{
		fill_up_left, fill_up, fill_up_right, fill_right,
		fill_down_right, fill_down, fill_down_left, fill_left
	};

	/* masks */
	for (s = A1; s < OFF_BOARD; s++)
		mask_bb[s] = (BITBOARD)1 << s;
	/* attack bitboards */
	for (s = A1; s < OFF_BOARD; s++)
	{
		mask = MASK(s);
		knight_moves_bb[s] = SHIFT_UP(SHIFT_UL(mask)) |
			SHIFT_UP(SHIFT_UR(mask)) |
			SHIFT_DN(SHIFT_DL(mask)) |
			SHIFT_DN(SHIFT_DR(mask)) |
			SHIFT_LF(SHIFT_UL(mask)) |
			SHIFT_LF(SHIFT_DL(mask)) |
			SHIFT_RT(SHIFT_UR(mask)) |
			SHIFT_RT(SHIFT_DR(mask));
		king_moves_bb[s] = SHIFT_UL(mask) | SHIFT_UP(mask) |
			SHIFT_UR(mask) | SHIFT_RT(mask) | SHIFT_DR(mask) |
			SHIFT_DN(mask) | SHIFT_DL(mask) | SHIFT_LF(mask);

		/* pawn attacks */
		for (c = WHITE; c <= BLACK; c++)
		{
			mask = MASK(s);
			mask = SHIFT_FORWARD(mask, c);
			mask = SHIFT_LF(mask) | SHIFT_RT(mask);
			pawn_caps_bb[c][s] = mask;
		}

		/* Set up the in-between bitboards for every set of two squares. */
		for (x = A1; x < OFF_BOARD; x++)
		{
			mask = (BITBOARD)0;
			/* Inefficient: try all directions and only if they match up
				set the bitboard. */
			for (y = 0; y < 8; y++)
				mask |= fill_dir[y](MASK(s), ~(BITBOARD)0) &
					fill_dir[(y + 4) & 7](MASK(x), ~(BITBOARD)0);

			in_between_bb[s][x] = mask & ~MASK(s) & ~MASK(x);
		}
	}

	castle_ks_mask[WHITE] = MASK(F1) | MASK(G1);
	castle_ks_mask[BLACK] = MASK(F8) | MASK(G8);
	castle_qs_mask[WHITE] = MASK(D1) | MASK(C1) | MASK(B1);
	castle_qs_mask[BLACK] = MASK(D8) | MASK(C8) | MASK(B8);

	/* Set up the Zobrist arrays for hashkeys. */
	/* Start by running the PRNG a bunch to properly initialize it. */
	for (x = 0; x < 1000; x++)
		(void)random_hashkey();
	for (c = WHITE; c <= BLACK; c++)
	{
		for (p = PAWN; p <= KING; p++)
		{
			for (s = A1; s < OFF_BOARD; s++)
			{
				zobrist_piece[c][p][s] = random_hashkey();
				/* Check against other keys to ensure good randoms. This
					shouldn't matter, but I found a PRNG bug this way... */
				for (x = WHITE; x <= c; x++)
					for (y = PAWN; y <= p; y++)
						for (z = A1; z < s; z++)
							if (zobrist_piece[c][p][s] ==
								zobrist_piece[x][y][z])
								zobrist_piece[c][p][s] ^=
									random_hashkey() ^ random_hashkey();
			}
		}
	}
	for (s = A1; s < OFF_BOARD; s++)
		zobrist_ep[s] = random_hashkey();
	
	for (x = 0; x < 16; x++)
		zobrist_castle[x] = random_hashkey();

	zobrist_stm = random_hashkey();

	for (x = 0; x < 4096; x++)
		for (y = 0; y < HASH_PATH_MOVE_COUNT; y++)
			zobrist_path_move[x][y] = random_hashkey();

	/* Now that we have set up our random numbers, that should be the same
		every time ZCT is ran, randomize more so that we can get
		nondeterministic values. This is just used for the opening book ATM. */
	for (x = 1; x < 100; x++)
	{
		seed_random(get_time()*x);
		for (y = 0; y < 1000; y++)
			random_hashkey();
	}

	/* If we are using SMP, we need to allocate the hash table in shared memory,
		done elsewhere. The other tables are allocated one per process. */
	hash_alloc(zct->hash_size);
	qsearch_hash_table = (HASH_ENTRY *)calloc(zct->qsearch_hash_size,
		sizeof(HASH_ENTRY));
	eval_hash_table = (EVAL_HASH_ENTRY *)calloc(zct->eval_hash_size,
		sizeof(EVAL_HASH_ENTRY));
	pawn_hash_table = (PAWN_HASH_ENTRY *)calloc(zct->pawn_hash_size,
		sizeof(PAWN_HASH_ENTRY));
}

/**
initialize_board():
Initializes board to the starting position if given a NULL argument, or the given FEN otherwise.
Created 060205; last modified 092507
**/
void initialize_board(char *fen)
{
	const char castle_string[5] = "KkQq";
	enum { INIT_BOARD, INIT_STM, INIT_CASTLE, INIT_EP, INIT_FIFTY,
		INIT_MOVE_COUNT } init_state;
	char *p;
	char *r;
	int x;
	COLOR c;
	SQ_FILE file;
	SQ_RANK rank;
	SQUARE square;

	zct->game_over = FALSE;
	zct->game_result = INCOMPLETE;
	zct->search_age = 0;
	zct->computer = FALSE;

	if (fen == NULL)
	{
		fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
		zct->zct_side = BLACK;
	}
	else
		zct->zct_side = EMPTY;

	/* Initialize. */
	for (square = A1; square < OFF_BOARD; square++)
	{
		board.piece[square] = EMPTY;
		board.color[square] = EMPTY;
	}
	board.castle_rights = CASTLE_NO;
	board.ep_square = OFF_BOARD;
	board.fifty_count = 0;
	board.move_number = 1;

	square = A8;
	file = FILE_A;
	rank = RANK_8;
	init_state = INIT_BOARD;

	/* Start reading the board information from the FEN. */
	for (p = fen; *p != '\0'; p++)
	{
		/* Space: next field */
		if (*p == ' ')
		{
			init_state++;
			continue;
		}
		switch (init_state)
		{
			/* Parse the main board data: pieces on squares. */
			case INIT_BOARD:
				/* Slash: next rank */
				if (*p == '/')
				{
					rank--;
					file = FILE_A;
				}
				/* Error checking: off board */
				if (rank < RANK_1 || file > FILE_H)
				{
					initialize_board(NULL);
					print("FEN parse error: square off board\n");
					return;
				}
				/* Look for a piece of either color. */
				square = SQ_FROM_RF(rank, file);
				for (c = WHITE; c <= BLACK; c++)
				{
					if ((r = strchr(piece_char[c], *p)) != NULL)
					{
						board.piece[square] = (PIECE)(r - piece_char[c]); 
						board.color[square] = c;
						square++;
						file++;
						break;
					}
				}
				/* Look for number indicating number of empty squares. */
				if (*p >= '1' && *p <= '8')
				{
					for (x = *p - '0'; x >= 1; x--)
					{
						board.piece[square] = EMPTY;
						board.color[square] = EMPTY;
						square++;
						file++;
					}
				}
				break;
			/* Side to move */
			case INIT_STM:	
				if (*p == 'W' || *p == 'w')
					board.side_tm = WHITE;
				else if (*p == 'B' || *p == 'b')
					board.side_tm = BLACK;
				else
				{
					initialize_board(NULL);
					print("FEN parse error: bad side to move char '%c'\n", *p);
					return;
				}
				board.side_ntm = COLOR_FLIP(board.side_tm);
				break;
			/* Castling rights */
			case INIT_CASTLE:
				/* You aren't supposed to understand this. */
				if ((r = strchr(castle_string, *p)) != NULL)
					board.castle_rights |= 1 << (r - castle_string);
				else if (*p != '-')
				{
					initialize_board(NULL);
					print("FEN parse error: bad castle status char '%c'\n", *p);
					return;
				}
				break;
			/* En passant square */
			case INIT_EP:
				if (*p >= 'a' && *p <= 'h' && *(p + 1) >= '1' &&
					*(p + 1) <= '8')
				{
					board.ep_square = SQ_FROM_RF(*(p + 1) - '1', *p - 'a');
					p++;
				}
				else if (*p != '-')
				{
					initialize_board(NULL);
					print("FEN parse error: bad ep square '%c%c'\n",
						*p, *(p + 1));
					return;
				}
				break;
			/* Fifty Move counter */
			case INIT_FIFTY:
				board.fifty_count = atoi(p);
				/* Hacky: Find where the number ends, and then back up so the
					parser starts reading in the right spot. */
				while (*p >= '0' && *p <= '9')
					p++;
				p--;
				break;
			/* Move counter */
			case INIT_MOVE_COUNT:
				board.move_number = atoi(p);
				/* Hacky: Find where the number ends, and then back up so the
					parser starts reading in the right spot. */
				while (*p >= '0' && *p <= '9')
					p++;
				p--;
				break;
			default:
				continue;
		}
	}
	/* Up to the side to move is the minimum amount of information we need. */
	if (init_state < INIT_STM)
	{
		initialize_board(NULL);
		print("FEN parse error: not enough information\n");
		return;
	}
	board.game_entry = &board.game_stack[0];

	initialize_bitboards();
	initialize_hashkey();
	reset_clocks();
}

/**
initialize_bitboards():
Set up piece and occupied bitboards from the piece[] and color[] arrays. Also
sets up other board representation data.
Created 070105; last modified 081608
**/
void initialize_bitboards(void)
{
	COLOR color;
	PIECE piece;
	SQUARE square;

	/* Clear out all the bitboards. */
	board.occupied_bb = (BITBOARD)0;
	for (color = WHITE; color <= BLACK; color++)
	{
		board.color_bb[color] = (BITBOARD)0;
		board.pawn_count[color] = 0;
		board.piece_count[color] = 0;
		board.material[color] = 0;
	}

	for (piece = 0; piece < 6; piece++)
		board.piece_bb[piece] = (BITBOARD)0;

	/* Loop through all squares and add in the piece data. */
	for (square = A1; square < OFF_BOARD; square++)
	{
		if (board.color[square] != EMPTY)
		{
			color = board.color[square];
			piece = board.piece[square];

			SET_BIT(board.occupied_bb, square);
			SET_BIT(board.color_bb[color], square);
			SET_BIT(board.piece_bb[piece], square);
			if (board.piece[square] == PAWN)
				board.pawn_count[color]++;
			else
				board.piece_count[color]++;
			if (piece != KING)
				board.material[color] += piece_value[piece];
		}
	}

	board.king_square[WHITE] = first_square(board.piece_bb[KING] &
		board.color_bb[WHITE]);
	board.king_square[BLACK] = first_square(board.piece_bb[KING] &
		board.color_bb[BLACK]);
}

/**
initialize_hashkey():
Initializes the main hashkey and pawn hashkey with the zobrist arrays.
Created 080906; last modified 050408
**/
void initialize_hashkey(void)
{
	BITBOARD pieces;
	SQUARE square;
	MOVE move;
	int x;

	board.hashkey = (HASHKEY)0;
	board.path_hashkey = (HASHKEY)0;
	board.pawn_entry.hashkey = (HASHKEY)0;
	pieces = board.occupied_bb;
	while (pieces)
	{
		square = first_square(pieces);
		CLEAR_BIT(pieces, square);
		
		board.hashkey ^=
			zobrist_piece[board.color[square]][board.piece[square]][square];
		if (board.piece[square] == PAWN)
			board.pawn_entry.hashkey ^=
				zobrist_piece[board.color[square]][PAWN][square];
	}
	if (board.side_tm == BLACK)
		board.hashkey ^= zobrist_stm;
	board.hashkey ^= zobrist_castle[board.castle_rights];
	board.hashkey ^= zobrist_ep[board.ep_square];
	/* Path hashing. Go back a certain number of moves and add the keys. */
	for (x = 1; x <= HASH_PATH_MOVE_COUNT &&
		board.game_entry - x >= board.game_stack; x++)
	{
		move = (board.game_entry - x)->move;
		/* Calculate the move number based on the ply modulo
			the path move count. */
		board.path_hashkey ^= zobrist_path_move[MOVE_KEY(move)]
			[(board.game_entry - x - board.game_stack) % HASH_PATH_MOVE_COUNT];
	}
}

/**
hash_alloc():
Allocate the hash table in either shared or local memory, depending on whether
we are using SMP. Note that the argument is the number of entries, not the
size in bytes.
Created 123107; last modified 103008
**/
void hash_alloc(BITBOARD hash_table_size)
{
#ifdef SMP
	BOOL restart = FALSE;

	/* If hash table was already allocated, we must set it free! */
	if (zct->hash_table != NULL)
	{
		restart = TRUE;
		shared_free(zct->hash_table, zct->hash_size * sizeof(HASH_ENTRY));
	}
	/* Allocate a new table via mmap() */
	zct->hash_table =
		(HASH_ENTRY *)shared_alloc(hash_table_size * sizeof(HASH_ENTRY));

	/* HACKY: restart all processors so that they have updated the references
		to shared memory. */
	if (restart)
		initialize_smp(zct->process_count);
#else
	if (zct->hash_table != NULL)
		free(zct->hash_table);
	if ((zct->hash_table =
		(HASH_ENTRY *)calloc(hash_table_size, sizeof(HASH_ENTRY))) == NULL)
		fatal_error("fatal error: could not allocate hash table.\n");
#endif
	zct->hash_size = hash_table_size;
}

/**
initialize_hash():
For all of the process specific hash tables (i.e. not the main one), allocate
them based on a process-global size.
Created 051708; last modified 103008
**/
void initialize_hash(void)
{
	/* qsearch hash table */
	if (qsearch_hash_table != NULL)
		free(qsearch_hash_table);
	qsearch_hash_table =
		(HASH_ENTRY *)calloc(zct->qsearch_hash_size, sizeof(HASH_ENTRY));

	/* eval hash table */
	if (eval_hash_table != NULL)
		free(eval_hash_table);
	eval_hash_table =
		(EVAL_HASH_ENTRY *)calloc(zct->eval_hash_size, sizeof(EVAL_HASH_ENTRY));

	/* pawn hash table */
	if (pawn_hash_table != NULL)
		free(pawn_hash_table);
	pawn_hash_table =
		(PAWN_HASH_ENTRY *)calloc(zct->pawn_hash_size, sizeof(PAWN_HASH_ENTRY));

	/* failure check */
	if (qsearch_hash_table == NULL || eval_hash_table == NULL ||
		pawn_hash_table == NULL)
		fatal_error("fatal error: could not allocate hash tables.\n");
}
