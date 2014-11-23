#include <string.h>
#include <stdlib.h>
#include <stdio.h>

struct node {
	int move;
	struct node *children[7] = { NULL };
};

struct player {
	int id;
	int offset;
	int kalah;
};

#define MAX_DEPTH 200
#define SOUTH 0
#define NORTH 1

// 0 to 6 are SOUTH's wells
// 7 is SOUTH's kalah
// 8 to 14 are NORTH's wells
// 15 is NORTH's kalah
int board_states[MAX_DEPTH][16];
struct node root = NULL;
char valid_filename_characters[59] = "abcdefghijklmnopqrstuvwxyz!@#$%^&*()-_=+`~,<>/?;'\"[]{}\|";

int check_game_over(int depth) {
	if (board_states[depth][7] > 49) {
		return 1; // SOUTH WINS
	} else if (board_states[depth][15] > 49) {
		return -1; // NORTH WINS
	} else if (board_states[depth][7] == 49 && board_states[depth][15] == 49) {
		return 0; // TIE GAME
	} else {
		return -99; // GAME IS NOT OVER
	}
}

int make_move(int depth, int move, struct player *player, struct player *opposite_player) {
	int beads = board_states[depth][move];

	// Copy the board state from depth to depth + 1 so it can be modified
	for (int i = 0; i < 16; i++) {
		board_states[depth + 1][i] = board_states[depth][i];
	}

	board_states[depth + 1][move] = 0;

	int offset = 0; // Used to skip the opposite player's kalah
	for (int i = 1; i <= beads; i++) {
		if ((move + i + offset) % 16 == opposite_player->kalah) {
			offset++; // Skip other player's kalah
		}
		board_states[depth + 1][(move + i + offset) % 16]++; // Sow
	}

	int end_point = (move + beads + offset) % 16;
	// Capture the other player's beads from the opposite well
	if (end_point >= player * 8 && end_point < player->kalah
			&& board_states[depth + 1][end_point] == 1) {
		board_states[depth + 1][end_point] = 0;
		board_states[depth + 1][player->kalah] += board_states[depth + 1][14
				- end_point] + 1;
		board_states[depth + 1][14 - end_point] = 0;
	}

	return end_point;
}

// Calculate the best move and eventual outcome for the
// board state in board_states[depth]
int trace(int depth, struct player *player, struct player *opposite_player) {
	// Check if the game is won/lost/tied in this board state
	int game_status = check_game_over(depth);
	if (game_status != -99) {
		return game_status;
	}

	int player_offset, best_result, subtree_best_result;
	int best_move = (player->id == SOUTH) ? -2 : 2;

	// Calculate the end result of performing each possible move on the board state
	for (int move = player_offset; move < player->kalah; move++) {
		if (board_states[depth][move] == 0) {
			// Skip empty wells
			continue;
		}

		if (make_move(depth, player, opposite_player) == player->kalah) {
			subtree_best_result = trace(depth + 1, player, opposite_player); // Free turn
		} else {
			subtree_best_result = trace(depth + 1, opposite_player, player);
		}

		if (player->id == SOUTH) {
			if (subtree_best_result > best_result) { // SOUTH wants to maximise best_result
				best_move = move;
				best_result = subtree_best_result;
				if (best_result == 1) {
					break; // Cut when SOUTH can force a win from this board state
				}
			}
		} else if (subtree_best_result < best_result) { // NORTH wants to minimise best_result
			best_result = subtree_best_result;
			if (best_result == -1) {
				break; // Cut when NORTH can force a loss from this board state
			}
		}
	}

	// Check if the player was able to perform any moves with this board state
	if (best_move == -1 && (best_result == 2 || best_result == -2)) {
		for (int move = opposite_player->offset; move < opposite_player->kalah;
				move++) {
			board_states[depth][opposite_player->kalah] +=
					board_states[depth][move];
			board_states[depth][move] = 0;
		}
		best_result = check_game_over(depth); // Guaranteed to not return -99
	}

	return best_result;
}

int main(int argc, char *argv[]) {
	// Initialise the starting board
	for (int i = 0; i < 16; i++) {
		board_states[0][i] = (i == 7 || i == 15) ? 0 : 7;
	}

	// Set up players
	struct player *south = (struct player*) malloc(sizeof(struct player));
	south->id = SOUTH;
	south->offset = 0;
	south->kalah = 7;
	struct player *north = (struct player*) malloc(sizeof(struct player));
	north->id = NORTH;
	north->offset = 8;
	north->kalah = 15;

	// Make the first move
	if (argc == 2) {
		make_move(strtol(argv[1], NULL, 10), south, north);
	} else {
		return 1;
	}

	// Should hopefully output 1 (meaning we are guaranteed to win as SOUTH)
	printf("Best guaranteed result for SOUTH: %d\n", trace(1, north, south));

	write_to_file();
	getch(); // Wait for user input to end (needed to see final output on Windows)

	return 0;
}
