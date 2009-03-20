/** ZCT/cmddef.c--Created 081206 **/

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
#include "cmd.h"
#include "eval.h"
#include "pgn.h"
#include "smp.h"

COMMAND def_commands[] =
{
	{ 0, "analyze", "enter analysis mode", 1, cmd_analyze },
	{ 0, "bench", "benchmark chess search speed", 1, cmd_bench },
	{ 0, "bookc", "create a book from a pgn file", 1, cmd_bookc },
	{ 0, "bookl", "load an opening book (ZCT native format)", 0, cmd_bookl },
	{ 0, "bookp", "probe the current position in the opening book for a move",
		1, cmd_bookp },
#ifdef DEBUG_SEARCH
	{ 0, "debug", "enter search debug mode", 1, cmd_debug },
#endif
	{ 0, "display", "display the current board position", 0, cmd_display },
	{ 0, "divide", "display perft counts to depth - 1 for each root move",
		1, cmd_divide },
	{ 0, "easy", "turn pondering off", 2, cmd_easy },
	{ 0, "eval", "display the static evaluation for the current position",
		0, cmd_eval },
	{ 0, "evalparam", "set an evaluation function parameter",
		1, cmd_evalparam },
	{ 0, "exit", "exit ZCT", 0, cmd_exit },
	{ 0, "fen", "print the FEN string for the current position.", 0, cmd_fen },
	{ 0, "flip", "flip the board position from rank 1 to 8, reversing colors "
		"to create a symmetric position", 1, cmd_flip },
	{ 0, "force", "set ZCT to move for neither side", 1, cmd_force },
	{ 0, "go", "set ZCT to move", 2, cmd_go },
	{ 0, "hard", "turn pondering on", 0, cmd_hard },
	{ 0, "hash", "set the hash table size for the various hash tables",
		1, cmd_hash },
	{ 0, "hashprobe", "display hash information for the current position",
		0, cmd_hashprobe },
	{ 0, "help", "display a list of commands", 0, cmd_help },
	{ 0, "history", "display the move history of the current game",
		0, cmd_history },
	{ 0, "level", "set the time control", 1, cmd_level },
	{ 0, "load", "load a specific game from the internal PGN database",
		1, cmd_load },
	{ 0, "moves", "display all legal moves", 0, cmd_moves },
#ifdef SMP
	{ 0, "mp", "set the maximum number of processors to use", 1, cmd_mp },
#endif
	{ 0, "new", "start a new game", 1, cmd_new },
	{ 0, "notation", "set the notation for displaying moves in coordinate, "
		"SAN, or LSAN.", 0, cmd_notation },
	{ 0, "omin", "(output minimum) set the minimum output limit in nodes",
		0, cmd_omin },
	{ 0, "nopost", "turn search output off", 0, cmd_nopost },
	{ 0, "open", "open a PGN file into the internal database", 1, cmd_open },
	{ 0, "perf", NULL, 1, cmd_perf },
	{ 0, "perft", NULL, 1, cmd_perft },
	{ 0, "post", "turn search output on", 0, cmd_post },
	{ 0, "save", "save the current game into a PGN file", 0, cmd_save },
	{ 0, "sd", "set ZCT to think for a certain depth each move", 0, cmd_sd },
	{ 0, "setboard", "sets up a new FEN position", 1, cmd_setboard },
	{ 0, "setfen", "sets up a FEN position from a file, given a line number",
		1, cmd_setfen },
	{ 0, "sort", "displays all moves for this position in a sorted order "
		"along with their scores", 0, cmd_sort },
	{ 0, "source", "read the contents of a file as if they were commands, "
		"then return to standard input", 0, cmd_source },
	{ 0, "st", "set ZCT to think for a certain time each move", 0, cmd_st },
	{ 0, "test", "run ZCT on an EPD test suite", 1, cmd_test },
	{ 0, "testeval", "test the symmetry of ZCT's eval", 1, cmd_testeval },
	{ 0, "tune", "Given a pgn file of good quality games, tune the evaluation "
		"function to best match the moves", 1, cmd_tune },
	{ 0, "uci", NULL, 1, cmd_uci },
	{ 0, "undo", "undoes a move", 1, cmd_undo },
	{ 0, "verify", NULL, 0, cmd_verify },
	{ 0, "xboard", NULL, 0, cmd_xboard },
	{ 0, NULL, NULL, 0, NULL }
};

