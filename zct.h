/** ZCT/zct.h--Created 060105 **/

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
#ifndef ZCT_H
#define ZCT_H

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define SMP
//#define CLUSTER
//#define EGBB
//#define ZCT_DEBUG
//#define DEBUG_SEARCH

/* These are the supported architectures/OSes. Note that OSX implies POSIX,
	and WINDOWS implies x86. */
#define ZCT_POSIX
//#define ZCT_OSX
//#define ZCT_WINDOWS
#define ZCT_x86
#define ZCT_64

#define ZCT_INLINE

/* Get the number of processors */
#ifndef MAX_CPUS
#	ifdef SMP
#		define MAX_CPUS			(8)
#	else
#		define MAX_CPUS			(1)
#	endif
#elif MAX_CPUS > 1
#	define SMP
#endif

/* During development, the version number comes externally from a file. */
#ifndef ZCT_VERSION
#	define ZCT_VERSION			(2491)
#endif

/* Ghetto ass preprocessor string concatenation */
#define ZCT_VERSION_STR1		"ZCT"

#ifdef ZCT_64
#	define ZCT_VERSION_STR2		ZCT_VERSION_STR1 "x64"
#else
#	define ZCT_VERSION_STR2		ZCT_VERSION_STR1
#endif

#ifdef SMP
#	define ZCT_VERSION_STR3		ZCT_VERSION_STR2 ".MP"
#else
#	define ZCT_VERSION_STR3		ZCT_VERSION_STR2
#endif

#define ZCT_VERSION_STR		ZCT_VERSION_STR3 "0.3."

/* ...that's enough of that. */

/* Platform dependency junk */
#if defined(ZCT_WINDOWS)

/* Ugly Windows compatibility!! */
#	define WIN32_LEAN_AND_MEAN
#	include <Windows.h>
#	undef INFINITE
#	undef TRUE
#	undef FALSE
#	define THREAD_LOCAL		__declspec(thread)
#	define I64			"I64u"
typedef unsigned __int64 BITBOARD;

#else /* ZCT_WINDOWS */

#	define THREAD_LOCAL
#	define I64			"llu"
typedef unsigned long long BITBOARD;
#	ifdef ZCT_POSIX
#		include <unistd.h>
#	endif /* ZCT_POSIX */

#endif /* !ZCT_WINDOWS */

/* Debug */
#ifdef ZCT_DEBUG
#define ASSERT(x)	do														\
					{														\
						if (!(x))	{										\
							fatal_error("Assert failed. %s(%i): " #x "\n%B",\
								__FILE__, __LINE__, &board);				\
		print("%B%i\n", &board, board.game_entry - board.game_stack);\
		while(--board.game_entry >= board.game_stack)\
			print("%S%S\n",	MOVE_FROM((board.game_entry)->move),\
				MOVE_TO((board.game_entry)->move));\
						}\
					} while (FALSE)
#else
#define ASSERT(x)
#endif

/* standard defines and types, not chess related */
typedef int BOOL;
enum { FALSE, TRUE };

#define ABS(a)					(((signed)(a) > 0) ? (a) : -(a))
#define MAX(a, b)				(((a) > (b)) ? (a) : (b))
#define MIN(a, b)				(((a) < (b)) ? (a) : (b))

typedef int ID;

/* General book-keeping and status enums */
typedef enum { INCOMPLETE, WHITE_MATES, BLACK_MATES, WHITE_RESIGNS,
	BLACK_RESIGNS, STALEMATE, FIFTY_DRAW, THREE_REP_DRAW, MATERIAL_DRAW,
	MUTUAL_DRAW } RESULT;
typedef enum { COORDINATE, SAN, LSAN } NOTATION;
typedef enum { INPUT_GET_MOVE, INPUT_USER_MOVE, INPUT_CHECK_MOVE } INPUT_MODE;
typedef enum { CMD_BAD, CMD_GOOD, CMD_STOP_SEARCH } CMD_RESULT;

/* chess board stuff */
typedef signed char COLOR;
enum { WHITE, BLACK };

#define COLOR_FLIP(s)			((s) ^ 1)

typedef unsigned char PIECE;
enum { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, EMPTY };

