#pragma once

#define _GNU_SOURCE
#include <pthread.h>
#include <stdatomic.h>

typedef enum {
  REQUIRED,
  LOADING,
  DONE,
  ERROR,
} cache_state_t;

typedef struct cache_entry {
  char *key;
  char *data;
  size_t data_size;
  size_t data_capacity;
  cache_state_t state;
  pthread_mutex_t lock;
  pthread_cond_t cond;
  atomic_size_t ref_count;
  struct cache_entry *next;
} cache_entry_t;

typedef struct {
  cache_entry_t **buckets;
  size_t buckets_amount;
  atomic_size_t entry_amount;
  pthread_mutex_t lock;
} cache_t;

// creates initialized cache
cache_t *cache_create(size_t buckets_amount);

// destroys cache with all it's content
void cache_destroy(cache_t *cache);

// takes ownership (reference) to cache entry, without removing it from table.
// if you aquire node, it means that you use it(ref++).
// however if you want to read/write data you need to lock entry(lock only
// during that action!)
cache_entry_t *cache_acquire(cache_t *cache, char *key);

// if you release node, it means that you will not longer use it(ref--).
// in case of ref == 0 that entry will be removed.
// cache_release() must be called only when entry->lock is not captured.
void cache_release(cache_t *cache, cache_entry_t *entry);