/**
cmd_analyze():
The "analyze" command causes ZCT to enter analyze mode. ZCT constantly thinks on the current position.
The only commands accepted are making and unmaking moves, setting up a board position, and exiting.
Created 102406; last modified 102406
**/
void cmd_analyze(void)
{
	zct->protocol = ANALYSIS;
	zct->zct_side = EMPTY;
	initialize_cmds();
}

/**
cmd_bench():
Run the bench() function to test speed.
Created 013008; last modified 013008
**/
void cmd_bench(void)
{
	bench();
}

/**
cmd_bookc():
The "bookc" command creates a new opening book.
Created 092507; last modified 092507
**/
void cmd_bookc(void)
{
	int width;
	int depth;
	int win_percent;

	if (cmd_input.arg_count != 6)
	{
		print("Usage: bookc pgn_file book_file min_play max_depth win_percent\n");
		return;
	}
	width = atoi(cmd_input.arg[3]);
	depth = atoi(cmd_input.arg[4]);	
	win_percent = atoi(cmd_input.arg[5]);
	book_update(cmd_input.arg[1], cmd_input.arg[2], width, depth, win_percent);
}

/**
cmd_bookl():
The "bookl" command loads an existing opening book.
Created 100207; last modified 100207
**/
void cmd_bookl(void)
{
	if (cmd_input.arg_count != 2)
	{
		print("Usage: bookl book_file\n");
		return;
	}
	book_load(cmd_input.arg[1]);
}

/**
cmd_bookp():
The "bookp" command probes the loaded opening book.
Created 100207; last modified 100207
**/
void cmd_bookp(void)
{
	book_probe();
}

/**
cmd_debug():
The "debug" command causes ZCT to enter search debug mode. At every step of the search a debug routine
is called and allows the user to step through the search.
Created 051407; last modified 051407
**/
void cmd_debug(void)
{
	zct->protocol = DEBUG;
	zct->zct_side = EMPTY;
	initialize_cmds();
}

/**
cmd_display():
The display command simply prints the current board position.
Created 070105; last modified 070105
**/
void cmd_display(void)
{
	print("%B", &board);
}

/**
cmd_divide():
The "divide" command displays perft counts for each move from the root position.
Created 102806; last modified 051507
**/
void cmd_divide(void)
{
	int depth;
	int time;
	BITBOARD nodes;
	BITBOARD total;
	static MOVE move_list[MAX_ROOT_MOVES];
	MOVE *next;
	MOVE *last;

	if (cmd_input.arg_count != 2)
	{
	 	print("Usage: divide depth\n");
		return;
	}
	depth = atoi(cmd_input.arg[1]);
	if (depth < 1 || depth > 63)
	{
		print("Depth must be between 1 and 63.\n");
		return;
	}

	last = generate_legal_moves(move_list);
//	check_legality(move_list, &last);

	total = 0;
	time = get_time();
	for (next = move_list; next < last; next++)
	{
		make_move(*next);
		nodes = perft(depth - 1, &board.move_stack[0]);
		unmake_move();
		total += nodes;
		print("%5M=%L\n", *next, nodes);
	}
	print("moves=%L time=%T\n", total, get_time() - time);
}

/**
cmd_eval():
Prints the static evaluation of the current position.
Created 081906; last modified 110607
**/
void cmd_eval(void)
{
	EVAL_BLOCK eval_block;
	VALUE value;
	COLOR color;

	value = evaluate(&eval_block);

	print("game phase: %i\n", eval_block.phase);
	for (color = WHITE; color <= BLACK; color++)
	{
		print("king safety[%C]: %V\n", color, eval_block.king_safety[color]);
		print("passed pawn[%C]: %V\n", color, eval_block.passed_pawn[color]);
	}

	print("total eval: %V\n", value);
}