typedef signed char SQ_FILE;
typedef signed char SQ_RANK;
enum { FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H };
enum { RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8 };

typedef enum
{
	A1, B1, C1, D1, E1, F1, G1, H1,
	A2, B2, C2, D2, E2, F2, G2, H2,
	A3, B3, C3, D3, E3, F3, G3, H3,
	A4, B4, C4, D4, E4, F4, G4, H4,
	A5, B5, C5, D5, E5, F5, G5, H5,
	A6, B6, C6, D6, E6, F6, G6, H6,
	A7, B7, C7, D7, E7, F7, G7, H7,
	A8, B8, C8, D8, E8, F8, G8, H8,
	OFF_BOARD
} SQUARE;

typedef enum { DIR_HORI, DIR_A1H8, DIR_A8H1, DIR_VERT } DIRECTION;

/* Some board-transforming stuff */
#define SQ_FROM_RF(r, f)		((SQUARE)((r) << 3) | (f))
#define FILE_OF(s)				((SQ_FILE)(s) & 7)
#define RANK_OF(s)				((SQ_RANK)(s) >> 3)
#define SQ_FLIP(s)				((s) ^ 0x38)
#define SQ_FLIP_COLOR(s, c)		((s) ^ (0x38 * (c)))
#define SQ_COLOR(s)				(((s) & 1) == ((s) >> 3 & 1))

#define RANK_DISTANCE(f, t)		(ABS(RANK_OF(f) - RANK_OF(t)))
#define FILE_DISTANCE(f, t)		(ABS(FILE_OF(f) - FILE_OF(t)))
#define DISTANCE(f, t)			(MAX(RANK_DISTANCE((f), (t)),				\
									FILE_DISTANCE((f), (t))))

#define FILE_MASK(f)			(1 << (f))
#define RANK_MASK(r)			(1 << (r))

#define SQ_FILE_CHAR(s)			(FILE_OF(s) + 'a')
#define SQ_RANK_CHAR(s)			(RANK_OF(s) + '1')

/* castling */
typedef enum { CASTLE_NO = 0, CASTLE_WK = 1, CASTLE_BK = 2, CASTLE_WQ = 4,
	CASTLE_WB = 5, CASTLE_BQ = 8, CASTLE_BB = 10,
	CASTLE_ALL = 15 } CASTLE_RIGHTS;

#define CAN_CASTLE_KS(c, s)		(((c) >> (s)) & 1)
#define CAN_CASTLE_QS(c, s)		(((c) >> (s)) & 4)
#define CAN_CASTLE(c, s)		(((c) >> (s)) & 5)

/* bitboard stuff */
#define MASK(s)					(mask_bb[s])

#define SET_BIT(b, s)			((b) |= MASK(s))
#define CLEAR_BIT(b, s)			((b) &= ~MASK(s))
#define CLEAR_FIRST_BIT(b)		((b) &= (b) - 1)
#define BIT_IS_SET(b, i)		((b) >> (i) & 1)

#define MASK_RANK(r)			(0x00000000000000FFull << ((r) << 3))
#define MASK_FILE(f)			(0x0101010101010101ull << (f))
#define MASK_RANK_COLOR(r, c)	(0x00000000000000FFull <<					\
									(((r) ^ 7 * (c)) << 3))
#define SQ_COLOR_MASK(c)		((c) == WHITE ? 0x55AA55AA55AA55AAull :		\
									0xAA55AA55AA55AA55ull)

#define MASK_SHL				(~MASK_FILE(FILE_H))
#define MASK_SHR				(~MASK_FILE(FILE_A))

#define SHIFT_UL(b)				(((b) << 7) & MASK_SHL)
#define SHIFT_UP(b)				((b) << 8)
#define SHIFT_UR(b)				(((b) << 9) & MASK_SHR)
#define SHIFT_RT(b)				(((b) << 1) & MASK_SHR)
#define SHIFT_DR(b)				(((b) >> 7) & MASK_SHR)
#define SHIFT_DN(b)				((b) >> 8)
#define SHIFT_DL(b)				(((b) >> 9) & MASK_SHL)
#define SHIFT_LF(b)				(((b) >> 1) & MASK_SHL)
#define SHIFT_FORWARD(b, s)		((s) == WHITE ? SHIFT_UP(b) : SHIFT_DN(b))

