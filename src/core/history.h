#ifndef HISTORY_H
#define HISTORY_H

#include "board.h"

#define MAX_MOVE_NOTATION_SIZE	10
#define TIMESTAMP_SIZE			14
#define	PLAYERNAME_SIZE			10
#define	TIME_LIMIT_STR_SIZE		4


typedef	char				timestamp_t[TIMESTAMP_SIZE+1];
typedef	char				player_name_t[PLAYERNAME_SIZE+1];
typedef	struct history_t	history_t;

enum	player_type	{HUMAN, LAN, ONLINE, AI_LVL0, AI_LVL1, AI_LVL2, AI_LVL3, PLAYER_TYPE_ERROR};

typedef struct player_t {
	player_name_t name;
	enum player_type type;
} player_t;


history_t*			create_history		(const player_t plr1, const player_t plr2, int time_limit);
void				add_move			(history_t *history, const board_t *board, const char *const move_notation);
void				undo				(history_t *history);
void				go_back				(history_t *history, int n);
void				delete_history		(history_t *history);
const board_t*		peek_board			(const history_t *history, int n);
int					get_size			(const history_t *history);
const char *const	peek_move			(const history_t *history, int n);
const char *const	get_timestamp		(const history_t *history);
void				set_timestamp		(history_t *history, const timestamp_t timestamp);
history_t*			reverse_history		(const history_t *history);
void				update_result		(history_t *history);
enum result			get_result			(const history_t *history);
void				get_players			(const history_t *history, player_t *plr1, player_t *plr2);
void				init_player			(player_t *plr, const char *const name, const enum player_type type);
int					get_time_limit		(const history_t *history);
bool				is_fake_history		(const history_t *history);
void				set_history_fake	(history_t *history);
void				unset_history_fake	(history_t *history);


#endif
