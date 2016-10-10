#ifndef PACHI_PATTERNMMAP_H
#define PACHI_PATTERNMMAP_H

struct pattern_mmap {
	char *mmap;
	int  size;

	/* allocator stuff */
	char *bottom;
	char *top;

	struct spatial_dict *sdict;
	struct pattern_pdict *pdict;
};


void *pattern_mmap_malloc(size_t size);
void *pattern_mmap_realloc(void *ptr, size_t size);
void *pattern_mmap_calloc(size_t nmemb, size_t size);

int patterns_init_from_shm(struct pattern_setup *pat, char *arg, bool will_append, bool load_prob);
void pattern_mmap_ready(struct pattern_setup *pat);

#endif /* PACHI_PATTERNMMAP_H */
