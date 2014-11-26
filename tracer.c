#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

struct node {
	int player;
	struct node *children[7];
};

struct player {
	int id;
	int offset;
	int kalah;
};

#define MAX_SOUTH_MOVES 7
#define MAX_RECORDED_SOUTH_MOVES (MAX_SOUTH_MOVES - 2)
#define MAX_DEPTH 25
#define SOUTH 0
#define NORTH 1

// 0 to 6 are SOUTH's wells
// 7 is SOUTH's kalah
// 8 to 14 are NORTH's wells
// 15 is NORTH's kalah
int board_states[MAX_DEPTH][16];

int check_game_over(int depth, int num_south_moves) {
	if (board_states[depth][7] > 49) {
		return 99; // SOUTH WINS
	} else if (board_states[depth][15] > 49) {
		return -99; // NORTH WINS
	} else if (num_south_moves == MAX_SOUTH_MOVES) {
		return board_states[depth][7] - board_states[depth][15]; // END OF TREE EXPLORATION
	} else {
		return 100; // GAME IS NOT OVER
	}
}

int make_move(int depth, int move, struct player *player,
		struct player *opposite_player) {
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
	if (end_point >= player->offset * 8 && end_point < player->kalah
			&& board_states[depth + 1][end_point] == 1) {
		board_states[depth + 1][end_point] = 0;
		board_states[depth + 1][player->kalah] += board_states[depth + 1][14
				- end_point] + 1;
		board_states[depth + 1][14 - end_point] = 0;
	}

	return end_point;
}

// Set all of the node's children to NULL.
void clear_children(struct node *self) {
	for (int i = 0; i < 7; i++) {
		self->children[i] = NULL;
	}
}

// Recursively free the given tree.
void free_subtree(struct node *self) {
	for (int i = 0; i < 7; i++) {
		if (self->children[i] != NULL) {
			free_subtree(self->children[i]);
		}
	}
	clear_children(self);
	free(self);
}

// Calculate the best move and eventual outcome for the
// board state in board_states[depth]
int trace(int depth, int num_south_moves, struct node *self,
		struct player *player, struct player *opposite_player) {
	// Check if the game is won/lost/tied in this board state
	int game_status = check_game_over(depth, num_south_moves);
	if (game_status != 100) {
		return game_status;
	}

	int subtree_result, best_move = -1;
	int max_min_result = (player->id == SOUTH) ? -100 : 100;
	int new_num_south_moves =
			(player->id == SOUTH) ? num_south_moves + 1 : num_south_moves;

	// Calculate the end result of performing each possible move on the board state
	for (int move = player->offset; move < player->kalah; move++) {
		if (board_states[depth][move] == 0) {
			continue; // Skip empty wells
		}

		struct node *child = (struct node*) malloc(sizeof(struct node));
		clear_children(child);
		self->children[move % 8] = child;

		if (make_move(depth, move, player, opposite_player) == player->kalah) {
			child->player = player->id;
			subtree_result = trace(depth + 1, num_south_moves, child, player,
					opposite_player); // Free turn
		} else {
			child->player = opposite_player->id;
			subtree_result = trace(depth + 1, new_num_south_moves, child,
					opposite_player, player);
		}

		if (player->id == SOUTH) {
			if (subtree_result > max_min_result) { // SOUTH wants to maximise best_result
				best_move = move;
				max_min_result = subtree_result;
			}
		} else if (subtree_result < max_min_result) { // NORTH wants to minimise best_result
			max_min_result = subtree_result;
		}
	}

	// Check if the player was able to perform any moves with this board state
	if (best_move == -1 && (max_min_result == 100 || max_min_result == -100)) {
		for (int move = opposite_player->offset; move < opposite_player->kalah;
				move++) {
			board_states[depth][opposite_player->kalah] +=
					board_states[depth][move];
			board_states[depth][move] = 0;
		}
		max_min_result = check_game_over(depth, num_south_moves); // Will not return 100
	} else if (player->id == SOUTH
			|| num_south_moves == MAX_RECORDED_SOUTH_MOVES) { // Free not useful/necessary trees
		best_move =
				(num_south_moves == MAX_RECORDED_SOUTH_MOVES) ? -1 : best_move;
		for (int i = 0; i < 7; i++) {
			if (self->children[i] != NULL && i != best_move) {
				free_subtree(self->children[i]);
				self->children[i] = NULL;
			}
		}
	}

	return max_min_result;
}

void __write_to_file(struct node *self, int depth, FILE *fp) {
	int new_depth = (self->player == NORTH) ? depth + 1 : depth;

	for (int i = 0; i < 7; i++) {
		if (self->children[i] != NULL) {
			if (self->player == SOUTH) {
				fprintf(fp, "%d", i); // Best move for this turn
			} else {
				fprintf(fp, "\n%x%d", depth, i);
			}
			__write_to_file(self->children[i], new_depth, fp);
		}
	}

	free(self);
}

// Write the resulting tree in a compressed format to a .txt file
void write_to_file(struct node *game_tree, int first_move, int max_min) {
	char file_name[7];
	sprintf(file_name, "N%d.txt", first_move);
	FILE *fp = fopen(file_name, "w+");

	fprintf(fp, "%d", max_min);
	__write_to_file(game_tree, 1, fp);

	fclose(fp);
}

int main(int argc, char *argv[]) {
	if (argc != 2) {
		fprintf(stderr,
				"Need one command line argument (representing first North move).\n");
		return 1;
	}

	time_t start = time(NULL);

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
	int first_move = (int) strtol(argv[1], NULL, 10);
	make_move(0, first_move, south, north); // Don't need to worry about capturing or free move
	struct node *root = (struct node*) malloc(sizeof(struct node));
	clear_children(root);
	root->player = NORTH;

	// Trace the game tree then write it to a file
	write_to_file(root, first_move, trace(1, 1, root, north, south));

	fprintf(stderr, "Run time: %.2f minute(s)\n", difftime(time(NULL), start) / 60);

	return 0;
}
