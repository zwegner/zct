/** ZCT/cluster.h--Created 122208 **/

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

#include <mpi.h>

#ifndef CLUSTER_H
#define CLUSTER_H

#ifdef CLUSTER

/* These are messages passed around by the cluster nodes in order to
	coordinate the distributed search. They are all asynchronous. */
typedef enum
{
	CLUSTER_INIT = 1, CLUSTER_SEARCH, CLUSTER_RESULT, CLUSTER_SPLIT,
	CLUSTER_JOIN, CLUSTER_NEXT_MOVE
} CLUSTER_MESSAGE_TYPE;

/* Our various datatypes that are passed around as arguments to messages. */
typedef struct
{
	char fen[256];
} CLUSTER_SEARCH_ARG;

typedef struct
{
	VALUE best_score;
	MOVE pv[MAX_PLY];
} CLUSTER_RESULT_ARG;

typedef struct
{
	ID node_id;
	int ply;
} CLUSTER_SPLIT_ARG;

typedef struct
{
	MOVE move_path[MAX_PLY];
} CLUSTER_JOIN_ARG;

typedef struct
{
	CLUSTER_MESSAGE_TYPE message;
	/* The various cluster messages require different arguments as data. */
	union
	{
		CLUSTER_RESULT_ARG result;
		CLUSTER_SEARCH_ARG search;
		CLUSTER_SPLIT_ARG split;
		CLUSTER_JOIN_ARG join;
	};
} CLUSTER_MESSAGE;

typedef struct
{
	ID id;
	volatile BOOL active;
	volatile BOOL no_moves_left;
	BOARD board;
	MOVE move_path[GAME_STACK_SIZE];
	MOVE *first_move;
	MOVE *last_move;
	volatile int child_count;
	volatile BOOL is_child[MAX_CPUS];
	volatile BOOL update[MAX_CPUS];
	volatile MOVE move_list[256];
	MOVE pv[MAX_PLY];
	SEARCH_BLOCK * volatile sb;
	LOCK_T lock; /* Used for all data in each split point */
	LOCK_T move_lock;
} CLUSTER_SPLIT_POINT;

/* Wrapper struct for global smp data */
typedef struct
{
	volatile CLUSTER_MESSAGE message_queue[MAX_MESSAGES];
	volatile int message_count;
	BOARD root_board;
	BITBOARD nodes;
	BITBOARD q_nodes;
} CLUSTER_DATA;

extern CLUSTER_DATA *cluster_data;

/* cluster.c */
void initialize_cluster(void);
void cluster_idle_loop(int id);
void cluster_split(SEARCH_BLOCK *sb);
void cluster_helper_loop(void);
void cluster_recv(CLUSTER_MESSAGE *msg);
void cluster_send(int dest_node, CLUSTER_MESSAGE *msg, int size);
int mpi_init(void);
void mpi_exit(void);


#endif /* CLUSTER */

#endif /* CLUSTER_H */