#define BISHOP_ATTACKS(s, o)	(dir_attacks((s), (o), DIR_A1H8) |			\
									dir_attacks((s), (o), DIR_A8H1))
#define ROOK_ATTACKS(s, o)		(dir_attacks((s), (o), DIR_HORI) |			\
									dir_attacks((s), (o), DIR_VERT))
#define QUEEN_ATTACKS(s, o)		(BISHOP_ATTACKS((s), (o)) |					\
									ROOK_ATTACKS((s), (o)))

/* move definitions */
typedef unsigned int MOVE;

#define NO_MOVE					(0)
#define NULL_MOVE				(MOVE_FROM_FT(B1, B1)) /* Invalid move */

#define SET_TO(t)				(t)
#define SET_FROM(f)				((f) << 6)
#define SET_PROMOTE(p)			((p) << 12)
#define SET_SCORE(s)			((s) << 15)

#define MOVE_TO(m)				(((m) & 0x0000003F) >> 0)
#define MOVE_FROM(m)			(((m) & 0x00000FC0) >> 6)
#define MOVE_PROMOTE(m)			(((m) & 0x00007000) >> 12)
#define MOVE_SCORE(m)			(((m) & 0x0FFF8000) >> 15)

#define MAX_SCORE				((1 << 13) - 1)

#define MOVE_NOREDUCE			(0x10000000)
#define MOVE_NODELPRUNE			(0x20000000)
#define MOVE_NOEXTEND			(0x40000000)
#define MOVE_ISREDUCED			(0x80000000)

#define MOVE_COMPARE(m)			((m) & 0x7FFF)
#define MOVE_KEY(m)				((m) & 0x0FFF)

#define MOVE_FROM_FT(f, t)		(SET_FROM(f) | SET_TO(t))

/* Evaluation */
typedef signed short VALUE;

#define NO_VALUE				(-32767)
#define MATE					(32766)
#define DRAW					(0)

typedef unsigned char PHASE;

#define MAX_PHASE 128
#define OPENING_PHASE			(0)
#define ENDGAME_PHASE			MAX_PHASE
#define MIDGAME_PHASE			(MAX_PHASE / 2)

typedef struct
{
	PHASE phase;
	VALUE king_safety[2];
	VALUE passed_pawn[2];
	VALUE eval[2];
	VALUE full_eval;
	BITBOARD good_squares[2];
} EVAL_BLOCK;

typedef enum { KING_SIDE, QUEEN_SIDE } SHELTER;

/* Search stuff */
#define MAX_PLY					(128)
#define PLY						(4)

#define MAX_SEARCH_AGE			(256)

typedef struct
{
	MOVE move;
	BITBOARD nodes;
	BITBOARD last_nodes;
	int score;
	VALUE last_score;
	MOVE pv[MAX_PLY];
} ROOT_MOVE;

#define MAX_ROOT_MOVES			(256)

typedef enum { SELECT_HASH_MOVE, SELECT_THREAT_MOVE, SELECT_GEN_MOVES,
	SELECT_GEN_CAPS, SELECT_GEN_CHECKS, SELECT_MOVE } SELECT_STATE;

typedef enum { SEARCH_START, SEARCH_NULL_1, SEARCH_IID, SEARCH_JOIN, SEARCH_1,
	SEARCH_2, SEARCH_3, SEARCH_4, QSEARCH_START, QSEARCH_1, SEARCH_WAIT,
	SEARCH_CHILD_RETURN, SEARCH_RETURN } SEARCH_STATE;

typedef enum { NODE_ALL, NODE_CUT, NODE_PV } NODE_TYPE;

#define NODE_FLIP(n)			((n) ^ ((n) != NODE_PV))

typedef struct
{
	ID id;
	/* Parameters */
	VALUE alpha;
	VALUE beta;
	VALUE best_score;
	int depth;
	int ply;
	NODE_TYPE node_type;
	/* Node state */
	BITBOARD check;
	SELECT_STATE select_state;
	MOVE move;
	int moves;
	int pv_found;
	int extension;

	VALUE null_value;
	VALUE iid_value;
	VALUE return_value;
	BOOL threat;
	MOVE hash_move;

	EVAL_BLOCK eval_block;

	BOOL is_qsearch;
	BOOL move_made;
	MOVE *first_move;
	MOVE *next_move;
	MOVE *last_move;
	SEARCH_STATE search_state;
} SEARCH_BLOCK;

