#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include "game.h"
#include "board.h"
#include "chess_engine.h"
#include "history.h"
#include "../ai/ai.h"
#include "../utils/common.h"
#include "../utils/file.h"
#include "chess_clock.h"


static	WINDOW			*game_scr;
static	WINDOW			*menu_scr;
static	WINDOW			*plr1_scr;
static	WINDOW			*plr2_scr;
static	WINDOW			*board_scr;
static	WINDOW			*hud_scr;
static	WINDOW			*game_over_scr;
static	int				term_h;
static	int				term_w;
static	bool			onboard;

typedef struct {
	const board_t		*board;
	const short			*sel_tile;
	const short			*cur_tile;
	history_t			*history;
	const player_t		plr1;
	const player_t		plr2;
	const chess_clock_t	*clock;
} display_data_t;


static	bool					is_human_chance		(const board_t *board, const player_t plr1, const player_t plr2);
static	bool					_play				(board_t *board, history_t *history);
static	void					undo_game			(board_t *board, history_t *history, chess_clock_t *clock);
static	enum game_return_code	game_over			(board_t *board, history_t *history);
static	void					del_game_wins		(void);
static	void*					refresh_display		(void* args);
static	void					draw_board			(const board_t *board, const short sel_tile[2], const short cur_tile[2]);
static	void					draw_tile			(int y, int x, bool is_cur, bool is_sel, bool is_avail);
static	void					show_history		(history_t *history);
static	void					show_player_info	(const board_t *board, const player_t plr1, const player_t plr2);
static	char					get_player_type_char	(const player_t plr);
static	void					show_menu			(bool is_undo_disabled);


enum game_return_code init_game(const game_settings_t game_settings) {
	enum game_return_code return_code = QUIT;

	// load this part from saved file to load a game
	board_t *board = (board_t *) calloc(1, sizeof(board_t));
	history_t *history = NULL;
	player_t plr1, plr2;
	chess_clock_t *clock = NULL;

	if (game_settings.game_mode != LOAD_MODE) {
		plr1 = game_settings.players[0];
		plr2 = game_settings.players[1];
		if (game_settings.time_limit != -1) {
			init_board(board, game_settings.time_limit * 60);
			history = create_history(plr1, plr2, game_settings.time_limit * 60);
			clock = create_chess_clock(board, game_settings.time_limit * 60, game_settings.time_limit * 60);
		} else {
			init_board(board, -1);
			history = create_history(plr1, plr2, -1);
		}
		// maybe start after first move !?
		if (clock)
			start_chess_clock(clock);
	} else {
		history = load_hstk(game_settings.load_timestamp);
		if (history == NULL)
			return INVALID_LOAD;
		get_players(history, &plr1, &plr2);
		copy_board(board, peek_board(history, 0));
		if (board->plr_times[0] != -1)
			clock = create_chess_clock(board, board->plr_times[0], board->plr_times[1]);
		// maybe start after first move !?
		if (clock)
			start_chess_clock(clock);
	}

	tile_t	**moves = NULL;
	int key = -1;
	onboard = true;
	short cur_tile[2] = {0, 0};
	short sel_tile[2] = {INVALID_ROW, INVALID_COL};

	getmaxyx(stdscr, term_h, term_w);

	// custom macro in utils/common.h to create new win with parameters associated with window's name
	initialize_with_box(game_scr);
	initialize_with_box(menu_scr);
	initialize_with_box(plr1_scr);
	initialize_with_box(plr2_scr);
	initialize_without_box(board_scr);
	initialize_with_box(hud_scr);


/* 	wrefresh(stdscr);
	wrefresh(game_scr);
	wrefresh(menu_scr);

	show_history(history);
	show_player_info(board, plr1, plr2, clock); */

	pthread_t display_thread;
	display_data_t display_data = {
		.board = board,
		.sel_tile = sel_tile,
		.cur_tile = cur_tile,
		.history = history,
		.plr1 = plr1,
		.plr2 = plr2,
		.clock = clock,
	};
	int rc = pthread_create(&display_thread, NULL, refresh_display, (void*) &display_data);

	// if cpu is white, trigger the first cpu move
	if (plr1.type != HUMAN)
		_play(board, history);
	//
	// initiallly clear these windows otherwise they print gibbrish on screen
	// because we won't be using wclear in display thread as it causes flicker and
	// will use werase instead.
	wclear(menu_scr);
	wclear(plr1_scr);
	wclear(plr2_scr);
	wclear(board_scr);
	wclear(hud_scr);

