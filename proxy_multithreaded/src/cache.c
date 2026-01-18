#include "cache.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CACHE_CLEAN_UP_LIMIT 100

/* ===== utility functions ===== */

static size_t hash(const char *key, size_t buckets_amount) {
  unsigned long hash = 5381;
  int c;
  while ((c = *key++)) {
    hash = ((hash << 5) + hash) + (unsigned char)c;
  }
  return (size_t)(hash % buckets_amount);
}

/*
hash-cleaning strategy:
removing entries with 0 references (no one uses them right now) in case amount
of entries in hash table more than CACHE_CLEAN_UP_LIMIT
*/
static void cache_clean_up(cache_t *cache) {
  pthread_mutex_lock(&cache->lock);

  for (size_t i = 0; i < cache->buckets_amount; i++) {
    cache_entry_t *prev = NULL;
    cache_entry_t *curr = cache->buckets[i];
    while (curr) {
      if (atomic_load(&curr->ref_count) != 0) {
        prev = curr;
        curr = curr->next;
        continue;
      }

      cache_entry_t *next = curr->next;

      if (prev) {
        prev->next = next;
      } else {
        cache->buckets[i] = next;
      }

      pthread_mutex_destroy(&curr->lock);
      pthread_cond_destroy(&curr->cond);
      free(curr->data);
      free(curr->key);
      free(curr);

      cache->entry_amount--;

      curr = next;
    }
  }

  pthread_mutex_unlock(&cache->lock);
}

/* ===== end of utility functions ===== */

cache_t *cache_create(size_t buckets_amount) {
  if (!buckets_amount) {
    errno = EINVAL;
    return NULL;
  }

  cache_t *cache = malloc(sizeof(cache_t));
  if (!cache) {
    return NULL;
  }

  cache->buckets = calloc(buckets_amount, sizeof(cache_entry_t *));
  if (!cache->buckets) {
    free(cache);
    return NULL;
  }

  cache->buckets_amount = buckets_amount;
  cache->entry_amount = 0;

  if (pthread_mutex_init(&cache->lock, NULL)) {
    free(cache->buckets);
    free(cache);
    return NULL;
  }

  return cache;
}

void cache_destroy(cache_t *cache) {
  if (!cache) {
    errno = EINVAL;
    return;
  }

  pthread_mutex_lock(&cache->lock);

  for (size_t i = 0; i < cache->buckets_amount; i++) {
    cache_entry_t *curr = cache->buckets[i];
    while (curr) {
      cache_entry_t *next = curr->next;
      free(curr->key);
      free(curr->data);
      pthread_mutex_destroy(&curr->lock);
      pthread_cond_destroy(&curr->cond);
      free(curr);
      curr = next;
    }
  }
  free(cache->buckets);

  pthread_mutex_unlock(&cache->lock);

  pthread_mutex_destroy(&cache->lock);

  free(cache);
}

cache_entry_t *cache_acquire(cache_t *cache, char *key) {
  if (!cache || !key) {
    errno = EINVAL;
    return NULL;
  }

  size_t idx = hash(key, cache->buckets_amount);

  pthread_mutex_lock(&cache->lock);

  cache_entry_t *entry = cache->buckets[idx];
  while (entry) {
    if (strcmp(entry->key, key)) {
      entry = entry->next;
      continue;
    }
    entry->ref_count++;
    pthread_mutex_unlock(&cache->lock);
    return entry;
  }

  // not found — create new entry
  entry = calloc(1, sizeof(cache_entry_t));
  if (!entry) {
    pthread_mutex_unlock(&cache->lock);
    return NULL;
  }

  entry->key = strdup(key);
  if (!entry->key) {
    pthread_mutex_unlock(&cache->lock);
    free(entry);
    return NULL;
  }
  entry->data = NULL;
  entry->data_size = 0;
  entry->data_capacity = 0;
  entry->state = REQUIRED;
  entry->ref_count = 1;
  pthread_mutex_init(&entry->lock, NULL);
  pthread_cond_init(&entry->cond, NULL);

  // insert into hash table
  entry->next = cache->buckets[idx];
  cache->buckets[idx] = entry;

  pthread_mutex_unlock(&cache->lock);

  cache->entry_amount++;

  return entry;
}

void cache_release(cache_t *cache, cache_entry_t *entry) {
  if (!cache || !entry) {
    errno = EINVAL;
    return;
  }

  entry->ref_count--;

  if (atomic_load(&cache->entry_amount) < CACHE_CLEAN_UP_LIMIT) {
    return;
  }

  cache_clean_up(cache);
}
