#define DEBUG
#include "debug.h"
#include <stdio.h>
#include <pthread.h>
#include <assert.h>
#include "random.h"


/* Atom linux x86 benchmarks (gcc 4.8)
                                              blank.t
     tls method               blank.t        2 threads
   ---------------------------------------------------------------------------------------
   __thread                    610               600
   own tls + pthread_self()    580               508
      (without indexing issues    610)
   pthread_set_specific()      532               518

*/


/* Simple Park-Miller for floating point; LCG as used in glibc and other places */


#if 0              // own tls + pthread_self()

#define MAX_THREADS 16
static unsigned long pmseed[MAX_THREADS] = { 29264, 29264, 29264, 29264, 29264, 29264, 29264, 29264,
					     29264, 29264, 29264, 29264, 29264, 29264, 29264, 29264 };

#ifdef _WIN32

/* pthread_self() conveniently returns 0, 1, 2, 3 ... indexes on mingw */
#define get_thread_id()   pthread_self()

#else

static unsigned int threads[MAX_THREADS] = { 0, };
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static int register_thread(unsigned int id);

static int
get_thread_id()
{
	unsigned int id = pthread_self(); 
	for (int i = 0; i < MAX_THREADS; i++)
		if (threads[i] == id)
			return i;
	
	return register_thread(id);  /* Add new thread */
}

static int
register_thread(unsigned int id)
{
	pthread_mutex_lock(&mutex);
	for (int i = 0; i < MAX_THREADS; i++)
		if (!threads[i]) {
			if (DEBUGL(4)) fprintf(stderr, "fast_random(): registering thread %i (%#x)\n", i, id);
			threads[i] = id;
			pthread_mutex_unlock(&mutex);
			return i;
		}
	assert(0);  return 0;
}

/* Call at thread-exit time */
void
fast_random_unregister_thread()
{
	unsigned int id = pthread_self();
	pthread_mutex_lock(&mutex);  
	for (int i = 0; i < MAX_THREADS; i++)
		if (threads[i] == id) {
			if (DEBUGL(4)) fprintf(stderr, "fast_random(): unregistering thread %i (%#x)\n", i, id);
			threads[i] = 0;
			pthread_mutex_unlock(&mutex);
			return;
		}
	assert(0);
}
#endif  // _WIN32

void
fast_srandom(unsigned long seed_)
{
	int id = get_thread_id();
	pmseed[id] = seed_;
}

unsigned long
fast_getseed(void)
{
	int id = get_thread_id();
	return pmseed[id];
}

uint16_t
fast_random(unsigned int max)
{
	int id = get_thread_id();
	pmseed[id] = ((pmseed[id] * 1103515245) + 12345) & 0x7fffffff;
	return ((pmseed[id] & 0xffff) * max) >> 16;
}

#endif


/*************************************************************************************/


#if 0              // TlsSetValue() / TlsGetValue()

static int tls_index = -1;

static void __attribute__((constructor))
init_fast_random()
{
	tls_index = TlsAlloc();
	fast_srandom(29264);
}


void
fast_srandom(unsigned long seed_)
{
	TlsSetValue(tls_index, (void*)seed_);
}

unsigned long
fast_getseed(void)
{
	return (unsigned long)TlsGetValue(tls_index);
}

uint16_t
fast_random(unsigned int max)
{
	unsigned long pmseed = fast_getseed();
	pmseed = ((pmseed * 1103515245) + 12345) & 0x7fffffff;
	fast_srandom(pmseed);
	return ((pmseed & 0xffff) * max) >> 16;
}


#endif


/***************************************************************************************************/


#if 0               // own tls + GetCurrentThreadId()

/* Use custom thread-local storage,
 * mingw-w64's __thread is painfully slow on some platforms. */

#define MAX_THREADS 16
static unsigned long pmseed[MAX_THREADS] = { 29264, 29264, 29264, 29264, 29264, 29264, 29264, 29264,
					     29264, 29264, 29264, 29264, 29264, 29264, 29264, 29264 };

static unsigned int threads[MAX_THREADS] = { 0, };
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static int register_thread(unsigned int id);

