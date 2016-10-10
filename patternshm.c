#define DEBUG
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "debug.h"
#include "pattern.h"
#include "patternsp.h"
#include "patternprob.h"
#include "patternshm.h"

#define ALIGN_8(p)  ((unsigned long)(p) & 0x7 ? (typeof(p))(((unsigned long)(p) & ~0x7) + 8) : p );

static int use_shm = 0;
static int size = 550 * 1024 * 1024;
static struct pattern_shm *pm = 0;

static void* pattern_shm_malloc(size_t size);
static void* pattern_shm_realloc(void *ptr, size_t size);
static void* pattern_shm_calloc(size_t nmemb, size_t size);

void * (*pattern_realloc)(void *ptr, size_t size) = 0;
void * (*pattern_malloc)(size_t size) = 0;
void * (*pattern_calloc)(size_t nmemb, size_t size) = 0;

void
patterns_use_shm(bool flag)
{
	use_shm = flag;
	pattern_realloc = realloc;
        pattern_malloc = malloc;
	pattern_calloc = calloc;
	
	if (use_shm) {
		pattern_realloc = pattern_shm_realloc;
		pattern_malloc = pattern_shm_malloc;
		pattern_calloc = pattern_shm_calloc;
	}
}

int
patterns_init_from_shm(struct pattern_setup *pat)
{
	assert(use_shm);
	if (!pm) {
		int fd = shm_open("pachi-patterns", O_RDONLY, 0);
		if (fd == -1)
			return 0;
		
		pm = mmap(0, size, PROT_READ, MAP_SHARED, fd, 0);
		assert(pm != MAP_FAILED);
		void *addr = pm->addr;
		int r = munmap(pm, size);  assert(r == 0);
		/* Now we know the address */
		pm = mmap(addr, size, PROT_READ, MAP_SHARED | MAP_FIXED, fd, 0);
		assert(pm != MAP_FAILED);
		//fprintf(stderr, "Patterns shared memory mapped @ %p\n", addr);
		fprintf(stderr, "Found patterns shared memory.\n");
		fprintf(stderr, "Loaded spatial dictionary of %d patterns.\n", pm->sdict->nspatials);
		fprintf(stderr, "Loaded pattern-probability pairs.\n");
	}

	assert(pm->magic == PACHI_SHM_MAGIC);
	assert(pm->version == PACHI_SHM_VERSION);
	assert(pm->ready);
	//fprintf(stderr, "sdict: %p  pdict: %p\n", pm->sdict, pm->pdict);
	pat->pc.spat_dict = pm->sdict;
	pat->pd = pm->pdict;

	return 1;
}


static void
pattern_shm_alloc_init()
{
	if (pm) return;
	
	int fd = shm_open("pachi-patterns", O_RDWR | O_CREAT | O_TRUNC, 0644);
	assert(fd != -1);
	int r = ftruncate(fd, size);  assert(r == 0);
	void *pt = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	assert(pt != MAP_FAILED);
	//fprintf(stderr, "Created patterns shared memory @ %p\n", pt);

	pm = pt;
	pm->addr = pt;
	pm->size = size;
	pm->magic = PACHI_SHM_MAGIC;
	pm->version = PACHI_SHM_VERSION;
	pm->ready = 0;

	pm->bottom = (char*)pt + sizeof(*pm);
	pm->bottom = ALIGN_8(pm->bottom);
	pm->top = pm->bottom;			
}

void
pattern_shm_ready(struct pattern_setup *pat)
{
	//int r = (pm->addr + pm->size) - pm->top;
	//fprintf(stderr, "remaining: %i  (%iMb)\n", r, r / (1024*1024));	

	pm->sdict = pat->pc.spat_dict;
	pm->pdict = pat->pd;
	pm->pc = pat->pc;
	pat->pd->pc = &pm->pc;
	assert(pm->pc.spat_dict == pm->sdict);
	//fprintf(stderr, "sdict: %p  pdict: %p\n", pm->sdict, pm->pdict);
	fprintf(stderr, "Patterns shared memory ready.\n");
	pm->ready = 1;
}


static int alloc_called = 0;

/* Dumb realloc() which doesn't move stuff around.
 * Fine as long as no other alloc calls happen in between. */
static void *
pattern_shm_realloc(void *ptr, size_t size)
{
	if (ptr) assert(!alloc_called);
	if (!ptr)
		ptr = pattern_shm_malloc(size);
	alloc_called = 0;
	pm->top = (char*)ptr + size;
	pm->top = ALIGN_8(pm->top);
	return ptr;
}


static void *
pattern_shm_malloc(size_t size)
{
	pattern_shm_alloc_init();
	alloc_called = 1;
	void *pt = pm->top;
	pm->top += size;
	pm->top = ALIGN_8(pm->top);
	return pt;
}

static void *
pattern_shm_calloc(size_t nmemb, size_t size)
{
	void *pt = pattern_shm_malloc(nmemb * size);
	memset(pt, 0, nmemb * size);
	return pt;
}

