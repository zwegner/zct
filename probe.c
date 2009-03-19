/** ZCT/probe.c--Created 508 **/
#include "zct.h"
#include "functions.h"
#include "globals.h"
#include "probe.h"

#ifdef EGBB

#ifdef ZCT_WINDOWS

#	include <windows.h>
#	define EGBB_NAME "egbbdll.dll"

#else /* ZCT_WINDOWS */

#	define _GNU_SOURCE
#	include <stdlib.h>
#	include <dlfcn.h>
#	define EGBB_NAME "egbbso.so"
#	define HMODULE void*
#	ifndef DL_LAZY
#		define DL_LAZY 1 /* TODO: Find the correct file to include */
#	endif /* RTLD_LAZY */
#	define LoadLibrary(x) dlopen(x,DL_LAZY)
#	define GetProcAddress dlsym
#	define FreeLibrary(x)

#endif

enum
{
	_EMPTY,
	_WKING,_WQUEEN,_WROOK,_WBISHOP,_WKNIGHT,_WPAWN,
	_BKING,_BQUEEN,_BROOK,_BBISHOP,_BKNIGHT,_BPAWN
};

static int ScorpioPiece[12] =
{
	_WPAWN,_WKNIGHT,_WBISHOP,_WROOK,_WQUEEN,_WKING,
	_BPAWN,_BKNIGHT,_BBISHOP,_BROOK,_BQUEEN,_BKING
};
#define SCORPIO_PIECE(type,color) ScorpioPiece[6*color+type]
enum
{
	LOAD_NONE,LOAD_4MEN,SMART_LOAD,LOAD_5MEN
};

static int egbb_load_type = LOAD_4MEN;
static PPROBE_EGBB probe_egbb;
BOOL egbb_is_loaded = FALSE;

int LoadEgbbLibrary(char* main_path, int egbb_cache_size)
{
	static HMODULE hmod;
	PLOAD_EGBB load_egbb;
	char path[512];

	strcpy(path,main_path);
	strcat(path,EGBB_NAME);
#ifdef ZCT_WINDOWS
	if (hmod)
		FreeLibrary(hmod);
#endif /* ZCT_WINDOWS */
	print("%s\n",path);
	if(hmod = LoadLibrary(path))
	{
		print("EgbbProbe loaded!\n");
//		load_egbb = (PLOAD_EGBB) GetProcAddress(hmod,"load_egbb_5men");
  //   	probe_egbb = (PPROBE_EGBB) GetProcAddress(hmod,"probe_egbb_5men");
//      load_egbb(main_path,egbb_cache_size,egbb_load_type);
//		print("EgbbProbe loaded!\n");
		return TRUE;
	}
	else
	{
		print("EgbbProbe failed to load! %s\n",EGBB_NAME);
		perror("asdfas");
		return FALSE;
	}
}

BOOL probe_bitbases(VALUE * value)
{
	PIECE piece[5];
	SQUARE square[5];
	SQUARE from;
	int count;
	COLOR color;
	PIECE piece_type;
	VALUE score;
	BITBOARD pieces;

	count = 0;
	piece[0] = _EMPTY;
	piece[1] = _EMPTY;
	piece[2] = _EMPTY;
	square[0] = _EMPTY;
	square[1] = _EMPTY;
	square[2] = _EMPTY;

	for (piece_type = 0; piece_type < KING; piece_type++) /* kings handled separately */
	{
		for (color = WHITE; color <= BLACK; color++)
		{
			pieces = board.piece_bb[piece_type] & board.color_bb[color];
			while (pieces)
			{
				from = first_square(pieces);
				square[count] = from;
				piece[count++] = SCORPIO_PIECE(piece_type,color);
				CLEAR_BIT(pieces,from);
			}
		}
	}

	score = probe_egbb(board.side_tm, board.king_square[WHITE], board.king_square[BLACK],
		piece[0],square[0],piece[1],square[1],piece[2],square[2]);
   if (score != _NOTFOUND)
   {
      *value = score;
      return TRUE;
   }
print("NOTFOUND!%B\n",&board);
   return FALSE;
} 

#endif /* EGBB */
