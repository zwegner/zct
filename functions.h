/** ZCT/functions.h--Created 070105 **/

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

#ifndef FUNCTIONS_H
#define FUNCTIONS_H

/* bit.c */
BITBOARD attacks_bb(PIECE piece, SQUARE square);
BITBOARD fill_up_right(BITBOARD piece, BITBOARD empty);
BITBOARD fill_up(BITBOARD piece, BITBOARD empty);
BITBOARD fill_up_left(BITBOARD piece, BITBOARD empty);
BITBOARD fill_right(BITBOARD piece, BITBOARD empty);
BITBOARD fill_left(BITBOARD piece, BITBOARD empty);
BITBOARD fill_down_right(BITBOARD piece, BITBOARD empty);
BITBOARD fill_down(BITBOARD piece, BITBOARD empty);
BITBOARD fill_down_left(BITBOARD piece, BITBOARD empty);
BITBOARD smear_up(BITBOARD g);
BITBOARD smear_down(BITBOARD g);
BITBOARD fill_attacks_knight(BITBOARD attackers);
BITBOARD fill_attacks_bishop(BITBOARD attackers, BITBOARD occupied);
BITBOARD fill_attacks_rook(BITBOARD attackers, BITBOARD occupied);
BITBOARD flood_fill_king(BITBOARD attackers, BITBOARD occupied, int moves);
int first_bit_8(unsigned char b);
void initialize_attacks(void);
BITBOARD dir_attacks(SQUARE from, BITBOARD occupied, DIRECTION dir);
/* book.c */
void book_update(char *pgn_file_name, char *book_file_name, int width,
	int depth, int win_percent);
void book_load(char *file_name);
BOOL book_probe(void);
/* check.c */
BOOL in_check(void);
BITBOARD check_squares(void);
BOOL is_attacked(SQUARE square, COLOR color);
BITBOARD attack_squares(SQUARE square, COLOR color);
BOOL move_gives_check(MOVE move);
BOOL is_pinned(SQUARE to, COLOR color);
void get_attack_bb_set(BITBOARD *checks, BITBOARD not_pinned[],
	BITBOARD *all_pinned);
BOOL is_mate(void);
void check_legality(MOVE *first, MOVE **last);
BOOL is_repetition(int limit);
BOOL is_quiet(void);
void generate_root_moves(void);
BOOL check_result(BOOL print_result);
/* cmd.c */
int command(char *input);
void set_cmd_input(char *input);
void initialize_cmds(void);
void display_help(void);
STR_HASHKEY str_hashkey(char *input, int size);
/* eval.c */
VALUE evaluate(EVAL_BLOCK *eval_block);
VALUE material_balance(void);
BOOL can_mate(COLOR color);
/* gen.c */
MOVE *generate_moves(MOVE *next_move);
MOVE *generate_captures(MOVE *next_move);
MOVE *generate_checks(MOVE *next_move);
MOVE *generate_evasions(MOVE *next_move, BITBOARD checkers);
MOVE *generate_legal_moves(MOVE *next_move);
/* hash.c */
BOOL hash_probe(SEARCH_BLOCK *sb, BOOL is_qsearch);
void hash_store(SEARCH_BLOCK *sb, MOVE move, VALUE value, HASH_BOUND_TYPE type, BOOL is_qsearch);
void stuff_pv(int depth, MOVE *pv, VALUE value);
int age_difference(int age);
void hash_clear(void);
void hash_print(void);
/* init.c */
void initialize_settings(void);
void initialize_data(void);
void initialize_hash(void);
void initialize_board(char *position);
void initialize_bitboards(void);
void initialize_hashkey(void);
void hash_alloc(BITBOARD hash_table_size);
/* initeval.c */
void initialize_eval(void);
/* input.c */
void read_line(void);
char *getln(FILE *stream);
int input_move(char *string, INPUT_MODE mode);
BOOL input_available(void);
/* make.c */
BOOL make_move(MOVE move);
void update_path_hashkey(MOVE move);
BOOL is_legal(MOVE move);
/* output.c */
char *move_string(MOVE move);
char *board_string(BOARD *board);
char *fen_string(BOARD *board);
char *bitboard_string(BITBOARD bitboard);
char *two_bitboards_string(BITBOARD bitboard_1, BITBOARD bitboard_2);
char *bitboard_hex_string(BITBOARD bitboard);
char *pv_string(MOVE *pv_start);
char *square_string(SQUARE square);
char *time_string(int time);
char *value_string(VALUE value);
char *value_prob_string(VALUE value);
char *search_block_string(SEARCH_BLOCK *sb);
char *search_state_string(SEARCH_STATE ss);
void display_search_line(BOOL final, MOVE *pv, VALUE value);
void display_search_header(void);
void line_wrap(char *string, int size, int column);
/* perft.c */
BITBOARD perft(int depth, MOVE *first_move);
BITBOARD perft_hash_lookup(int depth);
void perft_hash_store(int depth, BITBOARD nodes);
/* pgn.c */
int pgn_open(char *file_name);
void pgn_save(char *file_name);
/* ponder.c */
void ponder(void);
/* print.c */
void sprint(char *string, int max_length, char *text, ...);
void print(char *text, ...);
void sprint_(char *string, int max_length, char *text, va_list args);
#ifdef EGBB
typedef int (*PPROBE_EGBB) (int player, int w_king, int b_king,
							int piece1, int square1,
							int piece2, int square2,
							int piece3, int square3);