static int
get_thread_id()
{
	unsigned int id = GetCurrentThreadId();  /* Faster than pthread_self() */
	for (int i = 0; i < MAX_THREADS; i++)
		if (threads[i] == id)
			return i;
	
	return register_thread(id);  /* Add new thread */
}

static int
register_thread(unsigned int id)
{
	pthread_mutex_lock(&mutex);
	for (int i = 0; i < MAX_THREADS; i++)
		if (!threads[i]) {
			if (DEBUGL(4)) fprintf(stderr, "fast_random(): registering thread %i (%#x)\n", i, id);
			threads[i] = id;
			pthread_mutex_unlock(&mutex);
			return i;
		}
	assert(0);  return 0;
}

/* Called at thread-exit time */
void
fast_random_unregister_thread()
{
	unsigned int id = GetCurrentThreadId();
	pthread_mutex_lock(&mutex);  
	for (int i = 0; i < MAX_THREADS; i++)
		if (threads[i] == id) {
			if (DEBUGL(4)) fprintf(stderr, "fast_random(): unregistering thread %i (%#x)\n", i, id);
			threads[i] = 0;
			pthread_mutex_unlock(&mutex);
			return;
		}
	assert(0);
}


void
fast_srandom(unsigned long seed_)
{
        int id = get_thread_id();
	pmseed[id] = seed_;
}

unsigned long
fast_getseed(void)
{
        int id = get_thread_id();
	return pmseed[id];
}

uint16_t
fast_random(unsigned int max)
{
        int id = get_thread_id();
	pmseed[id] = ((pmseed[id] * 1103515245) + 12345) & 0x7fffffff;
	return ((pmseed[id] & 0xffff) * max) >> 16;
}


#endif


/***************************************************************************************************/


#if 0          // __thread

/* Default, use thread-local storage. */
static __thread unsigned long pmseed = 29264;


void
fast_srandom(unsigned long seed_)
{
	pmseed = seed_;
}

unsigned long
fast_getseed(void)
{
	return pmseed;
}

uint16_t
fast_random(unsigned int max)
{
	pmseed = ((pmseed * 1103515245) + 12345) & 0x7fffffff;
	return ((pmseed & 0xffff) * max) >> 16;
}

float
fast_frandom(void)
{
	/* Construct (1,2) IEEE floating_t from our random integer */
	/* http://rgba.org/articles/sfrand/sfrand.htm */
	union { unsigned long ul; floating_t f; } p;
	p.ul = (((pmseed *= 16807) & 0x007fffff) - 1) | 0x3f800000;
	return p.f - 1.0f;
}

#endif


/***************************************************************************************************/


#if 0             // pthread_set_specific() / pthread_get_specific()

/* Thread local storage not supported through __thread,
 * use pthread_getspecific() instead. */

#include <pthread.h>

static pthread_key_t seed_key;

static void __attribute__((constructor))
random_init(void)
{
	pthread_key_create(&seed_key, NULL);
	fast_srandom(29264UL);
}

void
fast_srandom(unsigned long seed_)
{
	pthread_setspecific(seed_key, (void *)seed_);
}

unsigned long
fast_getseed(void)
{
	return (unsigned long)pthread_getspecific(seed_key);
}

uint16_t
fast_random(unsigned int max)
{
	unsigned long pmseed = (unsigned long)pthread_getspecific(seed_key);
	pmseed = ((pmseed * 1103515245) + 12345) & 0x7fffffff;
	pthread_setspecific(seed_key, (void *)pmseed);
	return ((pmseed & 0xffff) * max) >> 16;
}

float
fast_frandom(void)
{
	/* Construct (1,2) IEEE floating_t from our random integer */
	/* http://rgba.org/articles/sfrand/sfrand.htm */
	unsigned long pmseed = (unsigned long)pthread_getspecific(seed_key);
	pmseed *= 16807;
	union { unsigned long ul; floating_t f; } p;
	p.ul = ((pmseed & 0x007fffff) - 1) | 0x3f800000;
	pthread_setspecific(seed_key, (void *)pmseed);
	return p.f - 1.0f;
}

#endif
