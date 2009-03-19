/** ZCT/stats.c--Created 071608 **/

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
#include "stats.h"

#define STAT_SIZE	(1024)

int stat_count;
STAT stat_table[STAT_SIZE];

STAT *stat_lookup(char *stat_name);

/**
statistic():
Statistic handles dynamic string-based statistics. The name of an integer
variable is given, and a pointer to that integer is returned.
Created 071608; last modified 082408
**/
int *statistic(char *stat_name)
{
	STAT *stat;
	stat = stat_lookup(stat_name);
	stat->used = TRUE;

	return &stat->value;
}

/**
statistic_array():
statistic_array() handles dynamic string-based array statistics. The name of an
array variable is given along with an index, and a pointer to that array
element is returned.
Created 071608; last modified 082408
**/
int *statistic_array(char *stat_name, int index)
{
	STAT *stat;
	int new_size;

	stat = stat_lookup(stat_name);

	/* New array, allocate some entries. */
	if (!stat->used)
	{
		/* Try to guess a reasonably sized array. */
		stat->size = MAX(index * 2, 64);
		stat->value_a = malloc(stat->size * sizeof(int));
		for (new_size = 0; new_size < stat->size; new_size++)
			stat->value_a[new_size] = 0;
		stat->used = TRUE;
	}

	/* Test the size of the array. */
	if (index >= stat->size)
	{
		/* Double the array size until it is big enough. */
		new_size = stat->size;
		while (index >= new_size)
			new_size *= 2;
		/* Resize the array and initialize the entries with zeroes. */
		stat->value_a = realloc(stat->value_a, new_size * sizeof(int));
		for (; stat->size < new_size; stat->size++)
			stat->value_a[stat->size] = 0;
	}

	return &stat->value_a[index];
}

/**
initialize_statistics():
Iterate through the dynamic statistic table and zero out all the entries.
Created 071608; last modified 031609
**/
void initialize_statistics(void)
{
#ifdef USE_STATS
	int x;

	for (x = 0; x < STAT_SIZE; x++)
	{
		/* If there is an array allocated there, free it now. */
		if (stat_table[x].used && stat_table[x].value_a != NULL)
			free(stat_table[x].value_a);
		strcpy(stat_table[x].name, "");
		stat_table[x].value = 0;
		stat_table[x].value_a = NULL;
		stat_table[x].used = FALSE;
		stat_table[x].size = 0;
	}
#endif
}

/**
stat():
Stat_array handles dynamic string-based array statistics. The name of an
array variable is given, and a pointer to that array is returned.
Created 071608; last modified 031609
**/
void print_statistics(void)
{
#ifdef USE_STATS
	STAT *stat;
	char buffer[8192];
	char temp[64];
	int x;
	int s;
	int min_a;
	int max_a;

	for (x = 0; x < STAT_SIZE; x++)
	{
		stat = &stat_table[x];
		if (stat->used)
		{
			strcpy(buffer, "");

			sprint(temp, sizeof(temp), "%s = ", stat->name);
			strcat(buffer, temp);
			
			/* Print out a statistic array. */
			if (stat->value_a != NULL)
			{
				/* Find minimum and maximum values in the array. */
				for (min_a = 0; min_a < stat->size &&
					stat->value_a[min_a] == 0; min_a++)
					;
				for (max_a = stat->size - 1; max_a >= 0 &&
					stat->value_a[max_a] == 0; max_a--)
					;

				/* Start off the printing, including an ellipsis if we've
					truncated some zero values. */
				strcat(buffer, "{");
				if (min_a > 0)
					strcat(buffer, " ...,");

				/* Print out each value from min to max. */
				for (s = min_a; s <= max_a; s++)
				{
					if (s > min_a)
						strcat(buffer, ",");
					/* Print out the stat, or a blank (for easy reading). */
					if (stat->value_a[s] != 0)
					{
						sprint(temp, sizeof(temp), " %i->%i",
							s, stat->value_a[s]);
						strcat(buffer, temp);
					}
					else
						strcat(buffer, " -");

				}

				/* Finish up the printing. */
				if (max_a < stat->size - 1)
					strcat(buffer, ", ... ");
				strcat(buffer, " }");

				line_wrap(buffer, sizeof(buffer), 8);
				print("%s\n", buffer);
			}
			else
				print("%i\n", stat->value);
		}
	}
#endif
}

/**
stat():
Stat_array handles dynamic string-based array statistics. The name of an
array variable is given, and a pointer to that array is returned.
Created 071608; last modified 031709
**/
STAT *stat_lookup(char *stat_name)
{
	STR_HASHKEY hashkey;
	STR_HASHKEY old_hashkey;

	old_hashkey = hashkey = str_hashkey(stat_name, STAT_SIZE);

	do
	{
		/* Exact match of preexisting entry. */
		if (!strcmp(stat_table[hashkey].name, stat_name))
			return &stat_table[hashkey];
		/* New entry in unused slot. */
		else if (!strcmp(stat_table[hashkey].name, ""))
		{
			strcpy(stat_table[hashkey].name, stat_name);
			return &stat_table[hashkey];
		}
		hashkey = (hashkey + 1) % STAT_SIZE;
	} while (hashkey != old_hashkey);

	fatal_error("ERROR: no room for statistic: %s\n", stat_name);
	return NULL;
}
