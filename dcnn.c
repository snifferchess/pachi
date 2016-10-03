#define DEBUG
#include <assert.h>
#include <unistd.h>

#include "debug.h"
#include "board.h"
#include "engine.h"
#include "uct/tree.h"
#include "caffe.h"
#include "dcnn.h"
	
	
bool
using_dcnn(struct board *b)
{
	return (real_board_size(b) == 19 && caffe_ready());
}

void
dcnn_init()
{
	caffe_init();
}

/* Make caffe quiet */
void
dcnn_quiet_caffe(int argc, char *argv[])
{
	if (DEBUGL(7) || getenv("GLOG_minloglevel"))
		return;
	
	setenv("GLOG_minloglevel", "2", 1);
	execvp(argv[0], argv);   /* Sucks that we have to do this */
}

#define min(a, b) ((a) < (b) ? (a) : (b))

static void
distance_transform(float *arr)
{
	// First dimension.                                                                                           
	for (int j = 0; j < 19; j++) {
		for (int i = 1; i < 19; i++) 
			arr[i*19 + j] = min(arr[i*19 + j], arr[(i-1)*19 + j] + 1);
		for (int i = 19 - 2; i >= 0; i--) 
			arr[i*19 + j] = min(arr[i*19 + j], arr[(i+1)*19 + j] + 1);
	}
	// Second dimension                                                                                           
	for (int i = 0; i < 19; i++) {
		for (int j = 1; j < 19; j++)
			arr[i*19 + j] = min(arr[i*19 + j], arr[i*19 + (j-1)] + 1);
		for (int j = 19 - 2; j >= 0; j--) 
			arr[i*19 + j] = min(arr[i*19+j], arr[i * 19 + (j+1)] + 1);
	}
}

static void
get_distance_map(struct board *b, enum stone color, float *data) 
{
	for (int i = 0; i < 19; i++) {
		for (int j = 0; j < 19; j++) {
			coord_t c = coord_xy(b, i+1, j+1);
			if (board_at(b, c) == color)
				data[i*19 + j] = 0;
			else
				data[i*19 + j] = 10000;
		}
	}
	distance_transform(data);
}

static float
board_history_decay(struct board *b, coord_t coord)
{
	return exp(0.1 * (b->moveno[coord] - b->moves));
}

void
dcnn_get_moves(struct board *b, enum stone color, float result[])
{
	assert(real_board_size(b) == 19);

	enum stone other_color = stone_other(color);
	int size = 19;
	int dsize = 25 * size * size;
	float *data = malloc(sizeof(float) * dsize);
	for (int i = 0; i < dsize; i++) 
		data[i] = 0.0;

	
	float our_dist[19*19];
	float opponent_dist[19*19];
	get_distance_map(b, color, our_dist);
	get_distance_map(b, other_color, opponent_dist);
	
	for (int j = 0; j < size; j++) {
		for(int k = 0; k < size; k++) {
			int p = size * j + k;
			coord_t c = coord_xy(b, j+1, k+1);
			group_t g = group_at(b, c);
			enum stone bc = board_at(b, c);
			int libs = board_group_info(b, g).libs;
			if (libs > 3) libs = 3;
			
			/* plane 0: our stones with 1 liberty */
			/* plane 1: our stones with 2 liberties */
			/* plane 2: our stones with 3+ liberties */
			if (bc == color)
				data[(libs-1)*size*size + p] = 1.0;

			/* planes 3, 4, 5: opponent liberties */
			if (bc == other_color)
				data[(3+libs-1)*size*size + p] = 1.0;

			/* plane 6: our simple ko.
			 * but actually, our stones. typo ? */
			if (bc == color)
				data[6*size*size + p] = 1.0;

			/* plane 7: our stones. */
			if (bc == color)
				data[7*size*size + p] = 1.0;

			/* plane 8: opponent stones. */
			if (bc == other_color)
				data[8*size*size + p] = 1.0;

			/* plane 9: empty spots. */
			if (bc == S_NONE)
				data[9*size*size + p] = 1.0;

			/* plane 10: our history */
			/* FIXME -1 for komi */
			if (bc == color)
				data[10*size*size + p] = board_history_decay(b, c);
			
			/* plane 11: opponent history */
			/* FIXME -1 for komi */
			if (bc == other_color)
				data[11*size*size + p] = board_history_decay(b, c);

			/* plane 12: border */
			if (!j || !k || j == size-1 || k == size-1)
				data[12*size*size + p] = 1.0;
			
			/* plane 13: position mask - distance from corner */
			float m = (float)size / 2;
			data[13*size*size + p] = exp(-0.5 * (j-m)*(j-m) + (k-m)*(k-m));

			/* plane 14: closest color is ours */
			data[14*size*size + p] = (our_dist[p] < opponent_dist[p]);

			/* plane 15: closest color is opponent */
			data[15*size*size + p] = (opponent_dist[p] < our_dist[p]);

			/* planes 16-24: encode rank - set 9th plane for 9d */
			data[24*size*size + p] = 1.0;
			
		}
	}

	caffe_get_data(data, result);
	free(data);
}

void
find_dcnn_best_moves(struct board *b, float *r, coord_t *best, float *best_r)
{
	for (int i = 0; i < DCNN_BEST_N; i++)
		best[i] = pass;

	foreach_free_point(b) {
		int k = (coord_x(c, b) - 1) * 19 + (coord_y(c, b) - 1);
		for (int i = 0; i < DCNN_BEST_N; i++)
			if (r[k] > best_r[i]) {
				for (int j = DCNN_BEST_N - 1; j > i; j--) { // shift
					best_r[j] = best_r[j - 1];
					best[j] = best[j - 1];
				}
				best_r[i] = r[k];
				best[i] = c;
				break;
			}
	} foreach_free_point_end;
}
	
void
print_dcnn_best_moves(struct tree_node *node, struct board *b,
		      coord_t *best, float *best_r)
{
	fprintf(stderr, "%.*sprior_dcnn(%s) = [ ",
		node->depth * 4, "                                   ",
		coord2sstr(node_coord(node), b));
	for (int i = 0; i < DCNN_BEST_N; i++)
		fprintf(stderr, "%s ", coord2sstr(best[i], b));
	fprintf(stderr, "]      ");

	fprintf(stderr, "[ ");
	for (int i = 0; i < DCNN_BEST_N; i++)
		fprintf(stderr, "%.2f ", best_r[i]);
	fprintf(stderr, "]\n");
}
	
static coord_t *
dcnn_genmove(struct engine *e, struct board *b, struct time_info *ti, enum stone color, bool pass_all_alive)
{
	float r[19 * 19];
	float best_r[DCNN_BEST_N] = { 0.0, };
	coord_t best_moves[DCNN_BEST_N];
	dcnn_get_moves(b, color, r);
	find_dcnn_best_moves(b, r, best_moves, best_r);
	
	return coord_copy(best_moves[0]);
}	

struct engine *
engine_dcnn_init(char *arg, struct board *b)
{
	dcnn_init();
	if (!caffe_ready()) {
		fprintf(stderr, "Couldn't initialize dcnn, aborting.\n");
		abort();
	}
	//struct patternplay *pp = patternplay_state_init(arg);
	struct engine *e = (struct engine*)calloc2(1, sizeof(struct engine));
	e->name = (char*)"DCNN Engine";
	e->comment = (char*)"I just select dcnn's best move.";
	e->genmove = dcnn_genmove;
	//e->evaluate = dcnn_evaluate;
	//e->data = pp;

	return e;
}

	