/**
cmd_evalparam():
Set an evaluation parameter.
Created 020808; last modified 020808
**/
void cmd_evalparam(void)
{
	int param;
	int element;
	VALUE value;

	if (cmd_input.arg_count < 3)
	{
		print("Usage: evalparam param_number value(s)...\n");
		return;
	}
	param = atoi(cmd_input.arg[1]);
	if (param < 0 || eval_parameter[param].value == NULL)
	{
		print("Invalid parameter number.\n");
		return;
	}
	switch (eval_parameter[param].dimensions)
	{
		case 0:
			value = (VALUE)atoi(cmd_input.arg[2]);
			*eval_parameter[param].value = value;
			break;
		case 1:
			if (eval_parameter[param].dimension[0] != cmd_input.arg_count - 2)
			{
				print("Invalid parameter count.\n");
				return;
			}
			for (element = 0; element < cmd_input.arg_count - 2; element++)
			{
				value = (VALUE)atoi(cmd_input.arg[element + 2]);
				eval_parameter[param].value[element] = value;
			}
			break;
		case 2:	
			if (eval_parameter[param].dimension[0] * eval_parameter[param].dimension[1] != cmd_input.arg_count - 2)
			{
				print("Invalid parameter count.\n");
				return;
			}
			for (element = 0; element < cmd_input.arg_count - 2; element++)
			{
				value = (VALUE)atoi(cmd_input.arg[element + 2]);
				eval_parameter[param].value[element] = value;
			}
			break;
	}
}

/**
cmd_exit():
Quits ZCT. Why would anyone want to do that???
Created 070105; last modified 070105
**/
void cmd_exit(void)
{
	exit(EXIT_SUCCESS);
}

/**
cmd_fen():
Prints the FEN representation of the current position.
Created 071708; last modified 071708
**/
void cmd_fen(void)
{
	print("%F\n", &board);
}

/**
cmd_flip():
Flips the board from top to bottom while switching the color of all pieces and
the side to move, creating a symmetrical position.
Created 100207; last modified 100207
**/
void cmd_flip(void)
{
	flip_board();
}

/**
cmd_hash():
The "hash" command adjusts the size of the various hash tables.
Created 123107; last modified 110908
**/
void cmd_hash(void)
{
	int p;
	BITBOARD size;

	if (cmd_input.arg_count != 3)
	{
		print("Usage: hash main|qsearch|eval|pawn size\n");
		return;
	}
	/* Calculate the size, including the "K", "M", and "G" markers. */
	size = atoi(cmd_input.arg[2]);
	if (strchr(cmd_input.arg[2], 'K'))
		size <<= 10;
	else if (strchr(cmd_input.arg[2], 'M'))
		size <<= 20;
	else if (strchr(cmd_input.arg[2], 'G'))
		size <<= 30;
	/* Now determine the table to resize. */
	if (!strcmp(cmd_input.arg[1], "main"))
		hash_alloc(size / sizeof(HASH_ENTRY));
	else if (!strcmp(cmd_input.arg[1], "qsearch")) 
		zct->qsearch_hash_size = size / sizeof(HASH_ENTRY);
	else if (!strcmp(cmd_input.arg[1], "eval")) 
		zct->eval_hash_size = size / sizeof(EVAL_HASH_ENTRY);
	else if (!strcmp(cmd_input.arg[1], "pawn")) 
		zct->pawn_hash_size = size / sizeof(PAWN_HASH_ENTRY);
	else
	{
		print("Invalid table type. Valid parameters are \"main\", \"qsearch\", \"eval\", and \"pawn\".\n");
		return;
	}
#ifdef SMP
	/* Tell the child processors to update the hash size. We must activate the
		processors in order to message them, and then deactivate them again.
		This is safe because the child processors will always be idle at this
		point. */
	for (p = 1; p < zct->process_count; p++)
	{
		make_active(p);
		smp_tell(p, SMP_UPDATE_HASH, 0);
		make_idle(p);
	}
#endif
	initialize_hash();
	print("%s hash size set to %s\n", cmd_input.arg[1], cmd_input.arg[2]);
}

