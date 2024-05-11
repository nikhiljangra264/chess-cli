#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "chess_clock.h"

struct chess_clock_t {
	bool is_paused;
	board_t *board;
	pthread_t clock_thread;
};

static	void*	_run_chess_clock	(void* arg);


chess_clock_t* create_chess_clock (board_t *board, int plr1_time, int plr2_time) {
	chess_clock_t *clock = (chess_clock_t *) malloc(sizeof(chess_clock_t));
	clock->is_paused = true;
	clock->board = board;
	clock->board->plr_times[0] = plr1_time;
	clock->board->plr_times[1] = plr2_time;
	return clock;
}

bool start_chess_clock (chess_clock_t *clock) {
	int rc = pthread_create(&clock->clock_thread, NULL, &_run_chess_clock, (void*) clock);
	clock->is_paused = false;
	return rc;
}

void pause_chess_clock (chess_clock_t *clock) {
	clock->is_paused = true;
}

void resume_chess_clock (chess_clock_t *clock) {
	clock->is_paused = false;
}

/* int get_plr1_time (const chess_clock_t *clock) {
	return clock->plr1_time;
}

int get_plr2_time (const chess_clock_t *clock) {
	return clock->plr2_time;
} */

bool destroy_chess_clock (chess_clock_t *clock) {
	// thread may already have finished it's execution
	int rc = pthread_cancel(clock->clock_thread);

	free(clock);
	return true;
}

static void* _run_chess_clock (void* arg) {
	chess_clock_t *clock = (chess_clock_t *) arg;
	while (clock->board->plr_times[0] > 0 && clock->board->plr_times[1] > 0) {
		sleep(1);
		if (clock->is_paused)
			continue;
		if (clock->board->chance == WHITE)
			clock->board->plr_times[0]--;
		else
			clock->board->plr_times[1]--;
	}

	// TIMEOUT: change the result
	clock->board->result = (clock->board->plr_times[0] < clock->board->plr_times[1] ? BLACK_WON: WHITE_WON);
	return NULL;
}
