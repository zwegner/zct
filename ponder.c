/** ZCT/ponder.c--Created 102506 **/

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

/**
ponder():
This controls the ponder search, which is used when the opponent is on move. A move is selected
that is likely for the opponent to play, and then ZCT thinks as if this move has been played. If the
opponent plays this move, then we have saved time that can be used later.
Created 102506; last modified 011308
**/
void ponder(void)
{
start:
	/* Right now we simply take the second move from the last principal variation as the ponder move.
		If there isn't one, we just skip pondering. */
	if (zct->ponder_move == NO_MOVE)
		return;
	if (zct->protocol == XBOARD)
		print("Hint: %M\n", zct->ponder_move);
	else
	{
		prompt();
		print("%M (pondering)\n", zct->ponder_move);
	}

	make_move(zct->ponder_move);
	/* Check the result to see if the ponder move ends the game, and don't
	   ponder if it does. */
	if (check_result(FALSE))
	{
		unmake_move();
		return;
	}
	zct->engine_state = PONDERING;
	generate_root_moves();
	search_root();
	/* Engine_state is set to NORMAL on a ponder hit. We must send the move we made and update the clocks. */
	if (zct->engine_state == NORMAL)
	{
		zct->engine_state = IDLE;
		if (zct->protocol == XBOARD)
			print("move ");
		else
			prompt();
		print("%M\n", board.pv_stack[0][0]);
		make_move(board.pv_stack[0][0]);
		update_clocks();
		if (zct->ping)
		{
			print("pong %i\n", zct->ping);
			zct->ping = 0;
		}
		goto start;
	}
	unmake_move();
	zct->engine_state = IDLE;
}