/**
cmd_hashprobe():
Looks inside the hashtable for the current position and displays any info found.
Created 051507; last modified 051507
**/
void cmd_hashprobe(void)
{
	hash_print();
}

/**
cmd_help():
The "help" command lists all commands available and a description of each.
Created 091406; last modified 071207
**/
void cmd_help(void)
{
	display_help();
}

/**
cmd_history():
Displays the game history.
Created 112907; last modified 112907
**/
void cmd_history(void)
{
	GAME_ENTRY *current;

	current = board.game_entry;
	while (board.game_entry > board.game_stack)
		unmake_move();
	if (board.side_tm == BLACK)
	{
		print("%i. ...\t%M\n", board.move_number, board.game_entry->move);
		make_move(board.game_entry->move);
	}
	while (board.game_entry < current)
	{
		if (board.side_tm == WHITE)
			print("%i. %M", board.move_number, board.game_entry->move);
		else
			print("\t%M\n", board.game_entry->move);
		make_move(board.game_entry->move);
	}
	if (board.side_tm == BLACK)
		print("\n");
}

/**
cmd_load():
The "load" command selects a game from the internal PGN database and loads it.
Created 091607; last modified 013008
**/
void cmd_load(void)
{
	if (cmd_input.arg_count != 2)
	{
		print("Usage: load game_number\n");
		return;
	}
	pgn_load(atoi(cmd_input.arg[1]), NULL, NULL);
}

/**
cmd_moves():
Displays the list of legal moves from the current position.
Created 070305; last modified 062607
**/
void cmd_moves(void)
{
	static MOVE move_list[MAX_ROOT_MOVES];
	MOVE *next;
	MOVE *last;

	last = generate_moves(move_list);
	check_legality(move_list, &last);

	print("%i move%s: ", last - move_list, last - move_list == 1 ? "" : "s");
	for (next = move_list; next < last; next++)
		print("%M ", *next);
	print("\n");
}

#ifdef SMP
/**
cmd_mp():
The "mp" command sets the number of processors to use in SMP mode.
Created 100306; last modified 102506
**/
void cmd_mp(void)
{
	int x;

	if (cmd_input.arg_count != 2)
	{
		print("Usage: mp number_of_processors\n");
		return;
	}
	x = atoi(cmd_input.arg[1]);
	if (x < 1 || x > MAX_CPUS)
	{
		print("Invalid number of processors. Must be 1-%i.\n", MAX_CPUS);
		return;
	}
	initialize_smp(x);
	print("Using %i processor%s.\n", x, x > 1 ? "s" : "");
}
#endif

/**
cmd_new():
Start a new game.
Created 070105; last modified 092507
**/
void cmd_new(void)
{
	initialize_board(NULL);
	hash_clear();
}

/**
cmd_notation():
Set the notation used for displaying moves.
Created 013008; last modified 013008
**/
void cmd_notation(void)
{
	if (cmd_input.arg_count != 2)
	{
		print("Usage: notation coord|san|lsan\n");
		return;
	}
	if (!strcmp(cmd_input.arg[1], "coordinate") ||
		!strcmp(cmd_input.arg[1], "coord") ||
		!strcmp(cmd_input.arg[1], "COORD"))
		zct->notation = COORDINATE;
	else if (!strcmp(cmd_input.arg[1], "san") ||
		!strcmp(cmd_input.arg[1], "SAN"))
		zct->notation = SAN;
	else if (!strcmp(cmd_input.arg[1], "lsan") ||
		!strcmp(cmd_input.arg[1], "LSAN"))
		zct->notation = LSAN;
}