	while (true) {
/* 		show_player_info(board, plr1, plr2, clock);
		doupdate(); */
		if (key == KEY_RESIZE) {
			getmaxyx(stdscr, term_h, term_w);

			// custom macro in utils/common.h to resize, move and clear window
			translate_with_box(game_scr);
			translate_with_box(menu_scr);
			translate_with_box(plr1_scr);
			translate_with_box(plr2_scr);
			translate_without_box(board_scr);
			translate_with_box(hud_scr);
			wclear(stdscr);
			
		} else if (onboard) {
			// handle menu keys
			if (key == 'q') {
				break;
			} else if (key == 'u') {
				undo_game(board, history, clock);
				sel_tile[0] = INVALID_ROW;
				sel_tile[1] = INVALID_COL;
			} else if (key == 's') {
				save_hstk(history);
			} else if (key == 'e') {
				export_pgn(history);
			} else if (key == 'a') {
				SETTINGS_UNICODE_MODE = !SETTINGS_UNICODE_MODE;
			}

			// handle board events
			else if (is_human_chance(board, plr1, plr2) && (key == '\n' || key == '\r' || key == KEY_ENTER)) {
// 			else if (key == '\n' || key == '\r' || key == KEY_ENTER) {
				if (sel_tile[0] != INVALID_ROW && sel_tile[1] != INVALID_ROW) {
					/* TODO: insert logic to check if selected move is of players own piece and
					 * not of opponent's piece that the player was able to select in it's prev move.
					 */
					if (move_piece(board, cur_tile, sel_tile, history)) {
						if (board->result != PENDING) { // result is calculated in move_piece
							show_history(history);
							if ((return_code = game_over(board, history)) != CONTINUE)
								break;
						}
						if(!_play(board, history)) {
							fprintf(stderr, "some error occured during _play...");
							return_code = PLAY_ERROR;
							break;
						}
					}

					clear_dest(board);
					sel_tile[0] = INVALID_ROW;
					sel_tile[1] = INVALID_COL;
					if (moves != NULL) {
						free(moves);
						moves = NULL;
					}
				} else if (board->tiles[cur_tile[0]][cur_tile[1]].piece != NULL && (board->tiles[cur_tile[0]][cur_tile[1]].piece->face & COLOR_BIT) == board->chance){
					sel_tile[0] = cur_tile[0];
					sel_tile[1] = cur_tile[1];
					moves = find_moves(board, &board->tiles[sel_tile[0]][sel_tile[1]], history);
					set_dest(board, moves);
				}
			} else if ( key == KEY_UP || key == 'k') {
				if (cur_tile[0] < 7) cur_tile[0]++;
			} else if (key == KEY_DOWN || key == 'j') {
				if (cur_tile[0] > 0) cur_tile[0]--;
			} else if (key == KEY_LEFT || key == 'h') {
				if (cur_tile[1] > 0) cur_tile[1]--;
			} else if (key == KEY_RIGHT || key == 'l') {
				if (cur_tile[1] < 7) cur_tile[1]++;
			}
		}

/* 		draw_board(board, sel_tile, cur_tile);
		show_history(history);
		show_player_info(board, plr1, plr2, clock);
		show_menu();

		wrefresh(stdscr);
		wrefresh(game_scr);
		wrefresh(menu_scr);
		wrefresh(plr1_scr);
		wrefresh(plr2_scr);
		wrefresh(board_scr);
		wrefresh(hud_scr); */

		key = wgetch(game_scr);
	}

	pthread_cancel(display_thread);
	pthread_join(display_thread, NULL);

	delete_board(board);
	delete_history(history);
	del_game_wins();

	return return_code;
}


static bool is_human_chance (const board_t *board, const player_t plr1, const player_t plr2) {
	return ((board->chance == WHITE && plr1.type == HUMAN) || (board->chance == BLACK && plr2.type == HUMAN));
}


static bool _play (board_t *board, history_t *history) {
	player_t players[2];
	get_players(history, players, players+1);
	int turn = (board->chance == BLACK ? 1: 0);
	if (players[turn].type == HUMAN)
		return true;

	// create condition for other types

	// AI
	// players[turn].type == AI_LVLx
	return ai_play(board, history);
}


