#define DEBUG
#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>


#include "board.h"
#include "debug.h"
#include "pattern.h"
#include "patternsp.h"
#include "patternprob.h"
#include "tactics/ladder.h"
#include "tactics/selfatari.h"
#include "tactics/util.h"
#include "patternmmap.h"

static struct pattern_mmap *pm = 0;

#define ALIGN_8(p)  ((unsigned long)(p) & 0x7 ? (typeof(p))(((unsigned long)(p) & ~0x7) + 8) : p );

static void
pattern_mmap_alloc_init()
{
	if (pm) return;

	int size = 550 * 1024 * 1024;
	//int fd = open("patterns.mmap", O_RDWR | O_CREAT | O_TRUNC, 0644);
	//ftruncate(fd, size);
	//void *pt = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	void *pt = mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	assert(pt != MAP_FAILED);
	fprintf(stderr, "mapped new file\n");

	pm = pt;
	pm->mmap = pt;
	pm->size = size;
	pm->bottom = (char*)pt + sizeof(*pm);
	pm->bottom = ALIGN_8(pm->bottom);
	pm->top = pm->bottom;			
}

void
pattern_mmap_status()
{
	if (!pm) return;
	int r = (pm->mmap + pm->size) - pm->top;
	fprintf(stderr, "remaining: %i  (%iMb)\n", r, r / (1024*1024));
}

static int alloc_called = 0;

void *
pattern_mmap_realloc(void *ptr, size_t size)
{
	if (ptr) assert(!alloc_called);
	if (!ptr)
		ptr = pattern_mmap_malloc(size);
	alloc_called = 0;
	pm->top = (char*)ptr + size;
	pm->top = ALIGN_8(pm->top);
	return ptr;
}


void *
pattern_mmap_malloc(size_t size)
{
	pattern_mmap_alloc_init();
	alloc_called = 1;
	void *pt = pm->top;
	pm->top += size;
	pm->top = ALIGN_8(pm->top);
	return pt;
}

void *
pattern_mmap_calloc(size_t nmemb, size_t size)
{
	void *pt = pattern_mmap_malloc(nmemb * size);
	memset(pt, 0, nmemb * size);
	return pt;
}



#define MIN(a, b) ((char*)(a) < (char*)(b) ? (a) : (b));
#define MAX(a, b) ((char*)(a) > (char*)(b) ? (a) : (b));

//	fprintf(stderr, "adding [%p - %p] \n", (p), (char*)(p) + sizeof(*p)); 
#define ADD_STRUCT(p)  do {  \
	assert(p); \
	map_start = MIN(map_start, p);  \
	map_end = MAX(map_end, (char*)p + sizeof(*p));  \
	} while(0)

		  //	fprintf(stderr, "adding [%p - %p]\n", (p), (char*)(p) + size); 
#define ADD_REGION(p, size)  do {		\
	assert(p);  assert(size != 0);  \
	map_start = MIN(map_start, p);  \
	map_end = MAX(map_end, (char*)p + size);  \
	} while(0)


void
pattern_mmap_dump(struct pattern_setup *pat)
{
	void *map_start = (void*)-1L;
	void *map_end = 0;
	
	struct spatial_dict  *sdict = pat->pc.spat_dict;
	struct pattern_pdict *pdict = pat->pd;


	//ADD_STRUCT(pat);
	//ADD_STRUCT(sdict);   // 256Mb
	//ADD_REGION(sdict->spatials, (sdict->nspatials + 1024) * sizeof(*sdict->spatials));  // 13Mb

	//ADD_STRUCT(pdict);
	//ADD_STRUCT(pdict->pc);
	
	int table_size = pdict->pc->spat_dict->nspatials + 1;
	ADD_REGION(pdict->table, table_size * sizeof(*pdict->table));


#if 1
	// 250Mb
	for (int i = 0; i < table_size; i++) {
		if (!pdict->table[i])
			continue;
		ADD_STRUCT(pdict->table[i]);
	}
#endif

	fprintf(stderr, "map_start: %p   map_end: %p\n", map_start, map_end);
	fprintf(stderr, "map size: %i  (%iMb)\n", 
		(char*)map_end - (char*)map_start,
		((char*)map_end - (char*)map_start) / (1024*1024));

	
}



/* 
   struct pattern_setup

   struct spatial_dict 
     spatials

   struct pattern_pdict
     struct pattern_config   (= pat->pc,  constant)
     table
       struct pattern_prob's   (malloc()'ed  one by one)
*/