/**
cmd_omin():
Set the minimum number of nodes to be searched before printing search info.
Created 011208; last modified 011208
**/
void cmd_omin(void)
{
	int n;

	if (cmd_input.arg_count != 2)
	{
	 	print("Usage: omin node_count\n");
		return;
	}
	n = atoi(cmd_input.arg[1]);
	if (n < 0)
	{
		print("Node count must be >= 0.\n");
		return;
	}
	zct->output_limit = n;
	print("Output minimum set at %i nodes.\n", zct->output_limit);
}

/**
cmd_open():
The "open" command opens a PGN file into the internal database.
Created 091607; last modified 091607
**/
void cmd_open(void)
{
	if (cmd_input.arg_count != 2)
	{
	 	print("Usage: open pgn_file_name\n");
		return;
	}
	pgn_open(cmd_input.arg[1]);
}

/**
cmd_perf():
The "perf" command tests the performance of the move generator.
Created 110606; last modified 110606
**/
void cmd_perf(void)
{
	int time;
	int iterations;
	int x;
	MOVE move_list[256];
	MOVE *last_move;

	if (cmd_input.arg_count != 2)
	{
	 	print("Usage: perf iterations\n");
		return;
	}
	iterations = atoi(cmd_input.arg[1]);
	time = get_time();
	for (x = iterations; x > 0; x--)
		generate_moves(move_list);
	last_move = generate_moves(move_list);
	print("moves=%i rate=%.2f\n", iterations * (last_move - move_list),
		(float)(iterations * (last_move - move_list)) / (get_time() - time) / 1000.0);
}

/**
cmd_perft():
The "perft" function debugs the move generator by trying all sequences of moves
to a given depth and counting the number of moves made.
Created 092106; last modified 042708
**/
void cmd_perft(void)
{
	int depth;
	int time;
	BITBOARD nodes;

	if (cmd_input.arg_count != 2)
	{
	 	print("Usage: perft depth\n");
		return;
	}
	depth = atoi(cmd_input.arg[1]);
	if (depth < 1 || depth > 63)
	{
		print("Depth must be between 1 and 63.\n");
		return;
	}
	time = get_time();
	nodes = perft(depth, &board.move_stack[0]);
	print("moves=%L time=%T\n", nodes, get_time() - time);
}

/**
cmd_save():
The "save" command saves the current game in PGN format to an external file.
Created 091407; last modified 091407
**/
void cmd_save(void)
{
	char file_name[128];

	if (cmd_input.arg_count != 2)
	{
	 	print("Usage: save file_name\n");
		return;
	}
	strcpy(file_name, cmd_input.arg[1]);
	pgn_save(file_name);
}

/**
cmd_setfen():
The "setfen" command looks at a FEN/EPD file and sets the board to a certain numbered position.
Created 112907; last modified 011208
**/
void cmd_setfen(void)
{
	FILE *fen_file;
	char buffer[BUFSIZ];
	char fen[BUFSIZ];
	int n;
	int pos_number;

	if (cmd_input.arg_count != 3)
	{
		print("Usage: setfen fen_file position_number\n");
		return;
	}
	if ((fen_file = fopen(cmd_input.arg[1], "r")) == NULL)
	{
		perror(cmd_input.arg[1]);
		return;
	}
	pos_number = atoi(cmd_input.arg[2]);
	n = 1;
	while (fgets(buffer, BUFSIZ, fen_file) != NULL)
	{
		if (n >= pos_number)
			break;
		n++;
	}
	if (n == pos_number)
	{
		set_cmd_input(buffer);
		cmd_parse(" \t\n");
		strcpy(fen, "");
		for (n = 0; n < cmd_input.arg_count; n++)
		{
			if (!strcmp(cmd_input.arg[n], "am") || !strcmp(cmd_input.arg[n], "bm") ||
				!strcmp(cmd_input.arg[n], "id"))
				break;
			strcat(fen, cmd_input.arg[n]);
			strcat(fen, " ");
		}
		initialize_board(fen);
	}
}

