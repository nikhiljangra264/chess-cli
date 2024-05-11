#include <string.h>
#include <stdlib.h>

#include "board.h"

const wchar_t PIECES[2][2][6] = {
	{
		{'K', 'Q', 'R', 'B', 'N', 'P'},
		{'k', 'q', 'r', 'b', 'n', 'p'}
	},
	{
		{L'\u2654', L'\u2655', L'\u2656', L'\u2657', L'\u2658', L'\u2659'},
		{L'\u265A', L'\u265B', L'\u265C', L'\u265D', L'\u265E', L'\u265F'}
	}
};

bool	SETTINGS_UNICODE_MODE	=	true;


static	piece_t*	init_piece	(short i, short j);


wchar_t get_piece_face(const piece_t *piece) {
	if (piece == NULL)
		return L' ';
	int type = 0;
	while (!(piece->face & (1 << type))) type++;
	// use SETTINGS_UNICODE_MODE
	return PIECES[SETTINGS_UNICODE_MODE][(bool) (piece->face & BLACK)][type];
}


char get_piece_for_move_notation (const piece_t *piece) {
	if (piece == NULL)
		return '\0';
	int type = 0;
	while (!(piece->face & (1 << type))) type++;
	return PIECES[ASCII][is_black(piece->face)][type];
}


void init_board(board_t *board, int time_limit) {
	memset(board, 0, sizeof(*board));
	for (short i=0; i<8; i++) {
		for (short j=0; j<8; j++) {
			board->tiles[i][j].row = i;
			board->tiles[i][j].col = j;
			board->tiles[i][j].piece = init_piece(i, j);
		}
	}

	board->kings[0] = &(board->tiles[0][4]);
	board->kings[1] = &(board->tiles[7][4]);

	board->chance = WHITE;
	board->result = PENDING;
	board->is_fake = false;
	board->plr_times[0] = board->plr_times[1] = time_limit;
}


void copy_board (board_t *dest_board, const board_t *src_board) {
	if (dest_board == NULL || src_board == NULL)
		return;

	for (short i = 0; i < 8; i++)
		for (short j = 0; j < 8; j++)
			if (dest_board->tiles[i][j].piece != NULL)
				free(dest_board->tiles[i][j].piece);

	memset(dest_board, 0, sizeof(board_t));
	memcpy(dest_board, src_board, sizeof(board_t));
	dest_board->is_fake = src_board->is_fake;
	for (short i = 0; i < 8; i++) {
		for (short j = 0; j < 8; j++) {
			if (dest_board->tiles[i][j].piece == NULL)
				continue;
			dest_board->tiles[i][j].piece = (piece_t*) malloc(sizeof(piece_t));
			memcpy(dest_board->tiles[i][j].piece, src_board->tiles[i][j].piece, sizeof(piece_t));
		}
	}

	// set kings
	for (int i = 0; i < 2; i++) {
		const tile_t *old_king = src_board->kings[i];
		dest_board->kings[i] = &(dest_board->tiles[old_king->row][old_king->col]);
	}
}


void delete_board (board_t *board) {
	if (board == NULL)
		return;

	for (short i = 0; i < 8; i++)
		for (short j = 0; j < 8; j++)
			if (board->tiles[i][j].piece)
				free(board->tiles[i][j].piece);
	
	free(board);
}


void promote_pawn (piece_t *piece, const face_t piece_type) {
	if (piece == NULL || !(piece->face & PAWN))
		return;
	piece->face ^= PAWN;
	piece->face |= piece_type;
}


static piece_t* init_piece(short i, short j) {
	// no piece
	if (i > 1 && i < 6)
		return NULL;

	piece_t *piece = (piece_t*) malloc(sizeof(piece_t));
	memset(piece, 0, sizeof(piece_t));

	if (i >= 6) piece->face |= BLACK;
	if (i == 1 || i == 6) {
		piece->face |= PAWN;
		return piece;
	}

	switch (j) {
		case 0:
		case 7:
			piece->face |= ROOK;
			break;
		case 1:
		case 6:
			piece->face |= KNIGHT;
			break;
		case 2:
		case 5:
			piece->face |= BISHOP;
			break;
		case 3:
			piece->face |= QUEEN;
			break;
		case 4:
			piece->face |= KING;
	}

	return piece;
}
