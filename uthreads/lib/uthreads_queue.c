#include "uthreads_queue.h"

#include <error.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

uthreads_queue_t *uthreads_queue_create() {
    uthreads_queue_t *queue = malloc(sizeof(uthreads_queue_t));
    if (!queue) {
        errno = ENOMEM;
        perror("uthreads_queue_create");
        return NULL;
    }

    pthread_mutex_init(&queue->lock, NULL);
    queue->first = NULL;
    queue->last = NULL;

    return queue;
}

// removes all nodes without removing their content
void uthreads_queue_destroy(uthreads_queue_t *queue) {
    if (!queue) {
        errno = EINVAL;
        perror("uthreads_queue_destroy");
        return;
    }

    pthread_mutex_lock(&queue->lock);

    queue_node_t *curr = queue->first;
    while (curr) {
        queue_node_t *next = curr->next;
        free(curr);
        curr = next;
    }
    queue->first = queue->last = NULL;

    pthread_mutex_unlock(&queue->lock);

    pthread_mutex_destroy(&queue->lock);

    free(queue);
}

// adds one node with uthread content at the end of queue
void uthreads_queue_add(uthreads_queue_t *queue, uthread_t *uthread) {
    if (!queue || !uthread) {
        errno = EINVAL;
        perror("uthreads_queue_add");
        return;
    }

    queue_node_t *node = malloc(sizeof(queue_node_t));
    if (!node) {
        errno = ENOMEM;
        perror("uthreads_queue_add");
        return;
    }

    node->uthread = uthread;
    node->next = NULL;

    pthread_mutex_lock(&queue->lock);

    if (!queue->first) {
        queue->first = queue->last = node;
    } else {
        queue->last->next = node;
        queue->last = node;
    }

    pthread_mutex_unlock(&queue->lock);
}

// removes one node from head of queue & returns it's content
uthread_t *uthreads_queue_get(uthreads_queue_t *queue) {
    if (!queue) {
        errno = EINVAL;
        perror("uthreads_queue_get");
        return NULL;
    }

    pthread_mutex_lock(&queue->lock);

    if (!queue->first) {
        pthread_mutex_unlock(&queue->lock);
        return NULL;
    }

    queue_node_t *node = queue->first;
    uthread_t *uthread = node->uthread;

    if (queue->first == queue->last) {
        queue->first = queue->last = NULL;
    } else {
        queue->first = queue->first->next;
    }

    free(node);

    pthread_mutex_unlock(&queue->lock);

    return uthread;
}