static void undo_game (board_t *board, history_t *history, chess_clock_t *clock) {
	// if clock, pause it first
	if (clock)
		pause_chess_clock(clock);
	player_t players[2];
	get_players(history, players, players + 1);
	undo(history);
	// extra undo for AI
	if (players[0].type != HUMAN || players[1].type != HUMAN)
		undo(history);
	const board_t *prev_board = peek_board(history, 0);
	if (prev_board == NULL)
		init_board(board, get_time_limit(history));
	else
		copy_board(board, prev_board);
	clear_dest(board);
	// if clock, resume it
	if (clock)
		resume_chess_clock(clock);
}


static enum game_return_code game_over (board_t *board, history_t * history) {
	if (board->result == PENDING)
		return CONTINUE;

	initialize_with_box(game_over_scr);

	bool is_saved = false;
	bool is_pgn_exported = false;

	const int NO_OF_OPTS = 5;
	const int OPTS_SIZE = 14;

	enum {UNDO_OPT, SAVE_OPT, EXPORT_PGN_OPT, RESTART_OPT, MAIN_MENU_OPT};

	// initialize options
	char options[NO_OF_OPTS][OPTS_SIZE+1];
	snprintf(options[UNDO_OPT], OPTS_SIZE, "%-*s", OPTS_SIZE-1, "Undo");
	snprintf(options[SAVE_OPT], OPTS_SIZE, "%-*s", OPTS_SIZE-1, "Save");
	snprintf(options[EXPORT_PGN_OPT], OPTS_SIZE, "%-*s", OPTS_SIZE-1, "Export PGN");
	snprintf(options[RESTART_OPT], OPTS_SIZE, "%-*s", OPTS_SIZE-1, "Restart");
	snprintf(options[MAIN_MENU_OPT], OPTS_SIZE, "%-*s", OPTS_SIZE-1, "Main Menu");
	int selected_opt = 0;

	const int RESULT_SIZE = 20;

	char result[RESULT_SIZE];
	if (board->result == WHITE_WON)
		sprintf(result, "%s", "White Won");
	else if (board->result == BLACK_WON)
		sprintf(result, "%s", "Black Won");
	else if (board->result == STALE_MATE)
		sprintf(result, "%s", "Stale Mate");
	// error
	else
		exit(EXIT_FAILURE);

	wattron(game_over_scr, A_UNDERLINE);
	mvwaddnstr(game_over_scr, 1, game_over_scr_w/2 - strlen(result)/2, result, RESULT_SIZE);
	wattroff(game_over_scr, A_UNDERLINE);

	int key = -1;
	while (true) {
		if (key == KEY_RESIZE) {
			getmaxyx(stdscr, term_h, term_w);
			translate_with_box(game_over_scr);
			wclear(stdscr);
		}

		if (key == KEY_UP || key == 'k') {
			selected_opt = (selected_opt-1 + NO_OF_OPTS) % NO_OF_OPTS;
			while ((selected_opt == SAVE_OPT && is_saved) || (selected_opt == EXPORT_PGN_OPT && is_pgn_exported))
				selected_opt = (selected_opt-1 + NO_OF_OPTS) % NO_OF_OPTS;
		} else if (key == KEY_DOWN || key == 'j') {
			selected_opt = (selected_opt+1) % NO_OF_OPTS;
			while ((selected_opt == SAVE_OPT && is_saved) || (selected_opt == EXPORT_PGN_OPT && is_pgn_exported))
				selected_opt = (selected_opt+1) % NO_OF_OPTS;
		} else if (key == '\n' || key == '\r' || key == KEY_ENTER) {
			if (selected_opt == UNDO_OPT) {
				undo_game(board, history, clock);
				return CONTINUE;
			} else if (selected_opt == SAVE_OPT) {
				is_saved = save_hstk(history);
				if (is_saved) {
					snprintf(options[SAVE_OPT], OPTS_SIZE, "%-*s", OPTS_SIZE-1, "Saved");
				}
			} else if (selected_opt == EXPORT_PGN_OPT) {
				is_pgn_exported = export_pgn(history);
				if (is_pgn_exported) {
					snprintf(options[EXPORT_PGN_OPT], OPTS_SIZE, "%-*s", OPTS_SIZE-1, "PGN Exported");
				}
			} else if (selected_opt == RESTART_OPT) {
				delwin(game_over_scr);
				return RESTART;
			} else if (selected_opt == MAIN_MENU_OPT) {
				wclear(game_over_scr);
				wrefresh(game_over_scr);
				delwin(game_over_scr);
				return QUIT;
			}
		}

		for (int i=0; i<NO_OF_OPTS; i++) {
			if (i == selected_opt)
				wattron(game_over_scr, A_STANDOUT);
			else
				wattroff(game_over_scr, A_STANDOUT);

			mvwaddnstr(game_over_scr, game_over_scr_h/2 - NO_OF_OPTS/2 + i, game_over_scr_w/2 - (OPTS_SIZE-1)/2, options[i], OPTS_SIZE);
		}

		wrefresh(stdscr);
		wrefresh(game_over_scr);
		key = wgetch(game_over_scr);
	}