/**
cmd_sort():
The "sort" command prints out each move along with its score in sorted order.
Created 082507; last modified 090608
**/
void cmd_sort(void)
{
	SEARCH_BLOCK sb;
	MOVE move_list[256];
	BOOL flags;

	flags = FALSE;
	if (cmd_input.arg_count == 2)
		flags = TRUE;

	sb.first_move = move_list;
	sb.next_move = move_list;
	sb.check = check_squares();
	sb.select_state = SELECT_HASH_MOVE;
	sb.ply = 0;
	print("move   score  flags\n");
	if (flags)
	{
		while ((sb.move = select_move(&sb)) != NO_MOVE)
		{
			print("%5M: %5i ", sb.move, MOVE_SCORE(sb.move));
			if (sb.move & MOVE_NOREDUCE)
				print("R");
			if (sb.move & MOVE_NODELPRUNE)
				print("D");
			if (sb.move & MOVE_NOEXTEND)
				print("E");
			if (sb.move & MOVE_ISREDUCED)
				print("I");
			print("\n");
		}
	}
	else
	{
		while ((sb.move = select_move(&sb)) != NO_MOVE)
			print("%5M: %5i\n", sb.move, MOVE_SCORE(sb.move));
	}
}

/**
cmd_source():
The "source" command changes ZCT's input to a file temporarily, rather than
stdin. It's basically a "batch" file.
Created 020808; last modified 020808
**/
void cmd_source(void)
{
	if (cmd_input.arg_count != 2)
	{
		print("Usage: source source_file\n");
		return;
	}

	zct->input_stream = fopen(cmd_input.arg[1], "r");
	if (zct->input_stream == NULL)
	{
		perror(cmd_input.arg[1]);
		zct->input_stream = stdin;
		return;
	}
	zct->source = TRUE;
}

/**
cmd_test():
The "test" command sets ZCT to run on an EPD test suite.
Created 110506; last modified 110506
**/
void cmd_test(void)
{
	int time;

	if (cmd_input.arg_count != 3)
	{
		print("Usage: test file_name time_per_position\n");
		return;
	}
	time = atoi(cmd_input.arg[2]);
	set_time_control(0, 0, time);
	test_epd(cmd_input.arg[1]);
}

/**
cmd_testeval():
The "testeval" command tests eval symmetry on an EPD file.
Created 092907; last modified 111308
**/
void cmd_testeval(void)
{
	if (cmd_input.arg_count != 2)
	{
		print("Usage: testeval file_name\n");
		return;
	}
	test_epd_eval(cmd_input.arg[1]);
}

/**
cmd_tune():
Takes a PGN filename and tunes the evaluation to match the moves made.
Created 101907; last modified 101907
**/
void cmd_tune(void)
{
	if (cmd_input.arg_count != 2)
	{
		print("Usage: tune file_name\n");
		return;
	}
	zct->protocol = TUNE;
	initialize_cmds();
	tune_pgn(cmd_input.arg[1]);
	zct->protocol = DEFAULT;
	initialize_cmds();
}

/**
cmd_uci():
Switches to UCI mode. Loads the UCI command set into the command table.
Created 101607; last modified 042808
**/
void cmd_uci(void)
{
	zct->protocol = UCI;
	zct->notation = COORDINATE;
	zct->zct_side = EMPTY;
	initialize_cmds();
	print("id name %s%i\n", ZCT_VERSION_STR, ZCT_VERSION);
	print("id author Zach Wegner\n");
	print("uciok\n");
}

/**
cmd_verify():
Verify the internal data structures.
Created 081105; last modified 081105
**/
void cmd_verify(void)
{
	verify();
}

/**
cmd_xboard():
Switches to xboard mode. Loads the xboard command set into the command table.
Created 081906; last modified 081906
**/
void cmd_xboard(void)
{
	zct->protocol = XBOARD;
	initialize_cmds();
}
