/** ZCT/regress.c--Created 080308 **/

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

typedef struct regression_test REG_TEST;

typedef void (*REG_TEST_FUNC)(REG_TEST *);

void perft_test(REG_TEST *type);
void move_sort_test(REG_TEST *type);
void see_test(REG_TEST *type);

struct regression_test
{
	const char *name;
	int tests_performed;
	int tests_passed;
	REG_TEST_FUNC test;
};

REG_TEST regression_tests[] =
{
	{ "Perft",			0, 0, perft_test },
	{ "Move Sorting",	0, 0, move_sort_test },
	{ "SEE",			0, 0, see_test },
	{ NULL,				0, 0, NULL }
};

/**
regress()
Regress runs a standard suite of regressive tests. These are designed to catch
some simple and some not-so-simple errors that can creep in during regular
modifications. These tests are designed to cover most aspects of ZCT, from
simple bugs to errors in chess logic.
Created 080308; last modified 082908
**/
void regress(void)
{
	int passed;
	REG_TEST *test;

	passed = 0;
	for (test = regression_tests; test->name != NULL; test++)
	{
		test->tests_performed = 0;
		test->tests_passed = 0;
		test->test(test);
		if (test->tests_passed == test->tests_performed)
			passed++;
	}
}

void perft_test(REG_TEST *type)
{
	struct perft_test_position
	{
		char *position;
		int depth;
		BITBOARD perft;
	};
}

void move_sort_test(REG_TEST *type)
{
	struct sort_test_position
	{
		char *position;
		/* Each move in this list comes in a certain order. The sort
			routine must place the moves in the given position that match
			up with moves here in the correct order. */
		MOVE moves[16];
	};
}

void see_test(REG_TEST *type)
{
	struct see_test_position
	{
		char *position;
		SQUARE from;
		SQUARE to;
		int value;
	};
}
