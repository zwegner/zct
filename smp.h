/** ZCT/smp.h--Created 081305 **/

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
#include "stats.h"

#ifndef SMP_H
#define SMP_H

#ifdef SMP

#ifdef ZCT_POSIX
#	include <fcntl.h>
#	include <sys/ipc.h>
#	include <sys/mman.h>
#	include <sys/sem.h>
#endif /* ZCT_POSIX */

#include <sys/types.h>
#include <signal.h>

//#define DEBUG_SMP

#ifdef DEBUG_SMP
#	define SMP_DEBUG(x)			x
#else
#	define SMP_DEBUG(x)
#endif

#define MAX_SPLIT_POINTS		(MAX_CPUS * MAX_CPUS)
#define MAX_MESSAGES			(32)

#ifdef ZCT_OSX

#	include <libkern/OSAtomic.h>

typedef int PID;
typedef OSSpinLock LOCK_T[1];

#	define LOCK(l)				(OSSpinLockLock(l))
#	define UNLOCK(l)			(OSSpinLockUnlock(l))
#	define LOCK_INIT(l)			((l)[0] = 0)
#	define LOCK_FREE(l)			((l)[0] = 0)

#elif defined(ZCT_x86) /* ZCT_OSX */

#	ifdef ZCT_POSIX

typedef int PID;
typedef volatile int LOCK_T[1];

/* From Crafty. Bob says he took it from the Linux kernel. */
static void __inline__ LOCK(volatile int *lock)
{
	int dummy;
	asm __volatile__ (
		"1:          movl    $1, %0" "\n\t"
		"            xchgl   (%1), %0" "\n\t"
		"            testl   %0, %0" "\n\t"
		"            jz      3f" "\n\t"
		"2:          pause" "\n\t"
		"            movl    (%1), %0" "\n\t"
		"            testl   %0, %0" "\n\t"
		"            jnz     2b" "\n\t"
		"            jmp     1b" "\n\t"
		"3:" "\n\t"
		:"=&q" (dummy)
		:"q" (lock)
		:"cc", "memory");
}

#	elif defined(ZCT_WINDOWS) /* ZCT_POSIX */

/* We actually use threads on Windows, so hack up the Unix syscalls. */
/* This compatibility code is from Teemu Pudas. */
#		define fork()			CreateThread(NULL, 0, thread_init, (LPVOID)&x, 0, &z)
#		define kill(a, b)		TerminateThread(a, b)
#		define SIGKILL			(0)
#		define munmap(a, b)		free(a)
#		define LOCK(l) while (InterlockedExchange((l),1) != 0) while ((l)[0] == 1)

typedef HANDLE PID;
typedef volatile long LOCK_T[1];

DWORD WINAPI thread_init(LPVOID arg);

#	endif /* ZCT_WINDOWS */

#	define UNLOCK(l)			((l)[0] = 0)
#	define LOCK_INIT(l)			((l)[0] = 0)
#	define LOCK_FREE(l)			((l)[0] = 0)

#else /* ZCT_x86 */

#	error "SMP is not supported for this platform."

#endif /* !SMP */

#define CACHE_ALIGNED __attribute__((aligned(64)))
/* These are messages passed around by processors to coordinate DTS searching.
	Look for the functions that implement their actions to get a better idea
	of what each command does. */
typedef enum { SMP_INIT = 1, SMP_SEARCH, SMP_PARK, SMP_SPLIT, SMP_PERFT,
	SMP_UPDATE_HASH, SMP_IDLE } SMP_INPUT;
typedef enum { SMP_DONE = 1 } SMP_OUTPUT;
/* These are asynchronous commands, meaning that the sending processor does
	not wait for a reply. */
typedef enum { SMP_UPDATE = 1, SMP_UNSPLIT, SMP_STOP } SMP_MESSAGE_TYPE;

typedef struct CACHE_ALIGNED
{
	SMP_MESSAGE_TYPE message;
	unsigned int data;
} SMP_MESSAGE;