	// failsafe
	return QUIT;
}


static void del_game_wins (void) {
	wclear(game_scr);
	wrefresh(game_scr);
	delwin(game_scr);
	wclear(menu_scr);
	wrefresh(menu_scr);
	delwin(menu_scr);
	wclear(plr1_scr);
	wrefresh(plr1_scr);
	delwin(plr1_scr);
	wclear(plr2_scr);
	wrefresh(plr2_scr);
	delwin(plr2_scr);
	wclear(board_scr);
	wrefresh(board_scr);
	delwin(board_scr);
	wclear(hud_scr);
	wrefresh(hud_scr);
	delwin(hud_scr);
}


static void* refresh_display (void* args) {
	while (true) {
		display_data_t *display_data = (display_data_t *) args;
		// don't display simulated moves by ai
		if (display_data->board->is_fake)
			continue;

		draw_board(display_data->board, display_data->sel_tile, display_data->cur_tile);
		show_history(display_data->history);
		show_player_info(display_data->board, display_data->plr1, display_data->plr2);

// 		bool is_undo_disabled = (display_data->clock ? true: false);
		bool is_undo_disabled = false;
		show_menu(is_undo_disabled);

		usleep(100);
	}
	return NULL;
}


static void draw_board (const board_t *board, const short sel_tile[2], const short cur_tile[2]) {
	werase(board_scr);

	for (short i=0; i<=8; i++) {
		for (short j=0; j<=8; j++) {
			if (i == 8 && j == 0) continue;
			int tile_row = 7-i, tile_col = j-1;

			wattroff(board_scr, A_BOLD);
			if ((tile_row + tile_col)%2 != 0)
				wattron(board_scr, A_STANDOUT);
			if (i < 8 && j > 0)
				draw_tile(i, j, (tile_row == cur_tile[0] && tile_col == cur_tile[1]), (tile_row == sel_tile[0] && tile_col == sel_tile[1]), board->tiles[tile_row][tile_col].can_be_dest);
			wattroff(board_scr, A_STANDOUT);

			if (i == 8) {
				char c[2] = {0};
				c[0] = 'A'+(j-1);
				mvwaddstr(board_scr, i*(tile_size_h + TILE_PAD_h) + tile_size_h/2, j*(tile_size_w + TILE_PAD_w) + tile_size_w/2, c);
			} else if (j == 0) {
				char a[20];
				memset(a, 0, 20);
				itoa(8-i, a);
				mvwaddstr(board_scr, i*(tile_size_h + TILE_PAD_h) + tile_size_h/2, j*(tile_size_w + TILE_PAD_w) + tile_size_w/2, a);
			} else {
				wattron(board_scr, A_BOLD);
				if ((tile_row + tile_col)%2 != 0) wattron(board_scr, A_STANDOUT);

				wchar_t piece[2] = {0};
				piece[0] = get_piece_face(board->tiles[tile_row][tile_col].piece);
				mvwaddwstr(board_scr, i*(tile_size_h + TILE_PAD_h) + tile_size_h/2, j*(tile_size_w + TILE_PAD_w) + tile_size_w/2, piece);

				wattroff(board_scr, A_STANDOUT);
			}
		}
	}

// 	wrefresh(board_scr);
	wrefresh(board_scr);
}