/*
Hash key. The structure for the 64-bit data structure is as follows:

bits  0 -  8:	depth
bits  9 - 10:	type: exact, upper bound, lower bound
bits 11 - 26:	eval
bits 27 - 45:	move
*/
typedef BITBOARD HASHKEY;

#define SET_HASH_DEPTH(d)		((BITBOARD)(d) & 0x01FF)
#define SET_HASH_AGE(a)			((BITBOARD)((a) & 0xFF) << 9)
#define SET_HASH_TYPE(t)		((BITBOARD)((t) & 0x03) << 17)
#define SET_HASH_THREAT(t)		((BITBOARD)((t) & 0x01) << 19)
#define SET_HASH_VALUE(s)		((BITBOARD)((s + MATE) & 0xFFFF) << 20)
#define SET_HASH_MOVE(m)		((BITBOARD)MOVE_COMPARE(m) << 36)

#define HASH_DEPTH(d)			((int)((d) & 0x01FF))
#define HASH_AGE(d)				((int)((d) >> 9 & 0xFF))
#define HASH_TYPE(d)			((HASH_BOUND_TYPE)((d) >> 17 & 0x03))
#define HASH_THREAT(d)			((BOOL)((d) >> 19 & 0x01))
#define HASH_VALUE(d)			((VALUE)((d) >> 20 & 0xFFFF) - MATE)
#define HASH_MOVE(d)			((MOVE)MOVE_COMPARE((d) >> 36))

typedef enum { HASH_NO_BOUND, HASH_LOWER_BOUND, HASH_UPPER_BOUND,
	HASH_EXACT_BOUND } HASH_BOUND_TYPE;

/* Structure for semi-path-dependent hashing. We don't completely hash the path,
	because we want to pick up "similar" paths. */
#define HASH_PATH_MOVE_COUNT	(2)

#define HASH_NON_PATH(h)		((h) & 0x0000FFFFFFFFFFFFull)
#define HASH_PATH(h)			((h) & 0xFFFF000000000000ull)

#define HASH_SLOT_COUNT			(4)

typedef struct
{
	struct
	{
		HASHKEY hashkey;
		BITBOARD data;
	} entry[HASH_SLOT_COUNT];
} HASH_ENTRY;

#define HASH_MB					((1 << 20) / sizeof(HASH_ENTRY))
#define HASH_KB					((1 << 10) / sizeof(HASH_ENTRY))

typedef struct
{
	HASHKEY hashkey;
	EVAL_BLOCK eval;
} EVAL_HASH_ENTRY;

#define EVAL_HASH_MB			((1 << 20) / sizeof(EVAL_HASH_ENTRY))
#define EVAL_HASH_KB			((1 << 10) / sizeof(EVAL_HASH_ENTRY))

typedef struct
{
	HASHKEY hashkey;
	BITBOARD passed_pawns;
	BITBOARD not_attacked[2];
	unsigned char open_files[2];
	VALUE king_shelter_value[2][2];
	VALUE bishop_color_value[2][2];
	VALUE eval[2];
} PAWN_HASH_ENTRY;

#define PAWN_HASH_MB			((1 << 20) / sizeof(PAWN_HASH_ENTRY))
#define PAWN_HASH_KB			((1 << 10) / sizeof(PAWN_HASH_ENTRY))


typedef struct
{
	MOVE move;
	PIECE capture;
	SQUARE ep_square;
	CASTLE_RIGHTS castle_rights;
	int fifty_count;
	HASHKEY hashkey;
	HASHKEY path_hashkey;
	HASHKEY pawn_hashkey;
} GAME_ENTRY;

#define MOVE_STACK_SIZE			(MAX_PLY * 128)
#define GAME_STACK_SIZE			(1024)