typedef void (*PLOAD_EGBB) (char* path,int cache_size,int load_options);
extern int LoadEgbbLibrary(char* main_path,int egbb_cache_size);
extern int probe_bitbases(VALUE * value);
#endif
/* rand.c */
HASHKEY random_hashkey(void);
void seed_random(HASHKEY hashkey);
/* search.c */
VALUE search(SEARCH_BLOCK *search_block);
/* searchroot.c */
void search_root(void);
void initialize_search(void);
void sum_counters(void);
void initialize_counters(void);
/* search2.c */
void heuristic(MOVE best_move, SEARCH_BLOCK *sb);
void heuristic_pv(MOVE best_move, SEARCH_BLOCK *sb);
BOOL moves_are_connected(MOVE move_1, MOVE move_2);
BOOL should_reduce(SEARCH_BLOCK *sb, MOVE move);
BOOL move_is_futile(SEARCH_BLOCK *sb, MOVE move);
int null_reduction(SEARCH_BLOCK *sb);
BOOL should_nullmove(SEARCH_BLOCK *sb);
int extend_move(SEARCH_BLOCK *sb);
void extend_node(SEARCH_BLOCK *sb);
BOOL delta_prune_move(SEARCH_BLOCK *sb, MOVE move);
void search_call(SEARCH_BLOCK *sb, BOOL is_qsearch, int depth, int ply,
	VALUE alpha, VALUE beta, MOVE *first_move, NODE_TYPE node_type,
	SEARCH_STATE search_state);
void search_return(SEARCH_BLOCK *sb, VALUE return_value);
void copy_pv(MOVE *to, MOVE *from);
BOOL search_maintenance(SEARCH_BLOCK **sb, VALUE *return_value);
BOOL search_check(void);
BOOL stop_search(void);
/* see.c */
VALUE see(SQUARE to, SQUARE from);
/* select.c */
void score_moves(SEARCH_BLOCK *sb);
void score_caps(SEARCH_BLOCK *sb);
void score_checks(SEARCH_BLOCK *sb);
MOVE select_move(SEARCH_BLOCK *sb);
MOVE select_qsearch_move(SEARCH_BLOCK *sb);
ROOT_MOVE *select_root_move(void);
/* smp.c */
#ifdef SMP
void initialize_smp(int procs);
#else
#define initialize_smp(p)
#endif
/* test.c */
void test_epd(char *file_name);
BOOL check_solutions(MOVE move, int depth);
void test_epd_eval(char *file_name);
void flip_board(void);
/* time.c */
unsigned int get_time(void);
void start_time(void);
int time_used(void);
BOOL time_is_up(void);
void set_time_limit(void);
void update_clocks(void);
void set_time_control(int moves, int base, int increment);
void reset_clocks(void);
void stop_clocks(void);
void set_zct_clock(int time);
void set_opponent_clock(int time);
/* tune.c */
void tune_pgn(char *file_name);
float value_to_prob(VALUE value);
/* unmake.c */
void unmake_move(void);
/* verify.c */
void verify(void);
BOOL move_is_valid(MOVE move);
/* zct.c */
void prompt(void);
void bench(void);
void fatal_error(char *text, ...);

#endif /* FUNCTIONS_H */
