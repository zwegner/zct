/** ZCT/cmd.h--Created 081206 **/

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

#ifndef CMD_H
#define CMD_H

typedef struct
{
	char *input; /* The original input string */
	char *old_input; /* The last input string */
	char *args; /* The input string separated into NUL-terminated strings; for internal use */
	char **arg; /* Pointers to the args in args[] */
	int arg_count;
} CMD_INPUT;

typedef struct
{
	STR_HASHKEY hashkey;
	char *cmd_name;
	char *help_text;
	int search_flag; /* Indicates if a search must be exited to execute the command. 1=always, 2=pondering */
	void (*cmd_func)(void);
} COMMAND;

#define CMD_TABLE_SIZE			(1024)

/* Command processing functions */
void cmd_parse(char *delim);

/* Default protocol */
void cmd_analyze(void);
void cmd_bench(void);
void cmd_bookc(void);
void cmd_bookl(void);
void cmd_bookp(void);
void cmd_debug(void);
void cmd_display(void);
void cmd_divide(void);
void cmd_eval(void);
void cmd_evalparam(void);
void cmd_exit(void);
void cmd_fen(void);
void cmd_flip(void);
void cmd_hash(void);
void cmd_hashprobe(void);
void cmd_help(void);
void cmd_history(void);
void cmd_load(void);
void cmd_moves(void);
#ifdef SMP
void cmd_mp(void);
#endif
void cmd_new(void);
void cmd_notation(void);
void cmd_omin(void);
void cmd_open(void);
void cmd_perf(void);
void cmd_perft(void);
void cmd_save(void);
void cmd_setfen(void);
void cmd_setname(void);
void cmd_sort(void);
void cmd_source(void);
void cmd_test(void);
void cmd_testeval(void);
void cmd_tune(void);
void cmd_uci(void);
void cmd_verify(void);
void cmd_xboard(void);

/* XBoard protocol */
void cmd_analyze_x(void);
void cmd_null(void);
void cmd_computer(void);
void cmd_easy(void);
void cmd_force(void);
void cmd_go(void);
void cmd_hard(void);
void cmd_ics(void);
void cmd_level(void);
void cmd_nopost(void);
void cmd_otim(void);
void cmd_ping(void);
void cmd_post(void);
void cmd_protover(void);
void cmd_rating(void);
void cmd_result(void);
void cmd_sd(void);
void cmd_setboard(void);
void cmd_st(void);
void cmd_time(void);
void cmd_undo(void);

/* UCI protocol */
void cmd_debug(void);
void cmd_isready(void);
void cmd_ponderhit(void);
void cmd_position(void);
void cmd_setoption(void);
void cmd_uci_stop(void);
void cmd_ucinewgame(void);
void cmd_uci_go(void);

/* Analysis protocol */
void cmd_stats(void);
void cmd_analyze_exit(void);

/* Debugging protocol */
void cmd_break(void);
void cmd_clear(void);
void cmd_continue(void);
void cmd_debug_exit(void);
void cmd_debug_go(void);
void cmd_debug_move(void);
void cmd_next(void);
void cmd_setbreak(void);
void cmd_stop(void);

/* Command tables */
extern COMMAND def_commands[];
extern COMMAND xb_commands[];
extern COMMAND uci_commands[];
extern COMMAND an_commands[];
extern COMMAND dbg_commands[];
extern COMMAND tune_commands[];

/* All input being interpreted */
extern CMD_INPUT cmd_input;

#endif /* CMD_H */
