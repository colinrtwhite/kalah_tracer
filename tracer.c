#include <string.h>
#include <stdlib.h>
#include <stdio.h>

struct node {
	int player;
	struct node *children[7];
};

struct side {
	int id;
	int offset;
	int kalah;
};

struct return_struct {
	int max_min;
	int height_from_leaf;
	int is_reduced;
};

#define MAX_MOVES 6
#define PREDICTION_DEPTH 0
#define MAX_DEPTH 200
#define SOUTH 0
#define NORTH 1
#define NOT_REDUCED 0
#define POSSIBLY_FULLY_REDUCED 1
#define FULLY_REDUCED 2

// 0 to 6 are SOUTH's wells
// 7 is SOUTH's kalah
// 8 to 14 are NORTH's wells
// 15 is NORTH's kalah
int board_states[MAX_DEPTH][16];
char *depth_tokens = "0123456789abcdefghijklmnopqrstuvwxyz";
struct side *us, *them;

// Check if the board state at depth is complete
int check_game_over(int depth, int num_moves) {
	if (board_states[depth][us->kalah] > 49) {
		return 99; // WE WIN
	} else if (board_states[depth][them->kalah] > 49) {
		return -99; // THEY WIN
	} else if (num_moves == MAX_MOVES) {
		return board_states[depth][us->kalah] - board_states[depth][them->kalah]; // END OF TREE EXPLORATION
	} else {
		return 100; // GAME IS NOT OVER
	}
}

