/** ZCT/cluster.c--Created 122208 **/

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
#include "smp.h"
#include "cluster.h"

#ifdef CLUSTER

/**
initialize_cluster():
Created 122208; last modified 122208
**/
void initialize_cluster(void)
{
	ID cluster_id;
	PID pid;

	/* Initialize some cluster data to be shared among the processors in
		this node. */
	cluster_data = shared_alloc(sizeof(CLUSTER_DATA));
	/* Fork a helper process for cluster communications. */
	if ((pid = fork()) == 0)
	{
		/* child */
		cluster_helper_loop();
	}
	else
	{
		/* parent */

		/* return to regular engine loop, accept input, etc. */
	}

	/* Initialize MPI to attach to our cluster universe. */
	cluster_id = mpi_init();

	/* Master node. */
	if (cluster_id == 0)
	{
	}
	else
	{
		cluster_idle_loop(cluster_id);

		/* All done here. */
		mpi_exit();
	}
}

/**
cluster_idle_loop():
Loop the child cluster node while waiting for work. This is basically
equivalent to idle_loop(), but the entire node (potentially many CPUs) are
all together idling here.
Created 122208; last modified 122208
**/
void cluster_idle_loop(int id)
{
	CLUSTER_MESSAGE msg;
	char buf[2];
	while (TRUE)
	{
		/* Wait for a message from the helper. */
		while (!cluster_data->input)
			;
		CLUSTER_DEBUG(print("node %i got input %i\n", id, msg.input));
		switch (cluster_data->input)
		{
			case CLUSTER_INIT:
				smp_copy_root(&board, &cluster_data->root_board);
				/* Tell node 0 we're done. */
				cluster_send(0, CLUSTER_DONE, (void *)NULL);
				break;
			case CLUSTER_SEARCH:
				/* We've been given a command to search from another cluster
					node. Set up our node-local data and start searching. */
				smp_block[id].input = 0;
				smp_block[id].output = CLUSTER_DONE;
				root_entry = board.game_entry;
				set_idle();
				board.search_stack[0].search_state = SEARCH_CHILD_RETURN;
				board.search_stack[1].search_state = SEARCH_WAIT;
				search(&board.search_stack[1]);
				break;
			case CLUSTER_IDLE:
				/* After we are done searching, do a blocked read on our
					pipe. This is so that we don't consume CPU time. */
				smp_block[id].input = 0;
				smp_block[id].output = SMP_DONE;
				read(smp_block[id].wait_pipe[0], buf, 1);
				break;
			default:
				/* Just send back the message if we're idle. */
				if (!cluster_message_is_async(&msg))
				{
					smp_block[id].data = -1;
					smp_block[id].input = 0;
					smp_block[id].output = SMP_DONE;
				}
				break;
		}
	}
}

/**
cluster_split():
Creates a split point to be used across nodes in the cluster.
Created 010809; last modified 010809
**/
void cluster_split(SEARCH_BLOCK *sb)
{
	
}

/**
cluster_helper_loop():
When in cluster mode, we have to deal with a significant amount of
communications and such, and we don't want individual processors getting all
tied up with the MPI code. So we spawn a helper process that handles all the
intra-node communication.
Created 122208; last modified 122208
**/
void cluster_helper_loop(void)
{
	while (TRUE)
	{
		cluster_recv(&msg);
	}
}

/**
cluster_recv():
Created 122208; last modified 122208
**/
void cluster_recv(CLUSTER_MESSAGE *msg)
{
	MPI_Status status;

	MPI_Recv(msg, sizeof(CLUSTER_MESSAGE), MPI_BYTE, MPI_ANY_SOURCE,
		MPI_ANY_TAG, MPI_COMM_WORLD, &status);
}

/**
cluster_recv():
Created 122208; last modified 122208
**/
void cluster_send(int dest_node, CLUSTER_MESSAGE *msg, int size)
{
	
}

/**
mpi_init():
Created 122208; last modified 122208
**/
int mpi_init(void)
{
	int argc;
	char **argv;
	int cluster_nodes;
	int cluster_id;

	/* Initialize cluster communications. */
	argc = 0;
	argv = NULL;
	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &cluster_nodes);
	MPI_Comm_rank(MPI_COMM_WORLD, &cluster_id);

	print("nodes=%i id=%i\n",cluster_nodes,cluster_id);

	return cluster_id;
}

/**
mpi_exit():
Created 122208; last modified 122208
**/
void mpi_exit(void)
{
	MPI_Finalize();
	exit(EXIT_SUCCESS);
}

#endif
