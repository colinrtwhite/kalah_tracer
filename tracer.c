#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "uthash.h"

struct hash_item {
	char state[41]; // Key (string is WITHIN the structure)
	int move;
	int result;
	int depth;
	UT_hash_handle hh; // Makes this structure hashable
};

#define MAX_DEPTH 40
#define MAX_HASHES 15000000
#define SOUTH 0
#define NORTH 1

// 0 to 6 are SOUTH's wells
// 7 is SOUTH's kalah
// 8 to 14 are NORTH's wells
// 15 is NORTH's kalah
int board_states[MAX_DEPTH][16];
struct hash_item *game_tree = NULL;
char board[41];
int first_move, num_files;

void convert_board_to_string(int depth, char *destination) {
	for (int i = 0; i < 15; i++) {
		destination += sprintf(destination, "%d|", board_states[depth][i]);
	}
	destination += sprintf(destination, "%d", board_states[depth][15]);
}

int sort_hashes(struct hash_item *a, struct hash_item *b) {
	if (a->depth < b->depth) {
		return -1;
	} else if (a->depth == b->depth) {
		return 0;
	} else {
		return 1;
	}
}

// Sort and write all the collected board states to a file
// and clear the hash table
void write_to_file() {
	fprintf(stderr, "Beginning sort and write to file process %d.\n", num_files);
	// Sort the hashes
	HASH_SORT(game_tree, sort_hashes);

	sprintf(board, "board_states_for_%d_num_%d.txt", first_move, num_files);
	FILE *fp = fopen(board, "w+");
	struct hash_item *i, *tmp;

	HASH_ITER(hh, game_tree, i, tmp)
	{
		fprintf(fp, "%d~%s~%d~%d\n", i->depth, i->state, i->move, i->result);
		HASH_DEL(game_tree, i);
		free(i);
	}

	fclose(fp);
	num_files++;
}

int check_game_over(int depth) {
	if (board_states[depth][7] > 49) {
		return 1; // SOUTH WINS
	} else if (board_states[depth][15] > 49 || depth == MAX_DEPTH) {
		return -1; // NORTH WINS
	} else if (board_states[depth][7] == 49 && board_states[depth][15] == 49) {
		return 0; // TIE GAME
	} else {
		return -99; // GAME IS NOT OVER
	}
}

// Calculate the best move and eventual outcome for the
// board state in board_states[depth]
int trace(int depth, int player) {
	// Check if the game is won/lost/tied in this board state
	int game_status = check_game_over(depth);
	if (game_status != -99) {
		return game_status;
	}

	// Check if we've already encountered this board state
	struct hash_item *entry;
	convert_board_to_string(depth, board);
	HASH_FIND_STR(game_tree, board, entry);
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
	int best_result = (player == SOUTH) ? -2 : 2;
	int best_move = -1, subtree_best_result, beads;

	// Calculate the end result of performing each possible move on the board state
	for (int move = player_offset; move < player_kalah; move++) {
		beads = board_states[depth][move];
		if (beads == 0 || (depth == 0 && move != first_move)) {
			// Skip empty wells and only perform first_move at depth = 0
			continue;
		}

		// Copy the board state from depth to depth + 1 so it can be modified
		for (int i = 0; i < 16; i++) {
			board_states[depth + 1][i] = board_states[depth][i];
		}

		board_states[depth + 1][move] = 0;

		int offset = 0; // Used to skip the opposite player's kalah
		for (int i = 1; i <= beads; i++) { // Sow
			if ((move + i + offset) % 16 == opposite_player_kalah) {
				offset++; // Skip other player's kalah
			}
			board_states[depth + 1][(move + i + offset) % 16]++;
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

		if (end_point == player_kalah && depth != 0) { // Free turn (not on first turn)
			subtree_best_result = trace(depth + 1, player);
		} else {
			subtree_best_result = trace(depth + 1, opposite_player);
		}

		if (player == SOUTH) {
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
		for (int move = opposite_player_offset; move < opposite_player_kalah;
				move++) {
			board_states[depth][opposite_player_kalah] +=
					board_states[depth][move];
			board_states[depth][move] = 0;
		}
		best_result = check_game_over(depth); // Should not return -99 at this point
	} else if (player == SOUTH && best_result > -1) { // Don't hash losing board states to save space
		struct hash_item *best_move_hash = (struct hash_item*) malloc(
				sizeof(struct hash_item));
		convert_board_to_string(depth, best_move_hash->state);
		best_move_hash->move = best_move;
		best_move_hash->result = best_result;
		best_move_hash->depth = depth; // Hash depth to help sort board states
		// Hash the best move and win percentage with respect to board layout
		HASH_ADD_STR(game_tree, state, best_move_hash);
		if (HASH_COUNT(game_tree) == MAX_HASHES) { // Clear the memory
			write_to_file();
		}
	}

	return best_result;
}

int main() {
	// Initialise the starting board
	for (int i = 0; i < 16; i++) {
		board_states[0][i] = (i == 7 || i == 15) ? 0 : 7;
	}

	printf("Enter the first move choice (0 - 6) to divide up the game tree: ");
	scanf("%d", &first_move);

	// Should hopefully output 1 (meaning we are guaranteed to win as SOUTH)
	printf("Best guaranteed result for SOUTH: %d\n", trace(0, SOUTH));

	write_to_file();

	return 0;
}