// Apply move on the board at depth and store the result at depth + 1.
// Returns the endpoint of the move.
int make_move(int depth, int move, struct side *player,
		struct side *opposite_player) {
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

// Recursively free the tree with root self
void free_subtree(struct node *self) {
	for (int i = 0; i < 7; i++) {
		if (self->children[i] != NULL) {
			free_subtree(self->children[i]);
			self->children[i] = NULL;
		}
	}
	free(self);
}

// Set all of node self's children to NULL
void clear_children(struct node *self) {
	for (int i = 0; i < 7; i++) {
		self->children[i] = NULL;
	}
}

// Recursively free the subtrees of all of self's children
void free_child_subtrees(struct node* self, int best_move) {
	// Free every child tree except our best move
	for (int i = 0; i < 7; i++) {
		if (self->children[i] != NULL && i != best_move) {
			free_subtree(self->children[i]);
			self->children[i] = NULL;
		}
	}
}

// Calculate the best move and eventual outcome for the
// board state in board_states[depth]
struct return_struct* trace(int depth, int num_moves, struct node *self,
		struct side *player, struct side *opposite_player) {
	// Check if the game is won/lost/tied in this board state
	int game_status = check_game_over(depth, num_moves);
	if (game_status != 100) {
		struct return_struct *ret = (struct return_struct*) malloc(
				sizeof(struct return_struct));
		ret->height_from_leaf = 1;
		ret->is_reduced = NOT_REDUCED;
		ret->max_min = game_status;
		return ret;
	}

	struct return_struct *return_value = (struct return_struct*) malloc(
			sizeof(struct return_struct)), *subtree_result;
	return_value->height_from_leaf = 0;
	return_value->is_reduced = NOT_REDUCED;
	return_value->max_min = (player->id == us->id) ? -100 : 100;

	// Calculate the end result of performing each possible move on the board state
	int best_move = -1;
	for (int move = player->offset; move < player->kalah; move++) {
		if (board_states[depth][move] == 0) {
			continue; // Skip empty wells
		}

		struct node *child = (struct node*) malloc(sizeof(struct node));
		clear_children(child);
		self->children[move % 8] = child;

		if (make_move(depth, move, player, opposite_player) == player->kalah) {
			child->player = player->id;
			subtree_result = trace(depth + 1, num_moves, child, player,
					opposite_player); // Free turn
		} else {
			child->player = opposite_player->id;
			subtree_result = trace(depth + 1,
					num_moves + (player->id == us->id), child, opposite_player,
					player);
		}

		if (subtree_result->height_from_leaf >= return_value->height_from_leaf) {
			return_value->height_from_leaf = subtree_result->height_from_leaf
					+ 1;
		}
		if (subtree_result->is_reduced > return_value->is_reduced) {
			return_value->is_reduced = subtree_result->is_reduced;
		}
		if (player->id == us->id) {
			if (subtree_result->max_min >= return_value->max_min) { // WE want to maximise max_min_result
				best_move = move % 8;
				return_value->max_min = subtree_result->max_min;
			}
		} else if (subtree_result->max_min < return_value->max_min) { // THEY want to minimise max_min_result
			return_value->max_min = subtree_result->max_min;
		}

		free(subtree_result);
	}

	if (best_move == -1 && abs(return_value->max_min) == 100) {
		// Player was able to perform any moves with this board state
		for (int move = opposite_player->offset; move < opposite_player->kalah;
				move++) {
			board_states[depth][opposite_player->kalah] +=
					board_states[depth][move];
			board_states[depth][move] = 0;
		}
		return_value->max_min = check_game_over(depth, num_moves); // Will not return 100
	} else if (return_value->height_from_leaf > PREDICTION_DEPTH
			&& return_value->is_reduced < FULLY_REDUCED) {
		// Don't store what we'll be able to predict better when at that game tree node
		return_value->is_reduced =
				(player->id == us->id) ? FULLY_REDUCED : POSSIBLY_FULLY_REDUCED;
		free_child_subtrees(self, -1);
		if (player->id == us->id) {
			self->children[best_move] = (struct node*) malloc(
					sizeof(struct node));
			clear_children(self->children[best_move]);
		}
	} else if (player->id == us->id) {
		// Free every child tree except our best move
		free_child_subtrees(self, best_move);
	}

	return return_value;
}

// Helper function for write_to_file
void __write_to_file(struct node *self, int depth, FILE *fp) {
	int new_depth = (self->player == them->id) ? depth + 1 : depth;

	for (int i = 0; i < 7; i++) {
		if (self->children[i] != NULL) {
			if (self->player == us->id) {
				fprintf(fp, "%d", i); // Best move for this turn
			} else {
				fprintf(fp, "\n%c%d", depth_tokens[depth], i);
			}
			__write_to_file(self->children[i], new_depth, fp);
		}
	}

	free(self);
}

// Write the resulting game tree in a compressed format to a .txt file
void write_to_file(struct node *game_tree, char *side_str, int first_move,
		int max_min) {
	char file_name[13];
	sprintf(file_name, "%s%d.txt", side_str, first_move);
	FILE *fp = fopen(file_name, "w+");

	fprintf(fp, "%d", max_min);
	if (us->id == NORTH) {
		fprintf(fp, "\n");
	}
	__write_to_file(game_tree, 0, fp);

	fclose(fp);
}

int main(int argc, char *argv[]) {
	if (argc != 3) {
		fprintf(stderr,
				"Need two command line arguments representing which \
				side we are playing as and the first move of the game.\n");
		return 1;
	}

	// Initialise the starting board
	for (int i = 0; i < 16; i++) {
		board_states[0][i] = (i == 7 || i == 15) ? 0 : 7;
	}

	// Set up players
	struct side south = { SOUTH, 0, 7 }, north = { NORTH, 8, 15 };
	us = (strcmp(argv[1], "S") == 0) ? &south : &north;
	them = (strcmp(argv[1], "S") == 0) ? &north : &south;

	// Make the first move (SOUTH's move)
	const int first_move = (int) strtol(argv[2], NULL, 10);
	// Don't need to worry about capturing or free move on first turn
	make_move(0, first_move, &south, &north);
	struct node *root = (struct node*) malloc(sizeof(struct node));
	clear_children(root);
	root->player = NORTH;

	// Trace the game tree then write it to a file
	write_to_file(root, argv[1], first_move,
			trace(1, 1, root, &north, &south)->max_min);

	return 0;
}
