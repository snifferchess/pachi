#ifndef PACHI_PATTERNSHM_H
#define PACHI_PATTERNSHM_H

#define PACHI_SHM_MAGIC   0x9acceee
#define PACHI_SHM_VERSION 1


struct pattern_shm {
	char *addr;
	int  size;
	int  magic;
	int  version;
	int  ready;

	/* allocator stuff */
	char *bottom;
	char *top;

	struct spatial_dict *sdict;
	struct pattern_pdict *pdict;	
	struct pattern_config pc;	/* copied here since pdict points to it */
};

void patterns_use_shm(bool flag);

/* Pointers to regular malloc/realloc/calloc if not using patterns shm
 * or custom mmap allocator versions if we are. */
void* (*pattern_malloc)(size_t size);
void* (*pattern_realloc)(void *ptr, size_t size);
void* (*pattern_calloc)(size_t nmemb, size_t size);

int patterns_init_from_shm(struct pattern_setup *pat);
void pattern_shm_ready(struct pattern_setup *pat);

#endif /* PACHI_PATTERNSHM_H */
