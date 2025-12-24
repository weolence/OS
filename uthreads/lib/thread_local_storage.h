#pragma once

#include <pthread.h>

typedef struct tlocal_node {
    pthread_t key;
    void *value;
    struct tlocal_node *next;
} tlocal_node_t;

// container (not owner) of key-value pairs (manual content freeing required)
typedef struct {
    tlocal_node_t **buckets;
    size_t buckets_num;
    pthread_mutex_t lock;
} tlocal_t;

tlocal_t *tlocal_create(size_t buckets_num);
void tlocal_destroy(tlocal_t *tlocal);
void tlocal_set(tlocal_t *tlocal, pthread_t key, void *value);
void *tlocal_get(tlocal_t *tlocal, pthread_t key);
void *tlocal_remove(tlocal_t *tlocal, pthread_t key);
