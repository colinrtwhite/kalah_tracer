#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "uthash.h"

struct hash_move {
	char state[41]; // Key (string is WITHIN the structure)
	int move;
	int result;
	UT_hash_handle hh; // Makes this structure hashable
};

#define PID 2
#define NUM_PROCESSES 3 // Max of 7
#define MAX_DEPTH 200 // Game should come nowhere near this number of turns
#define MAX_NORTH_BEADS 20
#define SOUTH 0
#define NORTH 1

// 0 to 6 are SOUTH's wells
// 7 is SOUTH's kalah
// 8 to 14 are NORTH's wells
// 15 is NORTH's kalah
int board_states[MAX_DEPTH][16];
struct hash_move *best_moves = NULL;
char board[41];
int minimum_move_offset, maximum_move_offset;

void convert_board_to_string(int depth, char *destination) {
	for (int i = 0; i < 15; i++) {
		destination += sprintf(destination, "%d-", board_states[depth][i]);
	}
	destination += sprintf(destination, "%d", board_states[depth][15]);
}

int check_game_over(int depth) {
	if (board_states[depth][7] > 49) {
		return 1; // SOUTH WINS
	} else if (board_states[depth][15] > MAX_NORTH_BEADS) {
		return -1; // NORTH WINS
	} else if (board_states[depth][7] == 49 && board_states[depth][15] == 49) {
		return 0; // TIE GAME
	} else {
		return -99; // GAME IS NOT OVER
	}
}

// Calculate the best move and eventual outcome for the
// board state in board_states[depth]
double trace(int depth, int player) {
	// Check if the game is won/lost/tied in this board state
	int game_status = check_game_over(depth);
	if (game_status != -99) {
		return game_status;
	}

	// Check if we've already encountered this board state
	struct hash_move *entry;
	convert_board_to_string(depth, board);
	HASH_FIND_STR(best_moves, board, entry);
	if (entry) {
		// This board state is identical to a previous point in the game tree,
		// which has already been explored
		return entry->result;
	}

	int player_offset = player * 8;
	int player_kalah = player_offset + 7;
	int opposite_player = (player + 1) % 2;
	int opposite_player_offset = opposite_player * 8;
	int opposite_player_kalah = opposite_player_offset + 7;
	int best_move = -1, best_result = -2, subtree_best_result, min_move = 0, max_move = 0;
	if (player == NORTH) {
		best_result = 2;
	}

	// Separate the top level game subtrees
	if (depth == 0) {
		min_move = minimum_move_offset;
		max_move = maximum_move_offset;
	}

	for (int move = player_offset + min_move; move < player_kalah - max_move; move++) {
		int beads = board_states[depth][move];
		if (beads == 0) { // Skip empty wells
			continue;
		}

		// Copy the board state from depth to depth + 1 so it can be modified
		for (int i = 0; i < 16; i++) {
			board_states[depth + 1][i] = board_states[depth][i];
		}

		board_states[depth + 1][move] = 0;

		int offset = 0; // Used to skip the opposite player's kalah
		for (int i = 1; i <= beads; i++) {
			// Skip other player's kalah
			if ((move + i + offset) % 16 == opposite_player_kalah) {
				offset++;
			}

			board_states[depth + 1][(move + i + offset) % 16]++; // Sow
		}

		int end_point = (move + beads + offset) % 16;
		// Capture the other player's beads from the opposite well
		if (end_point >= player * 8 && end_point < player_kalah
				&& board_states[depth + 1][end_point] == 1) {
			board_states[depth + 1][end_point] = 0;
			board_states[depth + 1][player_kalah] += board_states[depth + 1][14
					- end_point] + 1;
			board_states[depth + 1][14 - end_point] = 0;
		}

		if (end_point == player_kalah) { // Free turn
			subtree_best_result = trace(depth + 1, player); // Should never be -2 or 2
		} else {
			subtree_best_result = trace(depth + 1, opposite_player); // Should never be -2 or 2
		}

		if (player == SOUTH) {
			if (subtree_best_result > best_result) { // SOUTH wants to maximise best_result
				best_move = move;
				best_result = subtree_best_result;
			}
		} else if (subtree_best_result < best_result) { // NORTH wants to minimise best_result
			best_result = subtree_best_result;
		}
	}

	// Check if the player was able to perform any moves with this board state
	if (best_move == -1 && (best_result == 2 || best_result == -2)) {
		for (int move = opposite_player_offset; move < opposite_player_kalah;
				move++) {
			board_states[depth][opposite_player_kalah] +=
					board_states[depth][move];
			board_states[depth][move] = 0;
		}
		best_result = check_game_over(depth); // Should not return -99 at this point
	} else if (player == SOUTH) {
		// Hash the best move and win percentage with respect to board layout
		struct hash_move *best_move_hash = (struct hash_move*) malloc(
				sizeof(struct hash_move));
		convert_board_to_string(depth, best_move_hash->state);
		best_move_hash->move = best_move;
		best_move_hash->result = best_result;
		HASH_ADD_STR(best_moves, state, best_move_hash);
	}

	return best_result;
}

void write_to_file() {
	struct hash_move *i;
	sprintf(board, "board_states_%d_of_%d.txt", PID, NUM_PROCESSES);
    FILE *fp = fopen(board, "w+");

    for(i = best_moves; i != NULL; i = i->hh.next) {
		fprintf(fp, "%s#%d#%d\n", i->state, i->move, i->result);
    }

	fclose(fp);
}

int main() {
	// Initialise the starting board
	for (int i = 0; i < 16; i++) {
		if (i == 7 || i == 15) {
			board_states[0][i] = 0;
		} else {
			board_states[0][i] = 7;
		}
	}

	// Calculate the minimum and maximum move offsets
	int pid = PID - 1;
	minimum_move_offset = pid * ((int)(7 / NUM_PROCESSES));
	maximum_move_offset = PID * ((int)(7 / NUM_PROCESSES));
	for (int i = 0; i < 7 % NUM_PROCESSES && i < pid; i++) {
		minimum_move_offset++;
		maximum_move_offset++;
	}
	maximum_move_offset = 7 - maximum_move_offset - 1;

	// Should hopefully output 1 (meaning we are guaranteed to win as SOUTH)
	printf("Best guaranteed result for SOUTH: %f\n", trace(0, SOUTH));

	printf("Beginning write to file process.");
	write_to_file();

	return 0;
}
