#ifndef PACHI_PATTERNSHM_H
#define PACHI_PATTERNSHM_H

struct pattern_shm {
	char *addr;
	int  size;

	/* allocator stuff */
	char *bottom;
	char *top;

	struct spatial_dict *sdict;
	struct pattern_pdict *pdict;
};


void *pattern_shm_malloc(size_t size);
void *pattern_shm_realloc(void *ptr, size_t size);
void *pattern_shm_calloc(size_t nmemb, size_t size);

int patterns_init_from_shm(struct pattern_setup *pat, char *arg, bool will_append, bool load_prob);
void pattern_shm_ready(struct pattern_setup *pat);

#endif /* PACHI_PATTERNSHM_H */