static void draw_tile(int y, int x, bool is_cur, bool is_sel, bool can_be_dest) {
	y *= (tile_size_h + TILE_PAD_h);
	x *= (tile_size_w + TILE_PAD_w);
	int w = tile_size_w, h = tile_size_h;
	for (int i=0; i<h; i++) {
		for (int j=0; j<w; j++) {
			mvwaddch(board_scr, y+i, x+j, ' ');
		}
	}

	if (!onboard) return;

	wattron(board_scr, A_BOLD);
	if (is_sel) {
		mvwhline(board_scr, y, x, 'x', w);
		mvwhline(board_scr, y+h-1, x, 'x', w);
		mvwvline(board_scr, y, x, 'x', h);
		mvwvline(board_scr, y, x+w-1, 'x', h);
	}
	if (can_be_dest) {
		mvwhline(board_scr, y, x, '\'', w);
		mvwhline(board_scr, y+h-1, x, '\'', w);
		mvwvline(board_scr, y, x, '\'', h);
		mvwvline(board_scr, y, x+w-1, '\'', h);
	}
	if (is_cur) {
		mvwaddch(board_scr, y, x, ACS_ULCORNER);
		mvwaddch(board_scr, y, x+w-1, ACS_URCORNER);
		mvwaddch(board_scr, y+h-1, x, ACS_LLCORNER);
		mvwaddch(board_scr, y+h-1, x+w-1, ACS_LRCORNER);
	}
	wattroff(board_scr, A_BOLD);
}


static void show_history (history_t *history) {
	if (is_fake_history(history))
		return;
	werase(hud_scr);
	box(hud_scr, 0, 0);

	int sz = min(get_size(history), hud_scr_h - 2);
	float move_no = get_size(history) - sz + 0.5;
	int move_display_max_size = MAX_MOVE_NOTATION_SIZE + 5;
	for (int i=1; i<=sz; i++) {
		char s[move_display_max_size];
		snprintf(s, move_display_max_size, "%.1f  %s", move_no, peek_move(history, sz-i));
		mvwaddnstr(hud_scr, i, 1, s, move_display_max_size);
		move_no += 0.5;
	}

// 	wrefresh(hud_scr);
	wrefresh(hud_scr);
}


static void show_player_info (const board_t *board, const player_t plr1, const player_t plr2) {
	/*
	 *	FORMAT TO DISPLAY PLAYER INFO
	 *
	 *	CAPTRUED INFO - PLAYER_TYPE - PLAYER_NAME - TURN - TIMER
	 */

	werase(plr1_scr);
	box(plr1_scr, 0, 0);

	werase(plr2_scr);
	box(plr2_scr, 0, 0);

	// use SETTINGS_UNICODE_MODE
	bool is_unicode = SETTINGS_UNICODE_MODE;
	const int V_OFFSET = 1, H_OFFSET = 1, WSTR_SIZE = 7;
	wchar_t wstr[WSTR_SIZE];
	for (int i=0; i<PIECE_TYPES; i++) {
		if ((1 << i) & KING) continue;	// king is never captured
		swprintf(wstr, WSTR_SIZE, L"%lcx%hd", PIECES[is_unicode][1][i], board->captured[1][i]);
		mvwaddnwstr(plr1_scr, V_OFFSET, (i*WSTR_SIZE) + H_OFFSET, wstr, WSTR_SIZE);
		swprintf(wstr, WSTR_SIZE, L"%lcx%hd", PIECES[is_unicode][0][i], board->captured[0][i]);
		mvwaddnwstr(plr2_scr, V_OFFSET, (i*WSTR_SIZE) + H_OFFSET, wstr, WSTR_SIZE);
	}

	const int CAPTURED_SIZE = (PIECE_TYPES-1) * WSTR_SIZE;
	const int PLAYER_INFO_SIZE = 3 + sizeof(player_name_t) + 3 + 1;	// [TYPE]NAME(TURN)
	char plr1_info[PLAYER_INFO_SIZE], plr2_info[PLAYER_INFO_SIZE];
	snprintf(plr1_info, PLAYER_INFO_SIZE, "[%c]%-*s(%c)", get_player_type_char(plr1), sizeof(player_name_t), plr1.name, (board->chance == WHITE ? '*': ' '));
	snprintf(plr2_info, PLAYER_INFO_SIZE, "[%c]%-*s(%c)", get_player_type_char(plr2), sizeof(player_name_t), plr2.name, (board->chance == BLACK ? '*': ' '));
	mvwaddnstr(plr1_scr, V_OFFSET, H_OFFSET + CAPTURED_SIZE, plr1_info, PLAYER_INFO_SIZE);
	mvwaddnstr(plr2_scr, V_OFFSET, H_OFFSET + CAPTURED_SIZE, plr2_info, PLAYER_INFO_SIZE);

	// display the timer
	if (board->plr_times[0] != -1) {
		const int TIME_STR_SIZE = 5 + 1;	// "00:00" mins:secs
		char time_str[TIME_STR_SIZE];

		//plr1
		int time_rem = board->plr_times[0];
		int mins = time_rem / 60;
		int secs = time_rem % 60;
		snprintf(time_str, TIME_STR_SIZE, "%02d:%02d", mins, secs);
		mvwaddnstr(plr1_scr, V_OFFSET, H_OFFSET + CAPTURED_SIZE + PLAYER_INFO_SIZE, time_str, TIME_STR_SIZE);

		//plr2
		time_rem = board->plr_times[1];
		mins = time_rem / 60;
		secs = time_rem % 60;
		snprintf(time_str, TIME_STR_SIZE, "%02d:%02d", mins, secs);
		mvwaddnstr(plr2_scr, V_OFFSET, H_OFFSET + CAPTURED_SIZE + PLAYER_INFO_SIZE, time_str, TIME_STR_SIZE);
	}

// 	wrefresh(plr1_scr);
// 	wrefresh(plr2_scr);
	wrefresh(plr1_scr);
	wrefresh(plr2_scr);
}


