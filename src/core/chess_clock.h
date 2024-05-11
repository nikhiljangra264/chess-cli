#ifndef CHESS_CLOCK_H
#define CHESS_CLOCK_H

#include <stdbool.h>
#include "board.h"

typedef	struct chess_clock_t chess_clock_t;


chess_clock_t*	create_chess_clock	(board_t *board, int plr1_time, int plr2_time);
bool			start_chess_clock	(chess_clock_t *clock);
void			pause_chess_clock	(chess_clock_t *clock);
void			resume_chess_clock	(chess_clock_t *clock);
/* int				get_plr1_time		(const chess_clock_t *clock);
int				get_plr2_time		(const chess_clock_t *clock); */
bool			destroy_chess_clock (chess_clock_t *clock);

#endif
