/** ZCT/tune.c--Created 101807 **/

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
#include "pgn.h"
#include <math.h>

void modify_eval(EVAL_PARAMETER *param, int stagnancy);
BOOL tune_pgn_func(void *pgn_arg, PGN_GAME *pgn_game);

EVAL_PARAMETER old_params[128];

/* Tuning data. We keep track of the number of positions we evaluate, as well
	as the result of the games. We then keep an average of the evaluation and
	an average of a certain parameter (or set of parameters). */
typedef struct
{
	unsigned int count;
	unsigned int points;
	signed long long total;
	signed long long param_total;
} TUNE_DATA;

#define TUNE_COUNT 11

int tune_class[TUNE_COUNT][2] =
{
	{ -MATE,	-450 },
	{ -449,		-350 },
	{ -349,		-250 },
	{ -249,		-150 },
	{ -149,		-50 },
	{ -49,		50 },
	{ 51,		150 },
	{ 151,		250 },
	{ 251,		350 },
	{ 351,		450 },
	{ 451,		MATE },
};

int param;

/**
tune_pgn():
Given a PGN with good quality games, tune the evaluation function in order to
better predict either the result of the games.
Created 110506; last modified 082408
**/
void tune_pgn(char *file_name)
{
	int game;
	int game_count;
	TUNE_DATA tune_data[TUNE_COUNT];
	VALUE eval;
	VALUE p_eval;
	float prob;
	int class;
	int count;

	/* Open the pgn file. */
	game_count = pgn_open(file_name);
	if (game_count == -1)
		return;
	if (game_count == 0)
	{
		print("%s: no games found.\n", file_name);
		return;
	}

	/* Initialize the tuning data. We keep the old params to back up later. */
	initialize_params(old_params);
	copy_params(eval_parameter, old_params);
	zct->input_buffer[0] = 0;
	zct->engine_state = ANALYZING;
	param = 8; /* bishop pair */

	/* Run the loop over the games over and over, determining the match
		score and tuning based on this. */
	while (TRUE)
	{
		/* Initialize. */
		hash_clear();
		for (class = 0; class < TUNE_COUNT; class++)
		{
			tune_data[class].count = 0;
			tune_data[class].points = 0;
			tune_data[class].total = 0;
			tune_data[class].param_total = 0;
		}
		
		/* Read each game and run the tuning procedure. */
		for (game = 1; game <= game_count; game++)
		{
			if (game % 100 == 0)
				print(".");
			pgn_load(game, tune_pgn_func, (void *)tune_data);
		}

		print("\nParam %i:\n", param);
		for (class = 0; class < TUNE_COUNT; class++)
		{
			if (tune_data[class].count == 0)
				continue;

			count = tune_data[class].count;
			eval = tune_data[class].total / count;
			p_eval = tune_data[class].param_total / count;
			prob = (float)tune_data[class].points / (count * 2);

			print("\tpos=%i score=%.2f eval=(%V %O) p eval=(%V %O)\n",
				count, prob, eval, eval, p_eval, p_eval);
		}
	//	bishop_pair_value += p_eval - eval;

		if (zct->input_buffer[0] != '\0')
		{
			if (command(zct->input_buffer) != CMD_BAD)
			{
				print_params(old_params);
				break;
			}
		}
	}
	copy_params(old_params, eval_parameter);
	free_params(old_params);
}

