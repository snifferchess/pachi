#ifndef PACHI_PATTERNMMAP_H
#define PACHI_PATTERNMMAP_H

struct pattern_mmap {
	char *mmap;
	int  size;
	char *bottom;
	char *top;
};


void pattern_mmap_dump(struct pattern_setup *pat);


void *pattern_mmap_malloc(size_t size);
void *pattern_mmap_realloc(void *ptr, size_t size);
void *pattern_mmap_calloc(size_t nmemb, size_t size);


#endif /* PACHI_PATTERNMMAP_H */
