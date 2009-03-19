/** ZCT/stats.h--Created 031609 **/

/** Copyright 2009 Zach Wegner **/
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

#ifndef	STATS_H
#define STATS_H

#define USE_STATS

#ifdef USE_STATS
#	define STAT_INC(s)				((*statistic(s))++)
#	define STATA_INC(s,i)			((*statistic_array((s), (i)))++)
#else
#	define STAT_INC(s)
#	define STATA_INC(s,i)
#endif

typedef struct
{
	char name[BUFSIZ];
	int value;
	int *value_a;
	int size;
	BOOL used;
	/* XXX add lock per stat? */
} STAT;

/* prototypes */
int *statistic(char *stat_name);
int *statistic_array(char *stat_name, int index);
void initialize_statistics(void);
void print_statistics(void);

#endif /* STATS_H */