static char get_player_type_char (const player_t plr) {
	switch (plr.type) {
		case HUMAN:
			return 'H';
		case LAN:
			return 'L';
		case ONLINE:
			return 'O';
		case AI_LVL0:
			return '0';
		case AI_LVL1:
			return '1';
		case AI_LVL2:
			return '2';
		default:
			return 'A';
	}
}


static void show_menu (bool is_undo_disabled) {
	werase(menu_scr);
	box(menu_scr, 0, 0);

// 	int NO_OF_OPTS = 5;
	const int OPTS_SIZE = 14;
	const int V_OFFSET = 1, H_OFFSET = 3;

	enum { ASCII_OPT, UNDO_OPT, SAVE_OPT, PGN_OPT, QUIT_OPT, NO_OF_OPTS };

	// initialize options
	char options[NO_OF_OPTS][OPTS_SIZE+1];
	char prefixes[NO_OF_OPTS];// = {'a', 'u', 's', 'e', 'q'};

	prefixes[ASCII_OPT] = 'a';
	prefixes[UNDO_OPT]  = 'u';
	prefixes[SAVE_OPT]  = 's';
	prefixes[PGN_OPT]   = 'e';
	prefixes[QUIT_OPT]  = 'q';

	if (SETTINGS_UNICODE_MODE)
		snprintf(options[0], OPTS_SIZE, ": %s", "ascii");
	else
		snprintf(options[0], OPTS_SIZE, ": %s", "unicode");
	snprintf(options[1], OPTS_SIZE, ": %s", "undo");
	snprintf(options[2], OPTS_SIZE, ": %s", "save");
	snprintf(options[3], OPTS_SIZE, ": %s", "export pgn");
	snprintf(options[4], OPTS_SIZE, ": %s", "quit");

	if (is_undo_disabled) {
		for (int i = UNDO_OPT; i < NO_OF_OPTS-1; i++) {
			prefixes[i] = prefixes[i+1];
			strncpy(options[i], options[i+1], OPTS_SIZE);
		}
	}

	int prefix_pos[NO_OF_OPTS];
	prefix_pos[0] = 0;
	for (int i = 1; i < NO_OF_OPTS; i++) {
		prefix_pos[i] = prefix_pos[i-1] + strlen(options[i-1]) + H_OFFSET;
	}

	for (int i = 0; i < NO_OF_OPTS - (is_undo_disabled? 1: 0); i++) {
		wattron(menu_scr, A_BOLD);
		wattron(menu_scr, A_STANDOUT);
		mvwaddch(menu_scr, V_OFFSET, prefix_pos[i] + H_OFFSET, prefixes[i]);
		wattroff(menu_scr, A_BOLD);
		wattroff(menu_scr, A_STANDOUT);
		mvwaddnstr(menu_scr, V_OFFSET, prefix_pos[i] + H_OFFSET + 1, options[i], OPTS_SIZE);
	}

// 	wrefresh(menu_scr);
	wrefresh(menu_scr);
}
