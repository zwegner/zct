/** ZCT/smp.c--Created 081305 **/

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

#ifdef SMP

SPLIT_POINT *split_point = NULL;
SMP_BLOCK *smp_block = NULL;
SMP_DATA *smp_data = NULL;
TREE_BLOCK *tree_block = NULL;
int split_point_size;
int smp_block_size;
int smp_data_size;

void smp_child_sig(int x);

/**
initialize_smp():
Initializes the smp functionality for the given number of processes.
Created 081305; last modified 103008
**/
void initialize_smp(int procs)
{
	int x;
	int y;
	int z;

	/* Set up the default signals. This is because the child processes
		inherit all of the parent's signal handlers, and we don't want that. */
	signal(SIGINT, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
//	signal(SIGCHLD, SIG_IGN);

	if (smp_data != NULL)
		smp_cleanup();
	else if (atexit(smp_cleanup) == -1 || atexit(smp_cleanup_final) == -1)
	{
		smp_cleanup();
		smp_cleanup_final();
		fatal_error("fatal error: atexit failed");
	}
	zct->process_count = procs;

	/* Allocate the shared memory to the various data structures needed. */
	split_point_size = sizeof(SPLIT_POINT) * MAX_SPLIT_POINTS;
	smp_block_size = sizeof(SMP_BLOCK) * procs;
	smp_data_size = sizeof(SMP_DATA);
	split_point = (SPLIT_POINT *)shared_alloc(split_point_size);
	smp_block = (SMP_BLOCK *)shared_alloc(smp_block_size);
	smp_data = (SMP_DATA *)shared_alloc(smp_data_size);
	
#define DBG(t,s,i) do{int p;\
		print("%s: %x\n", #t, sizeof(t));\
		for (p=0;p<i;p++){\
			print("%p",&s[p]);\
			if(p>0)\
				print(" %i",((BITBOARD)&s[p])-((BITBOARD)&s[p-1]));\
				print("\n");\
		}}while(0)

	/*
	DBG(SMP_BLOCK, smp_block, procs);
	DBG(SMP_DATA, smp_data, 1);
*/
	/* Initialize the data. */
	/* SMP data */
	smp_data->return_flag = FALSE;
	/* smp blocks */
	for (x = 0; x < procs; x++)
	{
		smp_block[x].id = x;
		smp_block[x].idle = FALSE;
		smp_block[x].last_idle_time = 0;
		smp_block[x].message_count = 0;
		smp_block[x].input = 0;
		smp_block[x].output = 0;
		
		for (y = 0; y < MAX_PLY; y++)
			initialize_split_score(&smp_block[x].tree.sb_score[y]);
//		initialize_split_score(&smp_block[x].tree.split_score);

		/* The split ID is id*MAX_CPUS+board.id, so instead of calculating
			that every time we split, we just start at board.id and increment
			by MAX_CPUS. */
		smp_block[x].split_id = x;

#ifdef ZCT_WINDOWS
		_pipe(smp_block[x].wait_pipe, 8, O_BINARY);
#else
		pipe(smp_block[x].wait_pipe);
#endif
	}

	/* split points */
	for (x = 0; x < MAX_SPLIT_POINTS; x++)
	{
		split_point[x].n = x;
		split_point[x].active = FALSE;
		split_point[x].child_count = 0;
		for (y = 0; y < MAX_CPUS; y++)
		{
			split_point[x].update[y] = FALSE;
			split_point[x].is_child[y] = FALSE;
		}
	}

	/* main processor's smp info */
	board.id = 0;
	board.split_ply = board.split_ply_stack;
	board.split_ply_stack[0] = -1;
	board.split_point = board.split_point_stack;
	board.split_point_stack[0] = NULL;

	/* Initialize the spin locks. */
	LOCK_INIT(smp_data->io_lock);
	LOCK_INIT(smp_data->lock);
	for (x = 0; x < procs; x++)
	{
		LOCK_INIT(smp_block[x].lock);
		LOCK_INIT(smp_block[x].input_lock);
	}
	for (x = 0; x < MAX_SPLIT_POINTS; x++)
	{
		LOCK_INIT(split_point[x].lock);
		LOCK_INIT(split_point[x].move_lock);
	}

	/* Now start the child processes. */
	for (x = 1; x < zct->process_count; x++)
	{
		/* Child process. */
		if ((y = fork()) == 0)
		{
			board.id = x;
			idle_loop(x);
		}
		/* Parent process. */
		else
		{
			smp_block[x].pid = y;
			smp_tell(x, SMP_INIT, 0);
		}
	}

	/* Set up the signals to use for the master processor. */
	if (board.id == 0)
	{
		signal(SIGINT, smp_cleanup_sig);
		signal(SIGTERM, smp_cleanup_sig);
//	signal(SIGCHLD, smp_cleanup_sig);
	}

	/* Now that the processors are spawned, make them idle until we start
		searching. */
	stop_child_processors();
}

/**
shared_alloc():
Allocate memory to be shared among multiple processors.
Created 123107; last modified 071108
**/
void *shared_alloc(BITBOARD size)
{
	void *mem;

#ifdef ZCT_POSIX
	if ((mem = mmap(0, size, PROT_READ | PROT_WRITE,
		MAP_ANON | MAP_SHARED /*| MAP_HASSEMAPHORE*/, -1, 0)) == MAP_FAILED)
		fatal_error("mmap failed");
#elif defined(ZCT_WINDOWS)
	/* This comment is from Teemu Pudas. I preserved it because it's hilarious.
		It dates back to when ZCT used processes on Windows. */
	mem = calloc(size,1); /* Let's not talk about Windows shared memory ever again. */
#endif
	return mem;
}

/**
shared_free():
Free memory that has previously been allocated in shared mode.
Created 110808; last modified 110808
**/
void shared_free(void *mem, BITBOARD size)
{
#ifdef ZCT_POSIX
	munmap(mem, size);
#elif defined(ZCT_WINDOWS)
	free(mem);
#endif
}

#ifdef ZCT_WINDOWS
/**
thread_init():
Initialize a thread on Windows. This is written by Teemu Pudas.
Created 071108; last modified 071108
**/
DWORD WINAPI thread_init(LPVOID arg)
{
	board.id = *(int *)arg;
	initialize_board(NULL);
	initialize_hash();
	board.split_ply = board.split_ply_stack;
	board.split_ply_stack[0] = -1;
	board.split_point = board.split_point_stack;
	board.split_point_stack[0] = NULL;
	idle_loop(board.id);
	return 0;
}
#endif 

/**
idle_loop():
Loop the child processes while waiting for work.
Created 081405; last modified 122208
**/
void idle_loop(int id)
{
	char buf[2];
	while (TRUE)
	{
		while (smp_block[id].input == 0)
			;
		SMP_DEBUG(print("cpu %i got input %i\n", id, smp_block[id].input));
		switch (smp_block[id].input)
		{
			case SMP_INIT:
				/* Nothing here at the moment... */
				smp_block[id].input = 0;
				smp_block[id].output = SMP_DONE;
				break;
			case SMP_SEARCH:
				/* Jump into the search() function, where we actively wait
					for nodes to search, and then search them... (duh) */
				smp_copy_root(&board, &smp_data->root_board);
				smp_block[id].input = 0;
				smp_block[id].output = SMP_DONE;
				root_entry = board.game_entry;
				set_idle();
				board.search_stack[0].search_state = SEARCH_CHILD_RETURN;
				board.search_stack[1].search_state = SEARCH_WAIT;
				search(&board.search_stack[1]);
				break;
			case SMP_UPDATE_HASH:
				/* Resize the hash tables. */
				initialize_hash();
				smp_block[id].input = 0;
				smp_block[id].output = SMP_DONE;
				break;
			case SMP_IDLE:
				/* After we are done searching, do a blocked read on our
					pipe. This is so that we don't consume CPU time. */
				smp_block[id].input = 0;
				smp_block[id].output = SMP_DONE;
				read(smp_block[id].wait_pipe[0], buf, 1);
				break;
			default:
				/* Just send back the message if we're idle. */
				smp_block[id].data = -1;
				smp_block[id].input = 0;
				smp_block[id].output = SMP_DONE;
				break;
		}
	}
}

/**
smp_tell():
Sets the appropriate locks, sends a message to the given processor, and waits
for the response. Note that this function is synchronous, that is, the sending
processor needs a response before it can continue searching. The return value
is the data sent back to the processor.
Created 082606; last modified 061208
**/
int smp_tell(int id, SMP_INPUT input, int data)
{
	int r;

	LOCK(smp_block[id].input_lock);
	smp_block[id].data = data;
	smp_block[id].output = 0;
	smp_block[id].input = input;
	while (smp_block[id].output == 0)
	{
		/* Check if anyone is telling us to split too... */
		if (smp_block[board.id].input == SMP_SPLIT)
		{
			smp_block[board.id].data = -1;
			smp_block[board.id].input = 0;
			smp_block[board.id].output = SMP_DONE;
		}
		/* Check if we need to exit based on the search being over. */
		if (smp_data->return_flag || smp_block[board.id].input == SMP_PARK)
			break;
	}

	/* Check for error: the search stopped or completed before the
		processor responded. */
	if (smp_block[id].output == 0)
	{
		smp_block[id].input = 0;
		UNLOCK(smp_block[id].input_lock);
		return -1;
	}

	smp_block[id].output = 0;
	smp_block[id].input = 0;
	r = smp_block[id].data;
	UNLOCK(smp_block[id].input_lock);
	return r;
}

/**
smp_message():
Sends a message to a given processor. Since these messages are asynchronous, and many processors
could be sending many messages at a time, there is a message queue. This goes in FIFO order.
Created 081007; last modified 012408
**/
void smp_message(int id, SMP_MESSAGE_TYPE message, int data)
{
	LOCK(smp_block[id].lock);
	/* As a worst case scenario, when the message queue fills up, we return. */
	if (smp_block[id].message_count >= MAX_MESSAGES)
	{
		UNLOCK(smp_block[id].lock);
		return;
	}
	smp_block[id].message_queue[smp_block[id].message_count].message = message;
	smp_block[id].message_queue[smp_block[id].message_count].data = data;
	smp_block[id].message_count++;
	UNLOCK(smp_block[id].lock);
}

/**
smp_copy_to():
In a parallel search, copies the board state (moves etc.) to shared memory
on a split.
Created 081706; last modified 022408
**/
void smp_copy_to(SPLIT_POINT *sp_to, GAME_ENTRY *ge, SEARCH_BLOCK *sb)
{
	int x;
	BOARD *to;
	GAME_ENTRY *game_p;
	MOVE *m_from;
	MOVE *m_to;
	SEARCH_BLOCK *s_from;
	SEARCH_BLOCK *s_to;

	to = &sp_to->board;

	SMP_DEBUG(
	char stuff[2560] = "Moves: ";
	char temp[2560] = "";
	)
	/* Copy the move history. */
	game_p = root_entry;
	m_to = sp_to->move_path;
	while (game_p < ge)
	{
		SMP_DEBUG
		(
			sprint(temp, 2560, " <%S%S>",
				MOVE_FROM(game_p->move), MOVE_TO(game_p->move));
			strcat(stuff, temp);
		)
		*m_to++ = game_p++->move;
	}
	sp_to->last_move = m_to;
	SMP_DEBUG(print("%s\n",stuff));

	/* Copy the move stack. */
	m_from = board.move_stack;
	m_to = to->move_stack;
	while (m_from < sb->last_move)
		*m_to++ = *m_from++;

	/* Copy threat moves. */
	for (x = 0; x < sb->ply + 1; x++)
		to->threat_move[x] = board.threat_move[x];
	/* Copy the PVs. */
	for (x = 0; x <= sb->ply; x++)
		copy_pv(to->pv_stack[x], board.pv_stack[x]);

	/* Copy the search state. */
	s_from = board.search_stack + 1;
	s_to = to->search_stack + 1;
	while (s_from <= sb)
	{
		*s_to = *s_from;
		s_to->first_move = (s_from->first_move - board.move_stack) +
			to->move_stack;
		s_to->next_move = (s_from->next_move - board.move_stack) +
			to->move_stack;
		s_to->last_move = (s_from->last_move - board.move_stack) +
			to->move_stack;
		s_to++;
		s_from++;
	}
	
	/* Copy the split point information. */
	for (x = 0; x <= board.split_ply - board.split_ply_stack; x++)
		to->split_ply_stack[x] = board.split_ply_stack[x];
	to->split_ply = (board.split_ply - board.split_ply_stack) +
		to->split_ply_stack;

	for (x = 0; x <= board.split_point - board.split_point_stack; x++)
		to->split_point_stack[x] = board.split_point_stack[x];
	to->split_point = (board.split_point - board.split_point_stack) +
		to->split_point_stack;

	SMP_DEBUG(print("smp_copy_to() cpu %i:\n%B",board.id,&board));
}

/**
smp_copy_from():
In a parallel search, copies the board state (moves etc.) from shared memory
when we are joining a split point.
Created 022308; last modified 022408
**/
void smp_copy_from(SPLIT_POINT *sp_from, SEARCH_BLOCK *sb)
{
	int x;
	BOARD *from;
	MOVE *m_from;
	MOVE *m_to;
	SEARCH_BLOCK *s_from;
	SEARCH_BLOCK *s_to;

	from = &sp_from->board;

	/* Copy the move history, and make the moves. */
	m_from = sp_from->move_path;
	while (m_from < sp_from->last_move)
		make_move(*m_from++);

	/* Copy the move stack. */
	m_from = from->move_stack;
	m_to = board.move_stack;
	while (m_from < sb->last_move)
		*m_to++ = *m_from++;

	/* Copy threat moves. */
	for (x = 0; x <= sb->ply + 1; x++)
		board.threat_move[x] = from->threat_move[x];
	/* Copy the PVs. */
	for (x = 0; x <= sb->ply; x++)
		copy_pv(board.pv_stack[x], from->pv_stack[x]);

	/* Copy the search state. */
/*
	s_from = sb - 1;
	s_to = board.search_stack + (sb - from->search_stack) - 1;
	while (s_from <= sb)
	{
		*s_to = *s_from;
		s_to->first_move = (s_from->first_move - from->move_stack) +
			board.move_stack;
		s_to->next_move = (s_from->next_move - from->move_stack) +
			board.move_stack;
		s_to->last_move = (s_from->last_move - from->move_stack) +
			board.move_stack;
		s_to++;
		s_from++;
	}*/
	s_from = from->search_stack + 1;
	s_to = board.search_stack + 1;
	while (s_from <= sb)
	{
		*s_to = *s_from;
		s_to->first_move = (s_from->first_move - from->move_stack) +
			board.move_stack;
		s_to->next_move = (s_from->next_move - from->move_stack) +
			board.move_stack;
		s_to->last_move = (s_from->last_move - from->move_stack) +
			board.move_stack;
		s_to++;
		s_from++;
	}
	
	/* Copy the split point information. */
	for (x = 0; x <= from->split_ply - from->split_ply_stack; x++)
		board.split_ply_stack[x] = from->split_ply_stack[x];
	board.split_ply = (from->split_ply - from->split_ply_stack) +
		board.split_ply_stack;

	for (x = 0; x <= from->split_point - from->split_point_stack; x++)
		board.split_point_stack[x] = from->split_point_stack[x];
	board.split_point = (from->split_point - from->split_point_stack) +
		board.split_point_stack;
	SMP_DEBUG(print("smp_copy_from() cpu %i:\n%B",board.id,&board));
}

/**
smp_copy_unsplit():
In a parallel search, copies the board state (moves etc.) from shared memory
when we are joining a split point.
Created 111508; last modified 111508
**/
void smp_copy_unsplit(SPLIT_POINT *sp_from, SEARCH_BLOCK *sb)
{
	BOARD *from;
	MOVE *m_from;
	MOVE *m_to;
	SEARCH_BLOCK *s_from;
	SEARCH_BLOCK *s_to;
	return;

	from = &sp_from->board;

	/* Copy the move stack. */
	m_from = from->move_stack;
	m_to = board.move_stack;
	while (m_from < sb->first_move)
		*m_to++ = *m_from++;

	/* Copy the PVs. */
//	for (x = 0; x < sb->ply; x++)
//		copy_pv(board.pv_stack[x], from->pv_stack[x]);

	/* Copy the search state. */
	s_from = from->search_stack + 1;
	s_to = board.search_stack + 1;
	while (s_from < sb)
	{
		*s_to = *s_from;
		s_to->first_move = (s_from->first_move - from->move_stack) +
			board.move_stack;
		s_to->next_move = (s_from->next_move - from->move_stack) +
			board.move_stack;
		s_to->last_move = (s_from->last_move - from->move_stack) +
			board.move_stack;
		s_to++;
		s_from++;
	}
}

/**
smp_copy_root():
At the beginning of a search, copy the root position so that we can coordinate
the split points around it.
Created 022308; last modified 022308
**/
void smp_copy_root(BOARD *to, BOARD *from)
{
	int x;
	GAME_ENTRY *game_p;

	for (x = 0; x < 6; x++)
		to->piece_bb[x] = from->piece_bb[x];
	for (x = 0; x < 2; x++)
		to->color_bb[x] = from->color_bb[x];
	to->occupied_bb = from->occupied_bb;
	for (x = A1; x < OFF_BOARD; x++)
	{
		to->piece[x] = from->piece[x];
		to->color[x] = from->color[x];
	}
	for (x = 0; x < 2; x++)
	{
		to->king_square[x] = from->king_square[x];
		to->piece_count[x] = from->piece_count[x];
		to->material[x] = from->material[x];
		to->pawn_count[x] = from->pawn_count[x];
	}
	to->side_tm = from->side_tm;
	to->side_ntm = from->side_ntm;
	to->ep_square = from->ep_square;
	to->castle_rights = from->castle_rights;
	to->fifty_count = from->fifty_count;
	to->hashkey = from->hashkey;
	to->path_hashkey = from->path_hashkey;
	to->pawn_entry.hashkey = from->pawn_entry.hashkey;
	to->move_number = from->move_number;

	game_p = from->game_stack;
	to->game_entry = to->game_stack;
	while (game_p < from->game_entry)
		*to->game_entry++ = *game_p++;
	SMP_DEBUG(print("smp_copy_root() cpu %i:\n%B",board.id,&board));
}

/**
handle_smp_messages():
During the search, if we receive a message from another processor, we drop
into this function. While we are here we process every message there is in the
queue before releasing the lock.
Created 081207; last modified 061308
**/
void handle_smp_messages(SEARCH_BLOCK **sb)
{
	int message;
	SMP_MESSAGE message_queue[MAX_MESSAGES];
	int message_count;

	LOCK(smp_block[board.id].lock);
	/* Copy the message queue to local memory, so that we can process them
		without tying up the other processors waiting to message it
		(which could cause a deadlock). */
	message_count = smp_block[board.id].message_count;
	for (message = 0; message < smp_block[board.id].message_count; message++)
		message_queue[message] = smp_block[board.id].message_queue[message];
	/* Reset the message queue. */
	smp_block[board.id].message_count = 0;
	UNLOCK(smp_block[board.id].lock);

	/* Now process the messages. */
	for (message = 0; message < message_count; message++)
	{
		switch (message_queue[message].message)
		{
			case SMP_UPDATE:
				*sb = update(*sb, message_queue[message].data);
				break;
			case SMP_UNSPLIT:
				unsplit(*sb, message_queue[message].data);
				break;
			case SMP_STOP:
				detach(*sb);
				break;
		}
	}
}

/**
handle_smp_input():
During the search, there are synchronous messages sent between processors that
need an immediate response. The sending processor must wait for the response.
We handle these inputs here. It returns TRUE if we need to exit the search.
Created 030109; last modified 030109
**/
BOOL handle_smp_input(SEARCH_BLOCK *sb)
{
	int r;

	/* See if someone is telling us to split. */
	if (smp_block[board.id].input == SMP_SPLIT)
	{
		/* If we're in an idle state, just send an error message back. */
		if (sb->search_state == SEARCH_CHILD_RETURN ||
				sb->search_state == SEARCH_WAIT)
		{
			smp_block[board.id].input = 0;
			smp_block[board.id].data = -1;
			smp_block[board.id].output = SMP_DONE;
		}
		/* Otherwise, just assume we can split. */
		else
		{
			/* Determine the ply and the search_block id. */
			r = split(sb, smp_block[board.id].data);
			smp_block[board.id].input = 0;
			smp_block[board.id].data = r;
			smp_block[board.id].output = SMP_DONE;
		}
	}
	/* We're not searching anymore, so return to the parent function and
	   wait without consuming CPU time. */
	else if (smp_block[board.id].input == SMP_PARK)
	{
		stop(sb);
		smp_block[board.id].input = 0;
		smp_block[board.id].data = 0;
		smp_block[board.id].output = SMP_DONE;
		return TRUE;
	}

	return FALSE;
}

/**
set_idle():
A wrapper function for setting the current processor's state to idle. We also
need to keep track of time for statistics.
Created 060808; last modified 060808
**/
void set_idle(void)
{
	TREE_BLOCK *tb;
	int x;

	/* Reset our public search score, so that nobody tries to tell us to split
		and also so that we'll know to regenerate a score when we go active
		again. */
	if (!smp_block[board.id].idle)
	{
		tb = &smp_block[board.id].tree;
		tb->best_id = -1;
		tb->best_score = -1;
		for (x = 0; x < MAX_PLY; x++)
			initialize_split_score(&tb->sb_score[x]);
	}

	SMP_DEBUG(print("cpu %i going idle\n", board.id));
	if (smp_block[board.id].last_idle_time == 0)
		smp_block[board.id].last_idle_time = get_time();
	smp_block[board.id].idle = TRUE;

}

/**
set_active():
A wrapper function for setting the current processor's state to active. Used in
conjunction with the above set_idle function.
Created 060808; last modified 060808
**/
void set_active(void)
{
	smp_block[board.id].idle = FALSE;
	if (smp_block[board.id].last_idle_time != 0)
	{
		SMP_DEBUG(print("cpu %i going active, time=%T\n",
			board.id, get_time() - smp_block[board.id].last_idle_time));
		smp_block[board.id].idle_time +=
			get_time() - smp_block[board.id].last_idle_time;
		smp_block[board.id].last_idle_time = 0;
	}
}

/**
start_child_processors():
Makes all child processors enter the search function and start waiting for work.
Created 110107; last modified 110908
**/
void start_child_processors(void)
{
	int p;
	int x;

	/* Copy the root board to all the various board structures lying around. */
	smp_copy_root(&smp_data->root_board, &board);
	for (x = 0; x < MAX_SPLIT_POINTS; x++)
		smp_copy_root(&split_point[x].board, &board);

	/* Tell every child processor to start up. */
	for (p = 1; p < zct->process_count; p++)
	{
		make_active(p);
		smp_tell(p, SMP_SEARCH, 0);
	}
}

/**
stop_child_processors():
Makes all child processors stop searching and wait for the next search.
Created 110107; last modified 110908
**/
void stop_child_processors(void)
{
	int p;

	for (p = 1; p < zct->process_count; p++)
	{
		smp_tell(p, SMP_PARK, 0);
		make_idle(p);
	}
}

/**
make_active():
The master process uses this function to tell child processors to wake up.
Created 110908; last modified 110908
**/
void make_active(int processor)
{
	write(smp_block[processor].wait_pipe[1], "s", 1);
}

/**
make_idle():
The master process uses this function to tell child processors to become idle.
Created 110908; last modified 110908
**/
void make_idle(int processor)
{
	smp_tell(processor, SMP_IDLE, 0);
}

static BOOL dead = FALSE;

/**
smp_cleanup():
Takes care of all process-related stuff at exit, i.e. kill children, destroy
shared memory...
Created 080606; last modified 031609
**/
void smp_cleanup(void)
{
	int x;

	if (board.id == 0 && !dead)
	{
		for (x = 1; x < zct->process_count; x++)
		{
			SMP_DEBUG(print("Killing process %i (pid=%i)...\n",
				x, smp_block[x].pid));
			kill(smp_block[x].pid, SIGTERM);
		}

		shared_free(smp_block, smp_block_size);
		shared_free(smp_data, smp_data_size);
	}
}

/**
smp_cleanup_final():
When ZCT is exiting for good, and not just cleaning up old SMP data, we need
to do some final cleaning. At the moment this is just freeing the hash table.
Created 110908; last modified 110908
**/
void smp_cleanup_final(void)
{
	if (board.id == 0 && !dead)
		shared_free(zct->hash_table, zct->hash_size * sizeof(HASH_ENTRY));
}

/* This is just for compatibility with signal(). */
void smp_cleanup_sig(int x)
{
	SMP_DEBUG(print("cpu %i received signal %i.\n",board.id,x));
	smp_cleanup();
	smp_cleanup_final();

	dead = TRUE;

	exit(EXIT_SUCCESS);
}

#endif /* SMP */
