#pragma once

#include <pthread.h>
#include "uthreads.h"

typedef struct queue_node {
    uthread_t *uthread;
    struct queue_node *next;
} queue_node_t;

typedef struct {
    pthread_mutex_t lock;
    queue_node_t *first;
    queue_node_t *last;
} uthreads_queue_t;

uthreads_queue_t *uthreads_queue_create();
void uthreads_queue_destroy(uthreads_queue_t *queue);
void uthreads_queue_add(uthreads_queue_t *queue, uthread_t *uthread);
uthread_t *uthreads_queue_get(uthreads_queue_t *queue);