/**
modify_eval():
During an autotuning run, the evaluation parameters are modified and then test to see if the change is better or worse.
Created 101907; last modified 101907
**/
void modify_eval(EVAL_PARAMETER *param, int stagnancy)
{
	int change;
	int dim[2];
	/*
	int range;
	int lower_bound;
	*/
	int param_count;
	float factor;
	EVAL_PARAMETER *p;
	BITBOARD random;
	VALUE *value;

	for (param_count = 0; param[param_count].value != NULL; param_count++)
		;
	for (change = 0; change < 4 - MIN(stagnancy / 4, 1); change++)
	{
		/* Select a parameter to modify. */
		random = random_hashkey();
		/* Some common ranges:
			lb=0 r=1		Piece values
			lb=1 r=1		Piece square tables
			lb=4 r=4		Pawn structure
			lb=11 r=2		Mobility
			lb=13 r=7		King safety
		*/
//		lower_bound = 13;
//		range = 7;
		p = &param[((unsigned int)random) % param_count];
		dim[0] = dim[1] = 0;
		switch (p->dimensions)
		{
			case 0:
				value = p->value;
				break;
			case 1:
				dim[0] = (unsigned int)(random >> 16) % p->dimension[0];
				value = &p->value[dim[0]];
				break;
			case 2:
				dim[0] = (unsigned int)(random >> 16) % p->dimension[0];
				dim[1] = (unsigned int)(random >> 32) % p->dimension[1];
				value = &p->value[dim[0] * p->dimension[1] + dim[1]];
				break;
			default:
				print("Bad data in eval parameters: dimension %i == %i\n", p - param, p->dimensions);
				value = NULL;
		}
		/* Now modify the parameter by a factor between .618 and 1.618. This is
			to give an even distribution between increasing and decreasing with
			a range of factors of exactly 1. It's the golden ratio, for those
			of you who are nerdily inclined... */
		factor = (float)(random >> 48) / (2 * 0xFFFF) + 0.7807764064;
		print("Modifying %i[%i][%i] by %.3f: %i to %i\n",p-param,dim[0],dim[1],factor,*value,(int)((float)*value * factor));
		*value = (int)((float)*value * factor) + (random % 5 - 2);
	}
}

/**
tune_pgn_func():
This command plugs into the PGN parser to run the tuning procedure on the game.
Created 020908; last modified 020908
**/
BOOL tune_pgn_func(void *arg, PGN_GAME *pgn_game)
{
	TUNE_DATA *data = (TUNE_DATA *)arg;
	COLOR winner;
	BOOL drawn;
	EVAL_BLOCK eval_block;
	VALUE save_param;
	VALUE value_1;
	VALUE value_2;
	int class;

	/* Determine the game result. */
	winner = EMPTY;
	drawn = FALSE;
	if (!strncmp(pgn_game->tag.result, "1-0", 3))
		winner = WHITE;
	else if (!strncmp(pgn_game->tag.result, "0-1", 3))
		winner = BLACK;
	else
		drawn = TRUE;
	/* The first 10 moves are skipped, as they're typically handled by books. */
	if (board.game_entry <= board.game_stack + 10)
		return FALSE;

//	if (is_quiet())
	{
		/* First get the regular evaluation. */
	//	value_1 = evaluate(&eval_block);
	search_call(&board.search_stack[0], FALSE, 1 * PLY, 1, -MATE, MATE,
		board.move_stack, NODE_PV, SEARCH_RETURN);
	value_1 = search(&board.search_stack[1]);

		/* Now take out the influence of the eval parameter and reevaluate. */
		save_param = *eval_parameter[param].value;
		*eval_parameter[param].value = 0;
	//	value_2 = evaluate(&eval_block);
	search_call(&board.search_stack[0], FALSE, 1 * PLY, 1, -MATE, MATE,
		board.move_stack, NODE_PV, SEARCH_RETURN);
	value_2 = search(&board.search_stack[1]);
		*eval_parameter[param].value = save_param;

//			print("%V %V\n%B", value_1, value_2, &board);
//			getln(stdin);
		/* Now update the totals if the parameter has an effect. */
		if (value_1 != value_2 && value_1 > -MATE + MAX_PLY &&
			value_1 < MATE - MAX_PLY)
		{
			/* Find which class of data this data point is in. */
			for (class = 0; class < TUNE_COUNT; class++)
			{
				if (value_1 >= tune_class[class][0] &&
					value_1 <= tune_class[class][1])
					break;
			}
			if (class == TUNE_COUNT)
				fatal_error("Bad evaluation: %V\n%B", value_1, &board);

			data[class].count++;
			if (drawn)
				data[class].points++;
			else if (winner == board.side_tm)
				data[class].points += 2;

			data[class].total += value_1;
			data[class].param_total += value_1 - value_2;
		}
	}

	return FALSE;
}

/**
value_to_prob():
Given an evaluation in centipawns, convert it to a probability of winning
as a float between 0 and 1. This uses the approximate value 1 pawn=100 elo.
Created 082408; last modified 082408
**/
float value_to_prob(VALUE value)
{
	float exponent;
	float ten;

	exponent = (float)-value / 400.0;
	ten = pow(10.0, exponent);

	return 100 / (1 + ten);
}