/* The big one. */
typedef struct BOARD
{
	BITBOARD piece_bb[6];
	BITBOARD color_bb[2];
	BITBOARD occupied_bb;
	PIECE piece[64];
	COLOR color[64];
	SQUARE king_square[2];
	int piece_count[2];
	int pawn_count[2];
	VALUE material[2];
	COLOR side_tm;
	COLOR side_ntm;
	SQUARE ep_square;
	CASTLE_RIGHTS castle_rights;
	int fifty_count;

	HASHKEY hashkey;
	HASHKEY path_hashkey;
	int move_number;
	GAME_ENTRY *game_entry;
	GAME_ENTRY game_stack[GAME_STACK_SIZE];
	MOVE move_stack[MOVE_STACK_SIZE];
	MOVE pv_stack[MAX_PLY][MAX_PLY];
	SEARCH_BLOCK search_stack[MAX_PLY];
	MOVE threat_move[MAX_PLY + 1];
	PAWN_HASH_ENTRY pawn_entry;

#ifdef SMP
	ID id;
	int split_ply_stack[MAX_PLY];
	int *split_ply;
	struct SPLIT_POINT *split_point_stack[MAX_PLY + 1];
	struct SPLIT_POINT **split_point;
#endif
} BOARD;

/* Some settings stuff */
typedef enum { DEFAULT, XBOARD, UCI, ANALYSIS, ANALYSIS_X, DEBUG,
	TUNE } PROTOCOL;
typedef enum { IDLE, NORMAL, PONDERING, ANALYZING, INFINITE, TESTING,
	DEBUGGING, BENCHMARKING } ENGINE_STATE;

/* String hashkey, used for mapping strings into arrays */
typedef unsigned int STR_HASHKEY;

/* A massive global struct with all the settings and related stuff. */
typedef struct
{
	/* IO */
	FILE *input_stream;
	FILE *output_stream;
	FILE *log_stream;
	char *input_buffer;
	BOOL source;

	/* Engine state */
	int process_count;
	PROTOCOL protocol;
	COLOR zct_side;
	BOOL game_over;
	RESULT game_result;
	ENGINE_STATE engine_state;
	NOTATION notation;
	BOOL computer;
	BOOL post;
	BOOL hard;
	BOOL use_book;
	BOOL ics_mode;
	BOOL stop_search;
	int ping;

	/* Search globals */
	MOVE ponder_move;
	int current_iteration;
	int search_age;
	ROOT_MOVE *next_root_move;
	ROOT_MOVE root_move_list[MAX_ROOT_MOVES];
	int root_move_count;

	/* Search options */
	int max_depth;
	int max_nodes;
	int output_limit;
	int lmr_threshold;
	int singular_extension;
	int singular_margin;
	int check_extension;
	int one_rep_extension;
	int threat_extension;
	int passed_pawn_extension;

	/* Search data, heuristics */
	int root_pv_counter;
	VALUE last_root_score;
	VALUE best_score_by_depth[MAX_PLY];
	int pv_changes_by_depth[MAX_PLY];

	unsigned int history_counter;
	unsigned int history_table[2][4096];
	MOVE killer_move[MAX_PLY][2];
	MOVE counter_move[2][4096][2];

	HASH_ENTRY *hash_table;

	BITBOARD hash_size;
	unsigned int qsearch_hash_size;
	unsigned int pawn_hash_size;
	unsigned int eval_hash_size;

	/* statistics and search data */
	BITBOARD nodes;
	BITBOARD q_nodes;
	int max_depth_reached;
	int nodes_per_check;
	int nodes_until_check;
	BITBOARD pv_nodes;
	BITBOARD fail_high_nodes;
	BITBOARD fail_low_nodes;
	BITBOARD pv_first;
	BITBOARD fail_high_first;
	BITBOARD hash_probes;
	BITBOARD hash_hits;
	BITBOARD hash_cutoffs;
	BITBOARD hash_entries_full;
	BITBOARD pawn_hash_probes;
	BITBOARD pawn_hash_hits;
	BITBOARD eval_hash_probes;
	BITBOARD eval_hash_hits;
	BITBOARD qsearch_hash_probes;
	BITBOARD qsearch_hash_hits;
	int check_extensions_done;
	int one_rep_extensions_done;
	int threat_extensions_done;
	int passed_pawn_extensions_done;
	int singular_extensions_done;
} GLOBALS;

#endif /* ZCT_H */
