/*
 * The cache is a struct with a pointer to the front and rear of a linked 
 * list of cache objects and a size. When a cache object is read or written,
 * it is moved to the front of the list. When the cache becomes full, it
 * evicts the rear cache object. 
 * Cache object is a struct that contains the request that retrieved it,
 * its size, a buffer that comprises its body, and a pointer
 * to the next cache object in the cache.
 * All operations on the cache and cache objects is thread safe.
 */

#include "csapp.c"
#include <stdio.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

typedef struct {
  char *request;
  void *buf;
  size_t size;
  cache_object *next;
  cache_object *prev;
} cache_object;

typedef struct {
 	size_t size;
  size_t count;
  cache_object *front;
  cache_object *rear;
} cache;

/* init_cache - initializes the cache */
void init_cache();

/* 
 * search_cache - search the cache for a cache object with requestline
 * matching request. Reads and writes from the cache to connfd and
 * returns 0 on hit. Simply returns -1 on miss.
 */
int read_cache(char *request, int connfd);

/* 
 * insert_cache - inserts a new cache object with requestline matching
 * request, content body buf, and content size of size. Return 0 on
 * success, -1 otherwise.
 */
int write_cache(char *request, size_t size, void *buf);