typedef struct SPLIT_POINT
{
	int n; /* the array slot in split_point[] that this is */
	ID id;
	volatile BOOL active;
	volatile BOOL no_moves_left;
	BOARD board;
	MOVE move_path[GAME_STACK_SIZE];
	MOVE *first_move;
	MOVE *last_move;
	int child_count;
	BOOL is_child[MAX_CPUS];
	BOOL update[MAX_CPUS];
	MOVE move_list[256];
	MOVE pv[MAX_PLY];
	SEARCH_BLOCK * volatile sb;
//	SPLIT_SCORE score;
	LOCK_T lock; /* Used for all data in each split point */
	LOCK_T move_lock; /* Used just for grabbing moves from the move list */

//	char padding[0x30];
} SPLIT_POINT CACHE_ALIGNED;

typedef struct CACHE_ALIGNED
{
	ID id;
	int ply;
#ifdef USE_STATS
	int depth;
	int moves;
	int moves_to_go;
	float fh_score;
	float depth_score;
	float moves_score;
	NODE_TYPE node_type;
#endif
	float score;
} SPLIT_SCORE;

typedef struct CACHE_ALIGNED
{
	SPLIT_SCORE split_score;
	SPLIT_SCORE sb_score;
} TREE_BLOCK;

/* Wrapper struct for each process, used for communication */
typedef struct CACHE_ALIGNED
{
	ID id;
	PID pid;
	BITBOARD nodes;
	BITBOARD q_nodes;
	int idle_time;
	int last_idle_time;
	int wait;
	BOOL idle;
	volatile SMP_MESSAGE message_queue[MAX_MESSAGES];
	volatile int message_count;
	/* The input and output variables are grouped with the data. Any message
		that is sent to another process can be accompanied by a word of data. */
	volatile SMP_INPUT input;
	volatile SMP_OUTPUT output;
	volatile unsigned int data;
	int wait_pipe[2];
	/* The tree structure holds information about potential split points. */
	TREE_BLOCK tree;

	LOCK_T lock;
	LOCK_T input_lock; /* Used for all input/output */
} SMP_BLOCK;

/* Wrapper struct for global smp data */
typedef struct CACHE_ALIGNED
{
	int splits_done;
	int stops_done;
	BOARD root_board;
	ID split_id;
	int split_count;
	BOOL return_flag;
	int return_value;
	MOVE return_pv[MAX_PLY];
	LOCK_T lock; /* Used for general smp data, split points, etc. */
	LOCK_T io_lock; /* Used for all input/output */
} SMP_DATA;

extern SMP_BLOCK *smp_block;
extern SPLIT_POINT *split_point;
extern SMP_DATA *smp_data;

/* smp.c */
void *shared_alloc(BITBOARD size);
void shared_free(void *mem, BITBOARD size);
void idle_loop(int id);
int smp_tell(int id, SMP_INPUT input, int data);
void smp_message(int id, SMP_MESSAGE_TYPE message, int data);
void smp_copy_to(SPLIT_POINT *to, GAME_ENTRY *ge, SEARCH_BLOCK *sb);
void smp_copy_from(SPLIT_POINT *from, SEARCH_BLOCK *sb);
void smp_copy_unsplit(SPLIT_POINT *sp_from, SEARCH_BLOCK *sb);
void smp_copy_root(BOARD *to, BOARD *from);
MOVE smp_select_move(void);
void handle_smp_messages(SEARCH_BLOCK **sb);
BOOL handle_smp_input(SEARCH_BLOCK *sb);
void set_idle(void);
void set_active(void);
void start_child_processors(void);
void stop_child_processors(void);
void make_active(int processor);
void make_idle(int processor);
void smp_cleanup(void);
void smp_cleanup_final(void);
void smp_cleanup_sig(int x);
/* smp2.c */
int split(SEARCH_BLOCK *sb, ID id);
SEARCH_BLOCK *attach(int sp);
void merge(SEARCH_BLOCK *sb);
SEARCH_BLOCK *update(SEARCH_BLOCK *sb, int id);
void detach(SEARCH_BLOCK *sb);
void stop(SEARCH_BLOCK *sb);
void unsplit(SEARCH_BLOCK *sb, int id);
void copy_search_state(SEARCH_BLOCK *sb, int process);
void smp_wait(SEARCH_BLOCK **sb);
void update_best_sb(SEARCH_BLOCK *sb);
int find_split_point(void);

#else

#define LOCK(x)
#define UNLOCK(x)

#endif /* SMP */

#endif /* SMP_H */
